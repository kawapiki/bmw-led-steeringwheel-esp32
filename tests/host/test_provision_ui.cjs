const test=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const vm=require('node:vm');
const path=require('node:path');
const html=fs.readFileSync(path.join(__dirname,'../../components/service_wifi/provision.html'),'utf8');
const script=html.match(/<script>([\s\S]*?)<\/script>/)[1];
function portal(scanReplies){
 const nodes={};for(const id of ['scanButton','scanMessage','networks','ssid','password','connection','connectButton','configForm','forgetButton'])nodes[id]={value:'',textContent:'',disabled:false,children:[],replaceChildren(...v){this.children=v},add(v){this.children.push(v)},focus(){this.focused=true}};
 const calls=[];let status;
 const context={document:{getElementById:id=>nodes[id]},Option:function(text,value){this.text=text;this.value=value},TextEncoder,Uint8Array,AbortController,Date,Promise,Error,
 setInterval:fn=>{status=fn},setTimeout:(fn,ms)=>{if(ms===500)queueMicrotask(fn);return 1},clearTimeout:()=>{},
 fetch:async url=>{calls.push(url);const data=url==='/status'?{state:0}:scanReplies.shift();assert.ok(data,`unexpected request ${url}`);return {ok:true,json:async()=>data,text:async()=>JSON.stringify(data)}}};
 vm.runInNewContext(script,context);return {nodes,calls,status:()=>status()};
}
test('one click waits for fresh scan, creates selectable networks, status does not overwrite result',async()=>{
 const p=portal([{state:1,networks:[]},{state:2,networks:[{ssid:'Home "&<net>',rssi:-51,auth:3},{ssid:'',rssi:-70,auth:3},{ssid:'Open',rssi:-55,auth:0}]}]);
 await p.nodes.scanButton.onclick();
 assert.deepEqual(p.calls,['/scan?start=1','/scan']);
 assert.equal(p.nodes.networks.children[1].text,'Home "&<net> · -51 dBm');
 assert.equal(p.nodes.networks.children[2].disabled,true);assert.equal(p.nodes.networks.children[3].disabled,true);
 p.nodes.networks.value='Home "&<net>';p.nodes.networks.onchange();assert.equal(p.nodes.ssid.value,'Home "&<net>');
 const scanText=p.nodes.scanMessage.textContent;await p.status();assert.equal(p.nodes.scanMessage.textContent,scanText);
 assert.equal(p.nodes.scanButton.disabled,false);
});
test('scan errors are visible and allow retry',async()=>{
 const p=portal([{state:3,error:'ESP_ERR_WIFI_STATE',networks:[]}]);await p.nodes.scanButton.onclick();assert.equal(p.nodes.scanMessage.textContent,'ESP_ERR_WIFI_STATE');assert.equal(p.nodes.scanButton.disabled,false);
});
test('empty scan is explicit instead of stale result',async()=>{
 const p=portal([{state:2,error:'',networks:[]}]);await p.nodes.scanButton.onclick();assert.match(p.nodes.scanMessage.textContent,/No networks found/);
});
