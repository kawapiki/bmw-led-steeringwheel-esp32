#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef int esp_err_t;
#define IRAM_ATTR
#define ESP_OK 0
#define ESP_ERR_INVALID_ARG 1
#define ESP_ERR_INVALID_STATE 2
#define ESP_FAIL 3
#define ESP_ERR_TIMEOUT 4
#define ESP_ERROR_CHECK(x) assert((x)==ESP_OK)
#define GPIO_MODE_OUTPUT 1
#define RMT_CLK_SRC_DEFAULT 0
#define WHEEL_LED_CHAIN_0 3
#define WHEEL_LED_CHAIN_1 4
typedef void *rmt_channel_handle_t;
typedef void *rmt_encoder_handle_t;
typedef union {struct {uint32_t duration0:15,level0:1,duration1:15,level1:1;};uint32_t val;} rmt_symbol_word_t;
typedef struct {int gpio_num,clk_src,resolution_hz,mem_block_symbols,trans_queue_depth;struct {bool with_dma;} flags;} rmt_tx_channel_config_t;
typedef size_t (*rmt_encode_simple_cb_t)(const void*,size_t,size_t,size_t,rmt_symbol_word_t*,bool*,void*);
typedef struct {rmt_encode_simple_cb_t callback;void *arg;size_t min_chunk_size;} rmt_simple_encoder_config_t;
static rmt_simple_encoder_config_t encoded;
typedef struct {int loop_count;struct {int eot_level;} flags;} rmt_transmit_config_t;
static int hw_pin, enabled, pending, pin_low[5], failures, calls[6], new_channels;
static int matrix_rmt[5], output_enabled[5];
static rmt_tx_channel_config_t allocated;
static rmt_symbol_word_t captured[577];
static int gpio_set_level(int pin,int level){assert(!level);pin_low[pin]=1;return 0;}
static int gpio_set_direction(int pin,int mode){assert(mode==GPIO_MODE_OUTPUT);assert(!enabled);
 /* IDF6.1 gpio_set_direction -> gpio_output_enable -> gpio_hal_matrix_out_default. */
 matrix_rmt[pin]=0;output_enabled[pin]=1;pin_low[pin]=1;return 0;}
