"""Exercise actual BLE telemetry callbacks/poll gate with a fake ATT boundary."""
from pathlib import Path
import os, subprocess, unittest
root=Path(__file__).resolve().parents[2];out=root/'.test-build'
s=(root/'components/ble_link/ble_link.c').read_text(encoding='utf-8')
functions=s[s.index('static void telemetry_state('):s.index('bool ble_link_ready(')]
gate=s[s.index('static bool recovery_enter('):s.index('bool ble_link_send(')]
publish=s[s.index('static void publish('):s.index('static bool authenticated(')]
prefix=r'''
#include "telemetry_v1.h"
#include "telemetry_cockpit.h"
#include "telemetry_lighting.h"
#include <assert.h>
#include <string.h>
#define portENTER_CRITICAL(x) ((void)0)
#define portEXIT_CRITICAL(x) ((void)0)
#define BLE_ERR_REM_USER_CONN_TERM 0x13
#define DEMO_SOURCE_NONE 0
#define DEMO_SOURCE_GATEWAY_DEMO 1
#define DEMO_SOURCE_VEHICLE 2
typedef struct {bool link_secure,maintenance,writing,app_compatible,telemetry_fresh; uint32_t telemetry_source,ble_sequence,ble_rpm;uint64_t telemetry_received;uint16_t telemetry_valid,speed_dkph;uint8_t gear,closure_open;int16_t coolant_c,oil_c;uint8_t lights_valid,lights_on,lights_source;uint32_t lights_sequence;uint64_t lights_received;} demo_state_t;
struct ble_gatt_error {int status;};
struct ble_gatt_attr {uint16_t offset; void *om;};
static bool central=true,secure=true,app_pending,recovery_busy,app_discovered=true;
static uint16_t app_handle=5,cockpit_handle,lighting_handle,conn=4,last_attribute;
static bool app_cockpit,app_lighting,next_lighting;
static telemetry_v1_tracker_t lighting_tracker;
static uint32_t app_request,app_session,connection_nonce=42;
static uint64_t app_requested,app_polled,now=1000;
static telemetry_v1_tracker_t telemetry_tracker;
static demo_state_t state={.link_secure=true};
static int reads,terminated;
static uint16_t packet_length=16;
static int read_error;
static bool finish_on_delay;
static void vTaskDelay(unsigned ms);
#define pdMS_TO_TICKS(x) (x)
static uint32_t last_token;
static uint64_t demo_ms(void){return now;}
static bool ble_link_secure(void){return secure;}
static void demo_get(demo_state_t *s){*s=state;}
static void demo_edit(void (*fn)(demo_state_t *,void *),void *a){fn(&state,a);}
static bool telemetry_is_fresh(bool c,bool a,uint64_t t,uint64_t n){return c&&a&&t&&n>=t&&n-t<=2500;}
static int ble_hs_mbuf_to_flat(void *m,void *p,size_t n,uint16_t *length){if(packet_length>n)return -1;memcpy(p,m,packet_length);*length=packet_length;return 0;}
static int ble_gap_terminate(uint16_t c,int why){(void)c;(void)why;terminated++;return 0;}
static int ble_gattc_read(uint16_t c,uint16_t h,int (*cb)(uint16_t,const struct ble_gatt_error *,struct ble_gatt_attr *,void *),void *a){(void)c;last_attribute=h;(void)cb;reads++;last_token=(uint32_t)(uintptr_t)a;return read_error;}
'''
suffix=r'''
static void deliver(uint32_t sequence){uint8_t p[16];telemetry_v1_encode(p,sequence,2000,sequence*100);struct ble_gatt_error e={0};struct ble_gatt_attr v={0,p};app_read_done(conn,&e,&v,(void *)(uintptr_t)last_token);}
static void vTaskDelay(unsigned ms){now+=ms;ble_link_poll_telemetry();if(finish_on_delay)deliver(20);}
int main(void){
 ble_link_poll_telemetry();assert(reads==1&&app_pending);
 now+=100;ble_link_poll_telemetry();assert(reads==1);
 deliver(1);assert(!app_pending&&state.ble_rpm==2000&&state.telemetry_received==1000&&state.telemetry_source==1);
 recovery_busy=true;ble_link_poll_telemetry();assert(reads==1);
 recovery_busy=false;state.maintenance=true;ble_link_poll_telemetry();assert(reads==1);
 state.maintenance=false;state.writing=true;ble_link_poll_telemetry();assert(reads==1);
 state.writing=false;ble_link_poll_telemetry();assert(reads==2);
 state.maintenance=true;now+=100;deliver(2);assert(state.ble_sequence==1);
 state.maintenance=false;ble_link_poll_telemetry();assert(reads==3);
 connection_nonce++;now+=100;deliver(3);assert(state.ble_sequence==1);
 ble_link_poll_telemetry();assert(reads==4);now+=100;deliver(1);assert(state.ble_sequence==1&&state.telemetry_received==1300);
 ble_link_poll_telemetry();assert(reads==5);now+=100;deliver(1);assert(state.telemetry_received==1300);
 ble_link_poll_telemetry();assert(reads==6);uint32_t saved=last_token;last_token--;deliver(2);assert(app_pending);last_token=saved;deliver(2);assert(!app_pending&&state.ble_sequence==2);
 now+=100;read_error=1;ble_link_poll_telemetry();assert(!app_pending);read_error=0;
 now+=100;ble_link_poll_telemetry();assert(app_pending);now+=2501;ble_link_poll_telemetry();assert(terminated==1);
 // A recovery caller gates new reads before waiting for the pending ATT result.
 now=app_requested+100;finish_on_delay=true;int before=reads;
 assert(recovery_enter());assert(!app_pending&&recovery_busy&&reads==before);
 ble_link_poll_telemetry();assert(reads==before);recovery_leave();
 now+=100;ble_link_poll_telemetry();assert(reads==before+1);
 finish_on_delay=false;assert(!recovery_enter());assert(!recovery_busy&&terminated==2);
 // Optional cockpit characteristic fits a single default-MTU read.
 app_pending=false;cockpit_handle=6;now+=100;ble_link_poll_telemetry();assert(last_attribute==6);
 cockpit_sample_t cp={.source=1,.valid=COCKPIT_VALID_COOLANT|(1u<<COCKPIT_CLOSURE_SHIFT),.coolant_c=95,.closure_open=1};
 uint8_t bytes[22];cockpit_encode(bytes,30,3000,&cp);packet_length=22;
 struct ble_gatt_error e={0};struct ble_gatt_attr v={0,bytes};
 now+=100;app_read_done(conn,&e,&v,(void *)(uintptr_t)last_token);
 assert(state.telemetry_valid==cp.valid&&state.coolant_c==95&&state.closure_open==1&&state.ble_rpm==0);
 // Legacy gateway fallback clears optional validity and values.
 cockpit_handle=0;connection_nonce++;ble_link_poll_telemetry();assert(last_attribute==5);
 packet_length=16;now+=100;deliver(1);
 assert(state.telemetry_valid==COCKPIT_VALID_RPM&&state.speed_dkph==0&&state.gear==0&&state.closure_open==0&&state.coolant_c==0);
 // No optional lighting characteristic means unknown, never invented off.
 assert(!state.lights_valid&&!state.lights_received);
 lighting_handle=7;next_lighting=false;now+=50;ble_link_poll_telemetry();assert(last_attribute==5);
 deliver(2);now+=50;ble_link_poll_telemetry();assert(last_attribute==7);
 lighting_sample_t ls={.source=2,.valid=LIGHT_VALID_ALL,.on=LIGHT_LEFT_INDICATOR};
 uint8_t lp[16];lighting_encode(lp,1,100,&ls);struct ble_gatt_attr lv={0,lp};
 uint32_t lt=last_token;app_read_done(conn,&e,&lv,(void *)(uintptr_t)(lt-1));assert(app_pending);
 app_read_done(conn,&e,&lv,(void *)(uintptr_t)lt);
 assert(!app_pending&&state.lights_valid==63&&state.lights_on==8&&state.lights_source==2);
 uint64_t light_received=state.lights_received;
 now+=50;ble_link_poll_telemetry();assert(last_attribute==5);deliver(3);
 now+=50;ble_link_poll_telemetry();assert(last_attribute==7);
 lighting_encode(lp,2,200,&ls);lp[12]=1;app_read_done(conn,&e,&lv,(void *)(uintptr_t)last_token);
 assert(state.lights_received==light_received&&state.app_compatible);
 now+=50;ble_link_poll_telemetry();deliver(4);
 now+=50;ble_link_poll_telemetry();lighting_encode(lp,1,100,&ls);
 app_read_done(conn,&e,&lv,(void *)(uintptr_t)last_token);assert(state.lights_received==light_received);
 now=light_received+2501;publish(&state,0);assert(!state.lights_valid&&!state.lights_on&&!state.lights_source);
 // A disconnected callback cannot revive old data; legacy peers remain usable.
 now+=50;ble_link_poll_telemetry();secure=false;publish(&state,0);deliver(5);
 assert(!state.lights_received&&!state.lights_valid);
 lighting_handle=0;secure=true;publish(&state,0);now+=100;ble_link_poll_telemetry();assert(last_attribute==5);
 deliver(6);assert(state.telemetry_valid==COCKPIT_VALID_RPM&&!state.lights_valid);
 return 0;
}
'''
class TelemetryBle(unittest.TestCase):
 def test_production_callbacks_and_polling(self):
  source=out/'telemetry_ble_test.c';source.write_text(prefix+publish+functions+gate+suffix,encoding='utf-8')
  compiler=os.environ.get('HOST_CC',str(out/'host-clean/deps/ziglang/zig.exe'));binary=out/'telemetry_ble_test.exe'
  subprocess.run([compiler,'cc','-std=c11','-Wall','-Wextra','-Werror','-I',str(root/'components/ble_link/include'),'-I',str(root/'components/contracts/include'),str(source),str(root/'components/ble_link/telemetry_v1.c'),str(root/'components/ble_link/telemetry_cockpit.c'),str(root/'components/ble_link/telemetry_lighting.c'),'-o',str(binary)],check=True)
  subprocess.run([str(binary)],check=True)
if __name__=='__main__':unittest.main()
