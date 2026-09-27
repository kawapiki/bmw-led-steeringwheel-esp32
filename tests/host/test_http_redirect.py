"""Exercise the pinned IDF request-line formatter with long release redirects."""
import ctypes as C, os, re, subprocess, unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
IDF=Path(os.environ.get('IDF_PATH','C:/esp/v6.1/esp-idf'))
s=(IDF/'components/esp_http_client/esp_http_client.c').read_text(encoding='utf-8')
start=s.index('    const char *method = HTTP_METHOD_MAPPING[client->connection_info.method];',s.index('static int http_client_prepare_first_line('))
end=s.index('\n}',start)
body=s[start:end]
config=(ROOT/'components/update_manager/update_manager.c').read_text(encoding='utf-8')
match=re.search(r'\.buffer_size_tx\s*=\s*(\d+)',config)
capacity=int(match.group(1)) if match else 512
prefix=r'''
#include <stdio.h>
#include <string.h>
#define ESP_LOGE(...) ((void)0)
#define DEFAULT_HTTP_PROTOCOL "HTTP/1.1"
static const char *HTTP_METHOD_MAPPING[]={"GET"};
struct buf { char *data; };
struct req { struct buf *buffer; };
struct client { int buffer_size_tx; struct {int method; const char *path,*query;} connection_info; struct req *request; };
static int format(struct client *client) {
'''
suffix=r'''
}
__declspec(dllexport) int check(int capacity,int query_size) {
 char storage[4096],query[4096];memset(query,'x',query_size);query[query_size]=0;
 struct buf b={storage};struct req r={&b};struct client c={capacity,{0,"/asset",query},&r};
 int n=format(&c);if(n<0)return n;
 return (n==(int)strlen(storage)&&strstr(storage," HTTP/1.1\r\n"))?n:-2;
}
'''
out=ROOT/'.test-build';source=out/'http_redirect_harness.c';source.write_text(prefix+body+suffix,encoding='utf-8')
cc=os.environ.get('HOST_CC',str(out/'host-clean/deps/ziglang/zig.exe'))
subprocess.run([cc,'cc','-shared','-O1',str(source),'-o',str(out/'http_redirect.dll')],check=True)
lib=C.CDLL(str(out/'http_redirect.dll'));lib.check.argtypes=[C.c_int,C.c_int];lib.check.restype=C.c_int
class Redirect(unittest.TestCase):
 def test_default_reproduces_observed_failure(self):self.assertEqual(lib.check(512,858),-1)
 def test_production_configuration_handles_github_asset_request(self):self.assertGreater(lib.check(capacity,858),0)
 def test_longer_redirect_and_bounded_rejection(self):
  self.assertGreater(lib.check(capacity,1800),0)
  self.assertEqual(lib.check(capacity,2200),-1)
if __name__=='__main__':unittest.main()