static int gpio_pulldown_en(int pin){assert(pin==3||pin==4);return 0;}
static int rmt_new_tx_channel(const rmt_tx_channel_config_t *c,void **out){allocated=*c;new_channels++;hw_pin=c->gpio_num;matrix_rmt[hw_pin]=output_enabled[hw_pin]=1;*out=(void *)1;return 0;}
static int rmt_new_simple_encoder(const rmt_simple_encoder_config_t *c,void **out){encoded=*c;*out=(void *)2;return 0;}
static int rmt_del_channel(void *c){(void)c;return 0;}
static int rmt_tx_switch_gpio(void *c,int pin,bool invert){assert(c&&!enabled&&!pending&&!invert);calls[1]++;if(failures==1)return ESP_FAIL;pin_low[hw_pin]=0;output_enabled[hw_pin]=0;hw_pin=pin;matrix_rmt[pin]=1;return 0;}
static int rmt_enable(void *c){assert(c&&!enabled);calls[2]++;if(failures==2)return ESP_FAIL;assert(pin_low[hw_pin==3?4:3]);enabled=1;return 0;}
static int rmt_transmit(void *c,void *enc,const void *data,size_t n,const rmt_transmit_config_t *cfg){
 assert(c&&enc&&enabled&&!pending);
 assert(matrix_rmt[hw_pin]&&output_enabled[hw_pin]);
 int other=hw_pin==3?4:3;assert(!matrix_rmt[other]&&output_enabled[other]&&pin_low[other]);
 calls[3]++;if(failures==3)return ESP_FAIL;
 assert(cfg->loop_count==0&&cfg->flags.eot_level==0);assert(n==72);
 assert(allocated.flags.with_dma&&allocated.mem_block_symbols>=578);
 bool done=false;
 size_t count=encoded.callback(data,n,0,allocated.mem_block_symbols,captured,&done,encoded.arg);
 assert(done&&count==577);pending=1;return 0;
}
static int rmt_tx_wait_all_done(void *c,int ms){assert(c&&enabled&&pending&&ms>0&&ms<=20);calls[4]++;if(failures==4)return ESP_ERR_TIMEOUT;pending=0;return 0;}
static int rmt_disable(void *c){assert(c&&enabled);calls[5]++;if(failures==5)return ESP_FAIL;pending=enabled=0;return 0;}
#include "../../firmware/wheel/components/wheel_io/wheel_led.c"
static void setup(void){
 channel=NULL;encoder=NULL;active=0;faulted=false;memset(pixels,0,sizeof(pixels));
 enabled=pending=failures=new_channels=0;memset(calls,0,sizeof(calls));memset(pin_low,0,sizeof(pin_low));memset(matrix_rmt,0,sizeof(matrix_rmt));memset(output_enabled,0,sizeof(output_enabled));
 assert(wheel_led_init()==0);assert(new_channels==1&&!enabled&&!pending);memset(calls,0,sizeof(calls));
}
static uint8_t byte_at(unsigned index){
 uint8_t byte=0;
 for(unsigned b=0;b<8;b++){
  rmt_symbol_word_t s=captured[index*8+b];assert(s.level0==1&&s.level1==0);
  assert((s.duration0==3&&s.duration1==9)||(s.duration0==9&&s.duration1==3));
  byte=(byte<<1)|(s.duration0==9);
 }
 return byte;
}
int main(void){
 setup();
 wheel_led_set(0,0,0x80,0x01,0xaa);wheel_led_set(0,23,0x55,0xff,0x10);
 assert(wheel_led_refresh(0)==0);assert(hw_pin==3&&pin_low[4]);
 assert(byte_at(0)==0x01&&byte_at(1)==0x80&&byte_at(2)==0xaa);
 assert(byte_at(69)==0xff&&byte_at(70)==0x55&&byte_at(71)==0x10);
 for(unsigned i=3;i<69;i++)assert(byte_at(i)==0);
 assert(!captured[576].level0&&!captured[576].level1);
 assert(captured[576].duration0+captured[576].duration1==2800);
 assert(wheel_led_refresh(1)==0&&hw_pin==4&&pin_low[3]);
 for(unsigned i=0;i<72;i++)assert(byte_at(i)==0);
 assert(wheel_led_refresh(0)==0&&byte_at(0)==1);
 rmt_symbol_word_t chunked[577];size_t offset=0;bool done=false;
 while(!done){size_t n=encode_frame(pixels[0],72,offset,24,chunked+offset,&done,NULL);assert(n>0&&n<=24);offset+=n;}
 assert(offset==577&&!memcmp(chunked,captured,sizeof(chunked)));
 puts("PASS complete frame, GRB/MSB order, pixel 0 and 23, independent chains, low idle");
 for(int fail=1;fail<=5;fail++){
  setup();failures=fail;
  assert(wheel_led_refresh(0)!=0);
  if(fail==1)assert(!calls[2]&&!calls[3]);
  if(fail==2)assert(!calls[3]&&!calls[5]);
  if(fail==3||fail==4)assert(calls[5]==1&&!enabled&&!pending);
  if(fail==5){
   uint8_t held[2][72];memcpy(held,pixels,sizeof(held));
   int tx_before=calls[3];wheel_led_set(1,0,255,255,255);failures=0;
   assert(wheel_led_refresh(1)==ESP_ERR_INVALID_STATE);
   assert(calls[3]==tx_before&&!memcmp(held,pixels,sizeof(held)));
  }else{failures=0;assert(wheel_led_refresh(0)==ESP_OK);}
 }
 puts("PASS GPIO-switch, enable, transmit, timeout and stop failure handling");
 setup();assert(wheel_led_refresh(2)==ESP_ERR_INVALID_ARG&&!calls[3]);
 wheel_led_set(2,0,1,1,1);wheel_led_set(0,24,1,1,1);
 assert(wheel_led_refresh(0)==0);for(unsigned i=0;i<72;i++)assert(byte_at(i)==0);
 puts("PASS out-of-range writes and chain validation");return 0;
}
