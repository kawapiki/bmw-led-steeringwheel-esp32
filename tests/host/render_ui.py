"""Compile actual LVGL9.4 + production ui_view and render native-size snapshots.
Host framebuffer and 64KiB LVGL pool are test-only; firmware buffers are unchanged.
Requires local Zig0.14.1 and Pillow. No ESP board or network is accessed.
"""
from pathlib import Path
import subprocess
from PIL import Image
root=Path(__file__).resolve().parents[2]
out=root/'.test-build/ui-render';out.mkdir(parents=True,exist_ok=True)
lv=root/'firmware/wheel/managed_components/lvgl__lvgl';ui=root/'firmware/wheel/components/wheel_ui'
args=['-O1','-DLV_CONF_SKIP','-DLV_KCONFIG_IGNORE','-DLV_COLOR_DEPTH=16','-DLV_USE_QRCODE=1','-DLV_FONT_MONTSERRAT_40=1','-DLV_MEM_SIZE=65536','-I'+str(lv),'-I'+str(ui),str(root/'tests/host/ui_render.c'),str(ui/'ui_view.c')]
args += [str(p) for p in (lv/'src').rglob('*.c')]
args += ['-o',str(out/'ui_render.exe')]
rsp=out/'build.rsp';rsp.write_text('\n'.join('"'+a.replace('\\','/')+'"' for a in args),encoding='utf-8')
subprocess.run([str(root/'.test-build/host-clean/deps/ziglang/zig.exe'),'cc','@'+str(rsp)],check=True)
subprocess.run([str(out/'ui_render.exe'),str(out)],check=True)
for p in out.glob('*.ppm'): Image.open(p).save(p.with_suffix('.png'))
print('Actual LVGL native-size captures:',out)

