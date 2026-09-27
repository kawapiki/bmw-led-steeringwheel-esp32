"""Read-only audit of both generated targets. No serial or flashing commands."""
from pathlib import Path
import hashlib,json,struct
root=Path(__file__).resolve().parents[1]
expected="c8f796755d535f5eb5bdb820e3499595982c86fcfb986f1eb7ec6508f27d3e45"
assert hashlib.sha256((root/"back/steering-wheel-original.bin").read_bytes()).hexdigest()==expected
for target,flash,slot,boot_offset,chip in (("wheel",32<<20,0x600000,0,9),("gateway",4<<20,0x1e0000,0x1000,0)):
    project=root/"firmware"/target;build=project/"build"
    cfg=set((project/"sdkconfig").read_text(encoding="utf-8").splitlines())
    for flag in ("CONFIG_SECURE_BOOT","CONFIG_SECURE_BOOT_V2_ENABLED","CONFIG_SECURE_FLASH_ENC_ENABLED","CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK"):
        assert flag+"=y" not in cfg,flag
    for flag in ("CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE","CONFIG_BT_NIMBLE_NVS_PERSIST","CONFIG_BT_NIMBLE_SM_SC","CONFIG_MBEDTLS_CERTIFICATE_BUNDLE"):
        assert flag+"=y" in cfg,flag
    assert "CONFIG_BT_NIMBLE_SM_LEGACY=y" not in cfg
    assert "CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1" in cfg
    def config_int(name):
        return int(next(line.split("=",1)[1] for line in cfg if line.startswith(name+"=")))
    assert config_int("CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM") >= config_int("CONFIG_ESP_WIFI_RX_BA_WIN"), "Wi-Fi RX buffer/window mismatch"
    parts=[];raw=(build/"partition_table/partition-table.bin").read_bytes()
    for offset in range(0,len(raw),32):
        if struct.unpack_from("<H",raw,offset)[0]!=0x50aa:break
        _,kind,subtype,start,size,name,_=struct.unpack_from("<HBBII16sI",raw,offset)
        parts.append((kind,subtype,start,size,name.rstrip(b"\0").decode()))
    end=0x11000
    for kind,subtype,start,size,name in parts:
        assert start>=end and start+size<=flash,(target,name)
        end=start+size
    apps=[p for p in parts if p[0]==0]
    assert len(apps)==2 and {p[1] for p in apps}=={16,17}
    assert all(p[3]==slot for p in apps)
    assert any(p[4]=="otadata" and p[2]==0x17000 and p[3]==0x2000 for p in parts)
    data=(build/(target+"_demo.bin")).read_bytes()
    assert data[0]==0xe9 and struct.unpack_from("<H",data,12)[0]==chip
    assert struct.unpack_from("<I",data,32)[0]==0xabcd5432
    assert data[80:112].rstrip(b"\0").decode()==target+"_demo"
    marker=('BMWDEMO-ID:'+('wheel_cvs8161' if target=='wheel' else 'lilygo_xy32_v1_1')+':p1:c1:r1:END').encode().ljust(64,b'\0')
    assert data.count(marker)==1,'Wrong immutable build identity'
    assert len(data)<=slot
    boot=(build/"bootloader/bootloader.bin").stat().st_size
    assert boot+boot_offset<=0x10000
    print(f"{target}: app {len(data)} bytes / slot {slot}; boot {boot}; two OTA slots; chip/project/security config passed")
print("Original backup SHA-256 unchanged. Hardware/runtime behavior is NOT validated.")
