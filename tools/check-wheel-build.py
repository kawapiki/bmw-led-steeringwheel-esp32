"""Validate generated artifacts before they can be considered for a flash review."""
from pathlib import Path
import struct
import sys

root = Path(__file__).resolve().parents[1]
project = root / "firmware/wheel"
build = project / "build"
config = (project/"sdkconfig").read_text()
for key in ("CONFIG_SECURE_BOOT", "CONFIG_SECURE_BOOT_V2_ENABLED",
            "CONFIG_SECURE_FLASH_ENC_ENABLED", "CONFIG_BOOTLOADER_APP_ANTI_ROLLBACK"):
    if key+"=y" in config.splitlines():
        sys.exit("Forbidden irreversible security policy enabled: "+key)
if "CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y" not in config.splitlines():
    sys.exit("Missing application rollback support")
raw = (build/"partition_table/partition-table.bin").read_bytes()
parts=[]
for i in range(0,len(raw),32):
    magic, = struct.unpack_from("<H",raw,i)
    if magic != 0x50aa:
        break
    _,kind,subtype,offset,size,name,flags=struct.unpack_from("<HBBII16sI",raw,i)
    parts.append((kind,subtype,offset,size,name.split(b"\0")[0].decode()))
assert parts, "No partition entries"
end=0x11000
for kind,subtype,offset,size,name in parts:
    assert offset>=end and offset+size<=32*1024*1024,(name,"overlap or flash overflow")
    end=offset+size
slots=[p for p in parts if p[0]==0 and p[1] in (0x10,0x11)]
assert len(slots)==2 and {p[1] for p in slots}=={0x10,0x11}
assert all(p[3]==6*1024*1024 for p in slots)
assert any(p[0]==1 and p[1]==0 and p[3]>=8192 for p in parts),"Missing otadata"
app_size=(build/"wheel_demo.bin").stat().st_size
assert 0<app_size<=min(p[3] for p in slots),"Application does not fit both slots"
assert (build/"bootloader/bootloader.bin").stat().st_size<=0x10000
print(f"Artifact checks passed: app={app_size} bytes; 2 x 6 MiB OTA; no irreversible security flags.")
print("This is not hardware verification or flash authorization.")
