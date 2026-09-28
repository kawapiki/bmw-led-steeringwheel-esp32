"""Compile actual LVGL9.4 + production ui_view and render native-size snapshots.
Host framebuffer and 32KiB LVGL pool are test-only; firmware buffers are unchanged.
Requires local Zig0.14.1 and Pillow. No ESP board or network is accessed.
"""
from pathlib import Path
import subprocess
from PIL import Image
root=Path(__file__).resolve().parents[2]
out=root/'.test-build/ui-render';out.mkdir(parents=True,exist_ok=True)
lv=root/'firmware/wheel/managed_components/lvgl__lvgl';ui=root/'firmware/wheel/components/wheel_ui'
args=['-O1','-DLV_CONF_SKIP','-DLV_KCONFIG_IGNORE','-DLV_COLOR_DEPTH=16','-DLV_USE_QRCODE=1','-DLV_FONT_MONTSERRAT_40=1','-DLV_FONT_MONTSERRAT_28=1','-DLV_FONT_MONTSERRAT_20=1','-DLV_MEM_SIZE=32768','-I'+str(lv),'-I'+str(ui),str(root/'tests/host/ui_render.c'),str(ui/'ui_view.c'),str(ui/'ui_graphics.c'),str(ui/'ui_model.c')]
asset_bin=ui/'ui_vehicle_frames.bin'
if not asset_bin.exists(): raise RuntimeError('Generate the actual E90 assets before running the renderer')
assembly=out/'ui_vehicle_frames.S'
assembly.write_text('.section .rdata,"dr"\n.balign 4\n.globl _binary_ui_vehicle_frames_bin_start\n_binary_ui_vehicle_frames_bin_start:\n.incbin "'+asset_bin.as_posix()+'"\n',encoding='utf-8')
args += [str(ui/'ui_vehicle_assets.c'),str(ui/'ui_vehicle_scene.c'),str(assembly)]
args += [str(p) for p in (lv/'src').rglob('*.c')]
args += ['-o',str(out/'ui_render.exe')]
rsp=out/'build.rsp';rsp.write_text('\n'.join('"'+a.replace('\\','/')+'"' for a in args),encoding='utf-8')
subprocess.run([str(root/'.test-build/host-clean/deps/ziglang/zig.exe'),'cc','@'+str(rsp)],check=True)
subprocess.run([str(out/'ui_render.exe'),str(out)],check=True)
for p in out.glob('*.ppm'): Image.open(p).save(p.with_suffix('.png'))
print('Actual LVGL native-size captures:',out)

