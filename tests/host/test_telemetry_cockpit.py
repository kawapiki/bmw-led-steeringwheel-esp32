from pathlib import Path
import os,subprocess,unittest
root=Path(__file__).resolve().parents[2];out=root/'.test-build';out.mkdir(exist_ok=True)
source=out/'cockpit_codec_test.c'
source.write_text(r'''
#include "telemetry_cockpit.h"
#include <assert.h>
#include <string.h>
int main(void){
 telemetry_v1_tracker_t t={0};cockpit_sample_t s={.source=1,.valid=0x7ff,.rpm=2500,.speed_dkph=1234,.gear=0x45,.closure_open=1,.coolant_c=95,.oil_c=105},o;
 uint8_t p[22];cockpit_encode(p,1,100,&s);
 const uint8_t gold[22]={1,1,0xff,7,1,0,0,0,100,0,0,0,0xc4,9,0xd2,4,0x45,135,145,1,0,0};
 assert(!memcmp(p,gold,22));assert(cockpit_accept(&t,1,p,22,1000,1010,&o));
 assert(o.rpm==2500&&o.speed_dkph==1234&&o.gear==0x45&&o.coolant_c==95&&o.oil_c==105&&o.closure_open==1);
 assert(!cockpit_accept(&t,1,p,22,1100,1110,&o));
 assert(!cockpit_accept(&t,2,p,21,1100,1110,&o));
 p[1]=3;assert(!cockpit_accept(&t,2,p,22,1100,1110,&o));p[1]=1;
 p[1]=2;assert(cockpit_accept(&t,8,p,22,1100,1110,&o));assert(o.source==2);p[1]=1;
 s.source=2;cockpit_encode(p,1,100,&s);assert(p[1]==2);assert(cockpit_accept(&t,9,p,22,1100,1110,&o));assert(o.source==2);s.source=1;cockpit_encode(p,1,100,&s);
 p[20]=1;assert(!cockpit_accept(&t,2,p,22,1100,1110,&o));p[20]=0;
 p[16]=0x11;assert(!cockpit_accept(&t,2,p,22,1100,1110,&o));p[16]=0x40;
 assert(cockpit_accept(&t,2,p,22,1100,1110,&o));assert(o.gear==0x40);
 p[2]=0;p[3]=0;p[12]=255;p[13]=255;p[14]=255;p[15]=255;p[16]=255;p[17]=255;p[18]=255;
 assert(cockpit_accept(&t,3,p,22,1200,1210,&o));assert(o.valid==0&&o.rpm==0&&o.speed_dkph==0&&o.gear==0);
 p[2]=1;assert(!cockpit_accept(&t,4,p,22,1200,1210,&o));
 // Each valid field enforces its own domain; absent fields remain unknown.
 s.valid=0x7ff;cockpit_encode(p,2,200,&s);
 const unsigned offsets[]={12,14,17,18,19,20,21};
 const uint8_t bad[]={255,255,191,221,64,1,1};
 for(unsigned i=0;i<7;i++){cockpit_encode(p,2,200,&s);p[offsets[i]]=bad[i];if(offsets[i]==12||offsets[i]==14)p[offsets[i]+1]=255;assert(!cockpit_accept(&t,100+i,p,22,1300,1310,&o));}
 for(unsigned selector=0;selector<8;selector++)for(unsigned gear=0;gear<10;gear++){
  cockpit_encode(p,2,200,&s);p[16]=(selector<<4)|gear;
  bool allowed=selector>=1&&selector<=6&&gear<=8&&(selector>3||gear==0);
  assert(cockpit_accept(&t,200+selector*10+gear,p,22,1400,1410,&o)==allowed);
 }
 cockpit_encode(p,3,300,&s);p[3]|=0x80;assert(!cockpit_accept(&t,500,p,22,1500,1510,&o));
 uint8_t seen=0;for(uint32_t ms=0;ms<60000;ms+=100){cockpit_demo(ms,&s);assert(s.valid==0x7ff&&s.source==1);if(ms<6000)assert(!s.closure_open&&!s.speed_dkph);if(ms>=18000&&ms<21000)assert(s.closure_open==0x19);if(ms>=21000&&ms<24000)assert(!s.closure_open);if(s.closure_open){assert(!s.speed_dkph&&s.gear==0x10);seen|=s.closure_open;}cockpit_encode(p,ms+1,ms,&s);assert(cockpit_accept(&t,5,p,22,ms+1,ms+2,&o));}assert(seen==0x3f);
 return 0;
}
''',encoding='utf-8')
class CockpitCodec(unittest.TestCase):
 def test_production_codec_and_demo(self):
  cc=os.environ.get('HOST_CC',str(out/'host-clean/deps/ziglang/zig.exe'));binary=out/'cockpit_codec_test.exe'
  subprocess.run([cc,'cc','-std=c11','-Wall','-Wextra','-Werror','-I',str(root/'components/ble_link/include'),'-I',str(root/'components/contracts/include'),str(source),str(root/'components/ble_link/telemetry_v1.c'),str(root/'components/ble_link/telemetry_cockpit.c'),'-o',str(binary)],check=True)
  subprocess.run([str(binary)],check=True)
if __name__=='__main__':unittest.main()
