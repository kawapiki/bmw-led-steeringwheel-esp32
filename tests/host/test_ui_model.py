"""Native UI policy tests: navigation and authenticated/fresh remote display gates."""
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
out=root/'.test-build';out.mkdir(exist_ok=True)
ui=root/'firmware/wheel/components/wheel_ui'
source=out/'ui_model_test.c'
source.write_text(r'''
#include "ui_model.h"
#include <assert.h>
#include <string.h>
int main(void) {
 ui_nav_t n={0};
 ui_nav_key(&n,UI_NEXT);assert(n.screen==UI_SHIFT);
 ui_nav_key(&n,UI_NEXT);assert(n.screen==UI_VEHICLE);
 ui_nav_key(&n,UI_NEXT);assert(n.screen==UI_GATEWAY);
 ui_nav_key(&n,UI_NEXT);assert(n.screen==UI_SERVICE);
 ui_nav_key(&n,UI_SELECT);assert(n.screen==UI_MENU);
 ui_nav_key(&n,UI_SELECT);assert(n.screen==UI_DETAIL&&n.item==0);
 ui_nav_key(&n,UI_BACK);assert(n.screen==UI_MENU);
 for(int i=0;i<900;i++)ui_nav_key(&n,UI_NEXT);
 assert(n.item<UI_SERVICE_COUNT);
 ui_nav_key(&n,UI_BACK);assert(n.screen==UI_SERVICE);
 ui_nav_key(&n,UI_BACK);assert(n.screen==UI_ENGINE);
 ui_telemetry_t t={.secure=true,.compatible=true,.valid=true,.demo=true,.fresh=true,.rpm=4321,.valid_fields=UI_VALID_RPM};
 ui_reading_t r=ui_reading(&t);assert(r.available&&r.rpm==4321&&r.status==UI_DATA_DEMO);
 t.secure=false;r=ui_reading(&t);assert(!r.available&&r.rpm==0&&r.status==UI_DATA_OFFLINE);
 t.secure=true;t.compatible=false;r=ui_reading(&t);assert(!r.available&&r.status==UI_DATA_INCOMPATIBLE);
 t.compatible=true;t.fresh=false;r=ui_reading(&t);assert(!r.available&&r.status==UI_DATA_STALE);
 t.fresh=true;t.valid=false;r=ui_reading(&t);assert(!r.available&&r.status==UI_DATA_INVALID);


 t.valid=true;t.demo=false;t.rpm=0;r=ui_reading(&t);assert(r.available&&r.rpm==0&&r.status==UI_DATA_LIVE);
 t.lights_fresh=true;t.lights_valid=0x25;t.lights_on=0xff;
 r=ui_reading(&t);assert(r.lights_valid==0x25&&r.lights_on==0x25);
 t.fresh=false;r=ui_reading(&t);assert(r.lights_valid==0x25); /* independent packet age */
 t.lights_fresh=false;r=ui_reading(&t);assert(!r.lights_valid&&!r.lights_on);
 t.lights_fresh=true;t.secure=false;r=ui_reading(&t);assert(!r.lights_valid);
 t.secure=true;t.compatible=false;r=ui_reading(&t);assert(!r.lights_valid);
 t.compatible=true;t.fresh=true;t.lights_on=0;
 for(unsigned bit=0;bit<6;++bit){t.lights_valid=1u<<bit;r=ui_reading(&t);assert(r.lights_valid==(1u<<bit)&&!r.lights_on);}
 ui_confirmation_t c={0};ui_offer_t a={.generation=1,.release=5},b={.generation=2,.release=6},out={0};
 a.digest[0]=4;b.digest[0]=5;
 ui_confirmation_show(&c,&a,100);ui_confirmation_release(&c,true,120);
 assert(!ui_confirmation_take(&c,&a,&out));
 ui_confirmation_release(&c,false,200);assert(!ui_confirmation_take(&c,&a,&out));
 ui_confirmation_release(&c,true,200);assert(!ui_confirmation_take(&c,&b,&out));
 ui_confirmation_release(&c,true,201);assert(ui_confirmation_take(&c,&a,&out));
 assert(out.release==5&&out.generation==1&&out.digest[0]==4);
 assert(!ui_confirmation_take(&c,&a,&out));
 ui_confirmation_show(&c,&b,300);ui_confirmation_release(&c,true,400);
 ui_confirmation_show(&c,0,401);assert(!ui_confirmation_take(&c,&b,&out));

 t.valid_fields=UI_VALID_SPEED|UI_VALID_GEAR|UI_VALID_COOLANT;
 t.speed_dkph=0;t.gear=0x45;t.coolant_c=-12;t.oil_c=105;
 r=ui_reading(&t);assert(!r.available&&r.valid_fields==14&&r.speed_dkph==0&&r.coolant_c==-12);
 assert(!(r.valid_fields&UI_VALID_OIL));
 t.fresh=false;r=ui_reading(&t);assert(r.valid_fields==0&&r.closure_known==0);
 char gear[4];ui_gear_text(gear,0x45,true);assert(!strcmp(gear,"D5"));
 ui_gear_text(gear,0x10,true);assert(!strcmp(gear,"P"));
 ui_gear_text(gear,0x20,true);assert(!strcmp(gear,"R"));
 ui_gear_text(gear,0x30,true);assert(!strcmp(gear,"N"));
 ui_gear_text(gear,0x60,true);assert(!strcmp(gear,"M"));
 ui_gear_text(gear,0xFF,true);assert(!strcmp(gear,"--"));
 ui_gear_text(gear,0x45,false);assert(!strcmp(gear,"--"));
 ui_door_state_t d={0};r=(ui_reading_t){.closure_known=63};
 ui_door_update(&d,&r,UI_ENGINE,false);assert(!d.active&&!d.visible);
 for(unsigned bit=0;bit<6;bit++){
  r.closure_open=1u<<bit;ui_door_update(&d,&r,UI_ENGINE,false);
  assert(d.visible&&d.open==(1u<<bit));
  assert(ui_door_acknowledge(&d));assert(!d.visible&&d.acknowledged);
  ui_door_update(&d,&r,UI_ENGINE,false);assert(!d.visible);
  r.closure_open=0;ui_door_update(&d,&r,UI_ENGINE,false);assert(!d.active);
 }
 r.closure_open=5;ui_door_update(&d,&r,UI_SHIFT,false);assert(d.visible&&d.open==5);
 ui_door_update(&d,&r,UI_SHIFT,true);assert(!d.visible&&d.active);
 r.closure_known=0;ui_door_update(&d,&r,UI_SHIFT,false);assert(d.visible&&d.open==5&&d.known==0);
 ui_door_update(&d,&r,UI_DETAIL,false);assert(!d.visible&&d.active);
 ui_door_update(&d,&r,UI_MENU,false);assert(!d.visible);
 ui_door_update(&d,&r,UI_GATEWAY,false);assert(!d.visible);
 ui_door_update(&d,&r,UI_SHIFT,false);assert(d.visible); /* underlying view is unchanged */
 ui_door_acknowledge(&d);r.closure_known=63;r.closure_open=7;
 ui_door_update(&d,&r,UI_SHIFT,false);assert(d.visible&&d.open==7); /* new door reopens */
 ui_door_acknowledge(&d);r.closure_open=6;ui_door_update(&d,&r,UI_SHIFT,false);assert(!d.visible&&d.acknowledged);
 r.closure_known=1;r.closure_open=0;ui_door_update(&d,&r,UI_SHIFT,false);
 assert(d.active&&d.open==6&&d.known==1); /* unknown is never closed */
 r.closure_known=63;ui_door_update(&d,&r,UI_SHIFT,false);assert(!d.active&&!d.visible);
 return 0;

}
''',encoding='utf-8')
subprocess.run([str(out/'host-clean/deps/ziglang/zig.exe'),'cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ui),str(source),str(ui/'ui_model.c'),'-o',str(out/'ui_model_test.exe')],check=True)
subprocess.run([str(out/'ui_model_test.exe')],check=True)
print('UI navigation and remote-data policy assertions passed')


