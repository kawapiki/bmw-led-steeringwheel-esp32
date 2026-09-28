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
 assert(ui_vehicle_scene_step(&s,1000,false)&&s.progress==0);
 assert(ui_vehicle_scene_step(&s,1001,false)&&s.progress>0&&s.progress<0.001f);
 assert(ui_vehicle_scene_step(&s,3500,false)&&s.progress==0.5f);
 assert(ui_vehicle_scene_step(&s,5999,false)&&s.progress>0.999f&&s.progress<1);
 assert(!ui_vehicle_scene_step(&s,6000,false)&&s.finished);
 assert(!ui_vehicle_scene_step(&s,7000,false));
 s=(ui_vehicle_scene_t){0};assert(!ui_vehicle_scene_step(&s,0,true));assert(!ui_vehicle_scene_step(&s,1,false));
 s=(ui_vehicle_scene_t){0};assert(ui_vehicle_scene_step(&s,0,false));assert(ui_vehicle_scene_interrupt(&s));
 assert(!ui_vehicle_scene_interrupt(&s));assert(!ui_vehicle_scene_step(&s,1,false));
 s=(ui_vehicle_scene_t){0};assert(ui_vehicle_scene_step(&s,0,false));assert(!ui_vehicle_scene_step(&s,101,true));
 assert(!ui_vehicle_scene_step(&s,102,false)); /* reconnect/service exit does not replay */
 s=(ui_vehicle_scene_t){0};assert(ui_vehicle_scene_step(&s,500,false));
 assert(!ui_vehicle_scene_step(&s,400,false)); /* clock reversal cannot underflow index */
 ui_vehicle_closing_t c={0};
 assert(!ui_vehicle_closing_step(&c,100,true,true,1,63));
 assert(ui_vehicle_closing_step(&c,200,true,false,0,63));
 assert(ui_vehicle_closing_step(&c,1199,true,false,0,63));
 assert(!ui_vehicle_closing_step(&c,1200,true,false,0,63));
 assert(!ui_vehicle_closing_step(&c,1300,true,true,1,63));
 assert(!ui_vehicle_closing_step(&c,1400,true,false,1,63)); /* acknowledgement is not closing */
 assert(!ui_vehicle_closing_step(&c,1500,true,false,0,63));
 assert(!ui_vehicle_closing_step(&c,1600,true,true,1,63));
 assert(!ui_vehicle_closing_step(&c,1700,true,false,0,0)); /* unknown is not closed */
 assert(!ui_vehicle_closing_step(&c,1800,true,true,1,63));
 assert(ui_vehicle_closing_step(&c,1900,true,false,0,63));
 assert(!ui_vehicle_closing_step(&c,1950,false,false,0,63)); /* service cancels */
 assert(!ui_vehicle_closing_step(&c,2000,true,false,0,63));
 return 0;
}
''',encoding='utf-8')
subprocess.run([str(out/'host-clean/deps/ziglang/zig.exe'),'cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ui),str(source),str(ui/'ui_vehicle_scene.c'),'-o',str(out/'ui_vehicle_scene_test.exe')],check=True)
subprocess.run([str(out/'ui_vehicle_scene_test.exe')],check=True)
print('Bounded vehicle intro lifecycle tests passed')
