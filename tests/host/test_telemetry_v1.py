from pathlib import Path
import os, subprocess, unittest
root=Path(__file__).resolve().parents[2]
out=root/'.test-build';out.mkdir(exist_ok=True)
source=out/'telemetry_v1_test.c'
source.write_text(r'''
#include "telemetry_v1.h"
#include <assert.h>
#include <string.h>
int main(void) {
 telemetry_v1_tracker_t t={0}; telemetry_v1_sample_t s; uint8_t p[16];
 telemetry_v1_encode(p,1,1234,100);
 const uint8_t golden[16]={1,0,0,0,1,0,0,0,0xd2,4,0,0,100,0,0,0};
 assert(!memcmp(p,golden,16));
 assert(telemetry_v1_accept(&t,9,p,16,1000,1010,&s));
 assert(s.rpm==1234 && s.sequence==1 && s.received==1000);
 assert(!telemetry_v1_accept(&t,9,p,16,1100,1110,&s));
 telemetry_v1_encode(p,2,2000,200);
 assert(telemetry_v1_accept(&t,9,p,16,1200,1210,&s));
 telemetry_v1_encode(p,1,1000,100);
 assert(!telemetry_v1_accept(&t,9,p,16,1300,1310,&s));
 assert(telemetry_v1_accept(&t,10,p,16,1400,1410,&s));
 telemetry_v1_encode(p,2,10001,200);
 assert(!telemetry_v1_accept(&t,10,p,16,1500,1510,&s));
 telemetry_v1_encode(p,2,1000,200);
 assert(!telemetry_v1_accept(&t,10,p,15,1500,1510,&s));
 p[0]=2;assert(!telemetry_v1_accept(&t,10,p,16,1500,1510,&s));p[0]=1;
 assert(!telemetry_v1_accept(&t,10,p,16,1500,2001,&s));
 assert(!telemetry_v1_accept(&t,10,p,16,1500,1400,&s));
 assert(telemetry_v1_accept(&t,10,p,16,1500,1600,&s));
 telemetry_v1_encode(p,3,1000,199);
 assert(!telemetry_v1_accept(&t,10,p,16,1600,1610,&s));
 telemetry_v1_encode(p,0xffffffff,1000,0xfffffff0);
 assert(telemetry_v1_accept(&t,11,p,16,1700,1710,&s));
 telemetry_v1_encode(p,0,0,10);
 assert(telemetry_v1_accept(&t,11,p,16,1800,1810,&s));
 assert(s.rpm==0);
 telemetry_v1_encode(p,1,1000,10);
 assert(!telemetry_v1_accept(&t,11,p,16,1900,1910,&s));
 assert(!telemetry_v1_accept(&t,11,NULL,16,1900,1910,&s));
 assert(!telemetry_v1_accept(NULL,11,p,16,1900,1910,&s));
 telemetry_v1_encode(p,1,1000,20);
 assert(!telemetry_v1_accept(&t,11,p,16,0,1910,&s));
 assert(telemetry_v1_accept(&t,11,p,16,1900,2400,&s));
 assert(!telemetry_v1_accept(&t,0,p,16,1800,1810,&s));
 return 0;
}
''',encoding='utf-8')
class TelemetryV1(unittest.TestCase):
 def test_production_codec_tracker(self):
  compiler=os.environ.get('HOST_CC',str(out/'host-clean/deps/ziglang/zig.exe'))
  binary=out/'telemetry_v1_test.exe'
  subprocess.run([compiler,'cc','-std=c11','-Wall','-Wextra','-Werror','-I',str(root/'components/ble_link/include'),str(source),str(root/'components/ble_link/telemetry_v1.c'),'-o',str(binary)],check=True)
  subprocess.run([str(binary)],check=True)
if __name__=='__main__':unittest.main()
