"""Native bounded intro state-machine tests; no LVGL or physical device."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2];out=root/'.test-build';ui=root/'firmware/wheel/components/wheel_ui'
source=out/'ui_vehicle_scene_test.c'
source.write_text(r'''
#include "ui_vehicle_scene.h"
#include <assert.h>
int main(void){
 ui_vehicle_scene_t s={0};
 assert(ui_vehicle_scene_step(&s,1000,false)&&s.frame==0);
 assert(ui_vehicle_scene_step(&s,1049,false)&&s.frame==0);
 assert(ui_vehicle_scene_step(&s,1050,false)&&s.frame==1);
 assert(ui_vehicle_scene_step(&s,2299,false)&&s.frame==25);
 assert(ui_vehicle_scene_step(&s,2399,false)&&s.frame==27);
 assert(!ui_vehicle_scene_step(&s,2400,false)&&s.finished);
 assert(!ui_vehicle_scene_step(&s,5000,false));
 s=(ui_vehicle_scene_t){0};assert(!ui_vehicle_scene_step(&s,0,true));assert(!ui_vehicle_scene_step(&s,1,false));
 s=(ui_vehicle_scene_t){0};assert(ui_vehicle_scene_step(&s,0,false));assert(ui_vehicle_scene_interrupt(&s));
 assert(!ui_vehicle_scene_interrupt(&s));assert(!ui_vehicle_scene_step(&s,1,false));
 s=(ui_vehicle_scene_t){0};assert(ui_vehicle_scene_step(&s,0,false));assert(!ui_vehicle_scene_step(&s,101,true));
 assert(!ui_vehicle_scene_step(&s,102,false)); /* reconnect/service exit does not replay */
 s=(ui_vehicle_scene_t){0};assert(ui_vehicle_scene_step(&s,500,false));
 assert(!ui_vehicle_scene_step(&s,400,false)); /* clock reversal cannot underflow index */
 return 0;
}
''',encoding='utf-8')
subprocess.run([str(out/'host-clean/deps/ziglang/zig.exe'),'cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ui),str(source),str(ui/'ui_vehicle_scene.c'),'-o',str(out/'ui_vehicle_scene_test.exe')],check=True)
subprocess.run([str(out/'ui_vehicle_scene_test.exe')],check=True)
print('Bounded vehicle intro lifecycle tests passed')
