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
int main(void) {
 ui_nav_t n={0};
 ui_nav_key(&n,UI_NEXT);assert(n.screen==UI_SHIFT);
 ui_nav_key(&n,UI_NEXT);assert(n.screen==UI_GATEWAY);
 ui_nav_key(&n,UI_NEXT);assert(n.screen==UI_SERVICE);
 ui_nav_key(&n,UI_SELECT);assert(n.screen==UI_MENU);
 ui_nav_key(&n,UI_SELECT);assert(n.screen==UI_DETAIL&&n.item==0);
 ui_nav_key(&n,UI_BACK);assert(n.screen==UI_MENU);
 for(int i=0;i<900;i++)ui_nav_key(&n,UI_NEXT);
 assert(n.item<UI_SERVICE_COUNT);
 ui_nav_key(&n,UI_BACK);assert(n.screen==UI_SERVICE);
 ui_nav_key(&n,UI_BACK);assert(n.screen==UI_ENGINE);
 ui_telemetry_t t={.secure=true,.compatible=true,.valid=true,.demo=true,.fresh=true,.rpm=4321};
 ui_reading_t r=ui_reading(&t);assert(r.available&&r.rpm==4321&&r.status==UI_DATA_DEMO);
 t.secure=false;r=ui_reading(&t);assert(!r.available&&r.rpm==0&&r.status==UI_DATA_OFFLINE);
 t.secure=true;t.compatible=false;r=ui_reading(&t);assert(!r.available&&r.status==UI_DATA_INCOMPATIBLE);
 t.compatible=true;t.fresh=false;r=ui_reading(&t);assert(!r.available&&r.status==UI_DATA_STALE);
 t.fresh=true;t.valid=false;r=ui_reading(&t);assert(!r.available&&r.status==UI_DATA_INVALID);


 t.valid=true;t.demo=false;t.rpm=0;r=ui_reading(&t);assert(r.available&&r.rpm==0&&r.status==UI_DATA_LIVE);
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
 return 0;
}
''',encoding='utf-8')
subprocess.run([str(out/'host-clean/deps/ziglang/zig.exe'),'cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ui),str(source),str(ui/'ui_model.c'),'-o',str(out/'ui_model_test.exe')],check=True)
subprocess.run([str(out/'ui_model_test.exe')],check=True)
print('UI navigation and remote-data policy assertions passed')


