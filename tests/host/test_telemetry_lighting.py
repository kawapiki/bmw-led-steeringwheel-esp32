from pathlib import Path
import os,subprocess,unittest
root=Path(__file__).resolve().parents[2];out=root/'.test-build';out.mkdir(exist_ok=True)
source=out/'lighting_codec_test.c'
source.write_text(r'''
#include "telemetry_lighting.h"
#include <assert.h>
#include <string.h>
int main(void){
 telemetry_v1_tracker_t t={0};lighting_sample_t s={.source=1,.valid=63,.on=9},o;
 uint8_t p[16];lighting_encode(p,1,100,&s);
 const uint8_t gold[16]={1,1,63,9,1,0,0,0,100,0,0,0,0,0,0,0};
 assert(!memcmp(p,gold,16));assert(lighting_accept(&t,1,p,16,1000,1010,&o));
 assert(o.source==1&&o.valid==63&&o.on==9&&o.received==1000);
 assert(!lighting_accept(&t,1,p,16,1100,1110,&o));
 assert(!lighting_accept(&t,2,p,15,1100,1110,&o));
 assert(!lighting_accept(&t,2,p,16,1100,1601,&o));
 for(unsigned i=0;i<16;i++){lighting_encode(p,2,200,&s);if(i==0)p[i]=2;else if(i==1)p[i]=3;else if(i==2||i==3)p[i]=64;else if(i>=12)p[i]=1;else continue;assert(!lighting_accept(&t,2,p,16,1200,1210,&o));}
 s.source=2;s.valid=1;s.on=63;lighting_encode(p,1,100,&s);
 assert(lighting_accept(&t,3,p,16,1300,1310,&o));assert(o.source==2&&o.valid==1&&o.on==1);
 s.valid=0;lighting_encode(p,2,200,&s);assert(lighting_accept(&t,3,p,16,1400,1410,&o));assert(!o.on&&!o.valid);
 uint8_t seen=0;bool left_on=false,left_off=false;
 for(uint32_t ms=0;ms<60000;ms+=100){lighting_demo(ms,&s);assert(s.source==1&&s.valid==63);seen|=s.on;if(ms>=18000&&ms<24000){left_on|=!!(s.on&LIGHT_LEFT_INDICATOR);left_off|=!(s.on&LIGHT_LEFT_INDICATOR);}lighting_encode(p,ms+1,ms,&s);assert(lighting_accept(&t,4,p,16,ms+1,ms+2,&o));}
 assert(seen==63&&left_on&&left_off);
 return 0;
}
''',encoding='utf-8')
class LightingCodec(unittest.TestCase):
 def test_production_codec_and_gateway_scenario(self):
  cc=os.environ.get('HOST_CC',str(out/'host-clean/deps/ziglang/zig.exe'));binary=out/'lighting_codec_test.exe'
  subprocess.run([cc,'cc','-std=c11','-Wall','-Wextra','-Werror','-I',str(root/'components/ble_link/include'),'-I',str(root/'components/contracts/include'),str(source),str(root/'components/ble_link/telemetry_v1.c'),str(root/'components/ble_link/telemetry_lighting.c'),'-o',str(binary)],check=True)
  subprocess.run([str(binary)],check=True)
if __name__=='__main__':unittest.main()
