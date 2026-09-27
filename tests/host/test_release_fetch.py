"""Execute the production bounded listing reader with a fake HTTP stream.
Set HOST_CC to Zig0.14.1; no live network or credentials needed.
"""
from pathlib import Path
import ctypes as C, os, subprocess, unittest
root=Path(__file__).resolve().parents[2];out=root/'.test-build';out.mkdir(exist_ok=True)
s=(root/'components/update_manager/update_manager.c').read_text(encoding='utf-8')
reader=s[s.index('static int fetch_listing('):s.index('static bool get_manifest(')]
prefix=r'''
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
typedef void *esp_http_client_handle_t;
static char fetch_error[96];
static int scenario,pos,total,opened,closed,allocation_before_open;
static size_t first_allocation;
static esp_http_client_handle_t open_url(const char *url) {(void)url;opened=1;return scenario==6?NULL:(void *)1;}
static int64_t esp_http_client_get_content_length(void *h) {(void)h;return (scenario==1||scenario==4||scenario==5)?-1:total;}
static bool esp_http_client_is_complete_data_received(void *h) {(void)h;return scenario==8 || (pos==total&&scenario!=5);}
static int esp_http_client_read(void *h,char *p,size_t cap) {
 (void)h;if(pos>=total)return 0;size_t n=total-pos;if(n>cap)n=cap;if(n>17)n=17;
 memset(p,'x',n);pos+=n;return n;
}
static void esp_http_client_close(void *h) {(void)h;closed++;}
static void esp_http_client_cleanup(void *h) {(void)h;}
static void *checked_malloc(size_t n) {if(!opened)allocation_before_open++;if(!first_allocation)first_allocation=n;return scenario==7?NULL:malloc(n);}
#define malloc checked_malloc
#define ESP_LOGI(...) ((void)0)
'''
suffix=r'''
#undef malloc
__declspec(dllexport) int run_case(int c) {
 scenario=c;pos=0;opened=closed=allocation_before_open=0;first_allocation=0;
 total=c==0?2:c==2?24576:(c==3||c==4)?24577:51;
 char *result=NULL;int n=fetch_listing("https://test",&result);
 int expected=c>=3&&c!=8?-1:total;
 int ok=n==expected&&!allocation_before_open;
 if(n>=0){ok=ok&&result&&result[n]==0;for(int i=0;i<n;i++)ok=ok&&result[i]=='x';}
 else ok=ok&&!result;
 if(c==0)ok=ok&&first_allocation==3;
 if(c==3)ok=ok&&first_allocation==0;
 ok=ok&&closed==(c==6?0:1);
 free(result);return ok;
}
'''
source=out/'release_fetch_harness.c';source.write_text(prefix+reader+suffix,encoding='utf-8')
compiler=os.environ.get('HOST_CC',str(out/'host-clean/deps/ziglang/zig.exe'))
subprocess.run([compiler,'cc','-shared','-O1',str(source),'-o',str(out/'release_fetch.dll')],check=True)
lib=C.CDLL(str(out/'release_fetch.dll'));lib.run_case.argtypes=[C.c_int];lib.run_case.restype=C.c_int
class ListingFetch(unittest.TestCase):
 def test_stream_boundaries_and_allocation_order(self):
  for case in range(9):
   with self.subTest(case=case):self.assertEqual(lib.run_case(case),1)
if __name__=='__main__':unittest.main()
