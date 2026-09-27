"""Native regression for the production OTA read/write decision block."""
from pathlib import Path
import ctypes as C, os, subprocess, unittest
R=Path(__file__).resolve().parents[2];O=R/'.test-build'
s=(R/'components/update_manager/update_manager.c').read_text(encoding='utf-8')
a=s.index('    int n = esp_http_client_read(http, (char *)data, sizeof(data));',s.index('static bool install('))
b=s.index('    unsigned percent = update_progress_percent',a)
body=s[a:b]
prefix=r'''
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#define ESP_ERR_HTTP_EAGAIN 0x7007
#define ESP_OK 0
#define PSA_SUCCESS 0
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
static uint64_t now;static int scenario,reads,writes;
static uint64_t demo_ms(void){return now;}
static int esp_http_client_read(void*h,char*p,size_t n){(void)h;(void)p;(void)n;reads++;
 if(scenario==1&&reads==2){now+=12000;return -ESP_ERR_HTTP_EAGAIN;}
 if(scenario==2){now+=12000;return -ESP_ERR_HTTP_EAGAIN;}
 if(scenario==3)return -1;if(scenario==4)return 0;if(scenario==5)return 2048;
 return 512;
}
static int psa_hash_update(void*h,void*p,int n){(void)h;(void)p;return n>0?0:-1;}
static int esp_ota_write(int h,void*p,int n){(void)h;(void)p;writes+=n;return 0;}
__declspec(dllexport) int run(int c){
 scenario=c;reads=writes=0;now=0;unsigned count=0,timeouts=0;uint64_t last_data=0;
 bool good=true;void *http=0,*hash=0;int ota=0,index=0;char data[2048];
 struct {unsigned size[2];} manifest={{1024,0}},*m=&manifest;
 while(count<m->size[index]&&reads<10){
'''
suffix=r'''
 }
 if(c<=1)return good&&count==1024&&writes==1024;
 if(c==2)return !good&&writes==0&&reads<=3;
 return !good&&writes==0&&reads==1;
}
'''
p=O/'ota_retry_harness.c';p.write_text(prefix+body+suffix,encoding='utf-8')
cc=os.environ.get('HOST_CC',str(O/'host-clean/deps/ziglang/zig.exe'))
subprocess.run([cc,'cc','-shared','-O1',str(p),'-o',str(O/'ota_retry.dll')],check=True)
lib=C.CDLL(str(O/'ota_retry.dll'));lib.run.argtypes=[C.c_int];lib.run.restype=C.c_int
class Retry(unittest.TestCase):
 def test_body_stream_and_bounded_transient_retry(self):
  for case in range(6):
   with self.subTest(case=case):self.assertEqual(lib.run(case),1)
if __name__=='__main__':unittest.main()
