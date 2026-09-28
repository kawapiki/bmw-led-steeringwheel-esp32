from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[2]
out=root/'.test-build'; ui=root/'firmware/wheel/components/wheel_io'
src=out/'rpm_led_test.c'
src.write_text(r'''
#include "rpm_led.h"
#include <assert.h>
#include <math.h>
static unsigned lit(rpm_led_color_t *p) { unsigned n=0; for(unsigned i=0;i<23;i++) n+=!!(p[i].r||p[i].g||p[i].b); return n; }
int main(void) {
 rpm_led_state_t s={0}; rpm_led_color_t p[23];
 rpm_led_frame(&s,0,true,0,p); assert(lit(p)==1 && p[0].r>0 && p[0].g>0);
 rpm_led_frame(&s,1200,true,20,p); assert(lit(p)==5); for(int i=0;i<5;i++) assert(p[i].r>p[i].g);
 rpm_led_frame(&s,1201,true,40,p); assert(p[4].r==0 && p[4].g>0);
 rpm_led_frame(&s,6000,true,60,p); assert(lit(p)==22 && p[18].g>p[5].g);
 rpm_led_frame(&s,6500,true,125,p); assert(lit(p)==23);
 for(int i=20;i<23;i++) { assert(p[i].r>p[i-1].r); assert(p[i].g<p[i-1].g); }
 rpm_led_frame(&s,6500,true,250,p); assert(lit(p)==0);
 rpm_led_frame(&s,3000,true,270,p); assert(lit(p)==12 && p[22].r>0);
 float a=s.peak_rpm;
 rpm_led_frame(&s,3000,true,370,p); float b=s.peak_rpm;
 rpm_led_frame(&s,3000,true,470,p); float c=s.peak_rpm;
 assert(a>b && b>c && (b-c)>(a-b));
 rpm_led_frame(&s,3000,true,1500,p); assert(s.peak_rpm==3000 && lit(p)==11);
 rpm_led_frame(&s,6000,true,1520,p); assert(s.peak_rpm==6000 && s.fall_velocity==0);
 rpm_led_frame(&s,0,false,1540,p); assert(lit(p)==0 && !s.initialized);
 rpm_led_frame(&s,800,true,1560,p); assert(lit(p)==3 && s.peak_rpm==800);
 /* Equal elapsed time at different cadences gives equal peak positions. */
 rpm_led_state_t x={0},y={0};
 rpm_led_frame(&x,6000,true,0,p); rpm_led_frame(&y,6000,true,0,p);
 for(unsigned t=20;t<=400;t+=20) rpm_led_frame(&x,2000,true,t,p);
 rpm_led_frame(&y,2000,true,400,p); assert(fabsf(x.peak_rpm-y.peak_rpm)<0.05f);
 rpm_led_frame(&x,6000,true,UINT32_MAX-9u,p); rpm_led_frame(&x,2000,true,10,p);
 assert(x.peak_rpm>5990 && x.peak_rpm<6000);
 rpm_led_frame(&x,UINT32_MAX,true,125,p); assert(lit(p)==23 && x.peak_rpm==6500);
 /* Exhaustive clamped scale: fresh frame has the proportional number of pixels. */
 for(unsigned r=0;r<=10000;r++) {
   s=(rpm_led_state_t){0}; rpm_led_frame(&s,r,true,125,p);
   unsigned v=r>6500?6500:r, expected=v?(v*23+6499)/6500:1;
   assert(lit(p)==expected);
 }
}
''',encoding='utf-8')
subprocess.run([str(out/'host-clean/deps/ziglang/zig.exe'),'cc','-std=c11','-Wall','-Wextra','-Werror','-I'+str(ui),str(src),str(ui/'rpm_led.c'),'-o',str(out/'rpm_led_test.exe')],check=True)
subprocess.run([str(out/'rpm_led_test.exe')],check=True)
print('RPM scaling, colors, peak acceleration, freshness reset and timing tests passed')
