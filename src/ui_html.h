#pragma once
#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="zh"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>拓竹FDM雷达</title>
<meta name="theme-color" content="#0a0e13">
<link rel="icon" type="image/svg+xml" href="data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 64 64'%3E%3Cdefs%3E%3ClinearGradient id='g' x1='0' y1='0' x2='1' y2='1'%3E%3Cstop offset='0' stop-color='%233b9cff'/%3E%3Cstop offset='1' stop-color='%237c5cff'/%3E%3C/linearGradient%3E%3C/defs%3E%3Ccircle cx='32' cy='32' r='29' fill='%230d141c' stroke='url(%23g)' stroke-width='4'/%3E%3Ccircle cx='32' cy='32' r='18' fill='none' stroke='%232a3a4d' stroke-width='2'/%3E%3Ccircle cx='32' cy='32' r='7' fill='none' stroke='%232a3a4d' stroke-width='2'/%3E%3Cpath d='M32 32 L32 4 A28 28 0 0 1 56.2 18 Z' fill='%233b9cff' fill-opacity='.55'/%3E%3Ccircle cx='44' cy='20' r='3.5' fill='%2334d399'/%3E%3C/svg%3E">
<style>
:root{--bg:#0a0e13;--card:#121a23;--card2:#17212c;--line:#223041;--fg:#e8eef6;--mut:#7f8ea3;--acc:#3b9cff;--acc2:#7c5cff;--bad:#ff5d6c;--ok:#34d399;--warn:#ffb020}
*{box-sizing:border-box}
body{margin:0;background:radial-gradient(900px 420px at 8% -8%,#17406b55,transparent 70%),radial-gradient(700px 380px at 100% 0,#3a2a8a38,transparent 70%),var(--bg);background-attachment:fixed;color:var(--fg);font:15px/1.5 system-ui,-apple-system,"Segoe UI","Microsoft YaHei",sans-serif;padding-bottom:84px}
header{display:flex;align-items:center;gap:12px;padding:14px 16px;border-bottom:1px solid var(--line);background:#0a0e13cc;backdrop-filter:blur(10px)}
header .t{font-size:18px;font-weight:800;letter-spacing:.5px}
header .s{font-size:12px;color:var(--mut);font-weight:400}
header .chip{margin-left:auto}
.logo{flex:0 0 36px;width:36px;height:36px}
.logo .swp{transform-origin:18px 18px;animation:sw 3.2s linear infinite}
@keyframes sw{to{transform:rotate(360deg)}}
nav{display:flex;gap:6px;padding:10px 12px;overflow-x:auto;position:sticky;top:0;background:#0a0e13d9;backdrop-filter:blur(10px);z-index:5;border-bottom:1px solid var(--line)}
nav button{flex:0 0 auto;background:transparent;color:var(--mut);border:1px solid transparent;border-radius:18px;padding:7px 14px;font-size:14px;cursor:pointer;transition:.15s}
nav button:hover{color:var(--fg)}
nav button.on{background:linear-gradient(135deg,var(--acc),var(--acc2));color:#fff;font-weight:600;box-shadow:0 4px 14px #3b9cff44}
main{padding:12px;max-width:720px;margin:0 auto}
.card{background:linear-gradient(180deg,var(--card2),var(--card));border:1px solid var(--line);border-radius:16px;padding:14px 16px;margin-bottom:12px;box-shadow:0 6px 20px #00000040}
.card h3{margin:0 0 10px;font-size:14px;color:var(--mut);font-weight:600;letter-spacing:.6px}
.row{display:flex;gap:10px;align-items:center;margin:8px 0;flex-wrap:wrap}
.row>label{flex:1 1 130px;color:var(--mut);font-size:13px}
input[type=text],input[type=password],input[type=number],input[type=time],select{flex:2 1 160px;background:#0d141c;color:var(--fg);border:1px solid var(--line);border-radius:10px;padding:8px 10px;font-size:15px;min-width:0}
input:focus,select:focus{outline:2px solid #3b9cff77;border-color:var(--acc)}
input[type=range]{flex:2 1 160px}
button.b{background:linear-gradient(135deg,var(--acc),var(--acc2));color:#fff;border:0;border-radius:10px;padding:8px 14px;font-weight:600;font-size:14px;cursor:pointer;box-shadow:0 4px 14px #3b9cff44}
button.g{background:#232b35;color:var(--fg);border:1px solid var(--line);border-radius:10px;padding:7px 12px;font-size:14px;cursor:pointer}
button.d{background:#3a1d1f;color:#ff9a9a;border:1px solid #5b2a2d;border-radius:10px;padding:7px 12px;font-size:14px;cursor:pointer}
.sw{display:flex;gap:14px;flex-wrap:wrap;margin:6px 0}
.sw label{display:flex;gap:6px;align-items:center;font-size:14px}
.mut{color:var(--mut);font-size:13px}.ok{color:var(--ok)}.bad{color:var(--bad)}
code{background:#0d141c;border:1px solid var(--line);border-radius:5px;padding:0 5px;font-size:13px}
.bar{position:fixed;left:0;right:0;bottom:0;background:#0a0e13dd;backdrop-filter:blur(10px);border-top:1px solid var(--line);padding:10px 14px;display:flex;gap:10px;justify-content:center}
#toast{position:fixed;left:50%;bottom:72px;transform:translateX(-50%);background:#000c;padding:8px 16px;border-radius:20px;display:none;z-index:9}
.li{display:flex;justify-content:space-between;align-items:center;gap:8px;padding:8px 0;border-top:1px solid var(--line)}
.li:first-child{border-top:0}
.chip{display:inline-flex;align-items:center;gap:6px;padding:3px 10px;border-radius:999px;font-size:12px;font-weight:600;background:#1c2733;color:var(--mut)}
.chip::before{content:"";width:7px;height:7px;border-radius:50%;background:currentColor}
.chip.ok{background:#0f2f25;color:var(--ok)}.chip.bad{background:#3a1820;color:var(--bad)}.chip.warn{background:#3a2c0f;color:var(--warn)}.chip.run{background:#10294a;color:var(--acc)}
.hero{display:grid;grid-template-columns:repeat(3,1fr);gap:10px;margin-bottom:12px}
.tile{background:linear-gradient(180deg,var(--card2),var(--card));border:1px solid var(--line);border-radius:16px;padding:12px 10px;text-align:center}
.tile b{display:block;font-size:20px;line-height:1.2}
.tile span{font-size:12px;color:var(--mut)}
.pc{display:flex;gap:14px;align-items:center}
.ring{--c:var(--acc);--p:0;flex:0 0 76px;width:76px;height:76px;border-radius:50%;display:grid;place-items:center;background:conic-gradient(var(--c) calc(var(--p)*1%),#1d2835 0)}
.ring span{width:60px;height:60px;border-radius:50%;background:var(--card);display:grid;place-items:center;font-weight:800;font-size:17px}
.pm{flex:1;min-width:0}
.pm .nm{display:flex;justify-content:space-between;align-items:center;gap:8px;font-weight:700;font-size:16px}
.pm .job{color:var(--mut);font-size:13px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;margin:2px 0 0}
.st{display:grid;grid-template-columns:repeat(auto-fit,minmax(96px,1fr));gap:8px;margin-top:12px}
.st div{background:#0d141c;border:1px solid var(--line);border-radius:12px;padding:8px 10px}
.st div.act{border-color:var(--acc);box-shadow:0 0 0 1px #3b9cff55 inset}
.st small{display:block;font-size:11px;color:var(--mut)}
.st b{font-size:15px}
.st i{font-style:normal;font-size:12px;color:var(--mut);font-weight:400}
.sig{display:inline-flex;gap:2px;align-items:flex-end;height:14px;margin-right:6px;vertical-align:middle}
.sig i{width:4px;background:#2a3a4d;border-radius:1px}.sig i.on{background:var(--ok)}
.lang{background:#1c2733;color:var(--fg);border:1px solid var(--line);border-radius:999px;padding:3px 10px;font-size:12px;cursor:pointer;margin-left:8px}
.trays{display:grid;grid-template-columns:repeat(4,1fr);gap:6px;margin-top:6px}
.tray{background:#0d141c;border:1px solid var(--line);border-radius:10px;padding:7px 4px;text-align:center;min-width:0}
.tray.on{border-color:var(--acc);box-shadow:0 0 0 1px #3b9cff55 inset}
.tray.empty{opacity:.45;border-style:dashed}
.tray .sw2{display:block;width:22px;height:22px;border-radius:50%;margin:0 auto 4px;border:2px solid #3a4a5e}
.tray b{font-size:12px;display:block;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.tray small{font-size:11px;color:var(--mut);display:block;min-height:14px}
.amsh{display:flex;justify-content:space-between;gap:8px;font-size:12px;color:var(--mut);margin-top:12px}
.amsh b{color:var(--fg)}
</style></head><body>
<header><svg class="logo" viewBox="0 0 36 36"><defs><linearGradient id="lg" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#3b9cff"/><stop offset="1" stop-color="#7c5cff"/></linearGradient><linearGradient id="sg" x1="0" y1="0" x2="1" y2="0"><stop offset="0" stop-color="#3b9cff" stop-opacity="0"/><stop offset="1" stop-color="#3b9cff" stop-opacity=".8"/></linearGradient></defs><circle cx="18" cy="18" r="16" fill="#0d141c" stroke="url(#lg)" stroke-width="2"/><circle cx="18" cy="18" r="10" fill="none" stroke="#2a3a4d"/><circle cx="18" cy="18" r="4" fill="none" stroke="#2a3a4d"/><path class="swp" d="M18 18 L18 2 A16 16 0 0 1 32 10 Z" fill="url(#sg)"/><circle cx="25" cy="11" r="2" fill="#34d399"/></svg><div><div class="t" id="ht"></div><div class="s" id="hsub"></div></div><span id="hs" class="chip"></span><button class="lang" id="lang" onclick="setLang()"></button></header>
<nav id="nav"></nav>
<main id="v"></main>
<div class="bar"><button class="g" id="bstop" onclick="stopAlarm()"></button><button class="b" id="bsave" onclick="save()"></button></div>
<div id="toast"></div>
<script>
const $=s=>document.querySelector(s);
let LANG='zh';
try{LANG=(navigator.language||'').toLowerCase().indexOf('zh')===0?'zh':'en'}catch(e){}   // only for the loading screen: the device setting (cfg.lang) takes over as soon as the state arrives
const t=(z,e)=>LANG==='en'?e:z;
async function setLang(){LANG=LANG==='en'?'zh':'en';if(C)C.lang=LANG==='en'?1:0;applyStatic();if(S)render();await api('/api/lang?l='+LANG,{method:'POST'});if(LOG)loadLog()}   // stored on the device: push messages and error texts follow it
function applyStatic(){
  document.documentElement.lang=LANG==='en'?'en':'zh';
  document.title=t('拓竹FDM雷达','Bambu FDM Radar');
  $('#ht').textContent=t('拓竹FDM雷达','Bambu FDM Radar');
  $('#hsub').textContent=t('Bambu Lab 打印机监控 · 设置','Bambu Lab printer monitor · Settings');
  $('#bstop').textContent=t('停止报警','Stop alarm');
  $('#bsave').textContent=t('保存设置','Save settings');
  $('#lang').textContent=LANG==='en'?'中文':'EN';
  if(!S){$('#v').textContent=t('加载中…','Loading…');$('#hs').className='chip';$('#hs').textContent=t('连接中','Connecting')}
}
const TABS=()=>[['status','📡 '+t('状态','Status')],['wifi','📶 Wi-Fi'],['printer','🖨️ '+t('打印机','Printers')],['alarm','🔔 '+t('告警','Alerts')],['clock','⏰ '+t('闹钟','Alarm clock')],['err','🧹 '+t('错误码','Error codes')],['notify','📨 '+t('通知','Notify')],['log','📋 '+t('记录','Log')],['sound','🔊 '+t('音效','Sounds')],['fw','⬆️ '+t('固件','Firmware')],['debug','🐞 '+t('调试','Debug')]];
const EV=()=>[['failed',t('打印失败','Print failed')],['finish',t('打印完成','Print finished')],['pause',t('暂停 / 断料','Paused / filament out')],['error',t('错误 / HMS','Error / HMS')],['offline',t('打印机失联','Printer offline')],['clock',t('闹钟','Alarm clock')]];
const HINT=k=>({clock:t('这里只设置闹钟的铃声、强度和自动停止时间;响铃的时间在「闹钟」页设置。','Only the sound, strength and auto-stop of the alarm clock are set here; the ring times are set on the Alarm clock tab.'),error:t('只对「错误码」页设定的门槛以上的 HMS 和打印机错误码响铃,其余只记入「记录」页。','Rings only for HMS notices above the threshold set on the Error codes tab, and for printer error codes. Everything else is only written to the Log tab.'),
 offline:t('判定依据:与打印机的 MQTT 连接断开超过「失联判定」秒;连接正常时(哪怕打印机空闲不发数据)不会触发。打印机恢复连接后,失联告警会自动停止。','Rule: the MQTT link to the printer has been down longer than "Offline after (s)". It never fires while the link is up, even if an idle printer sends no data. The alarm stops by itself when the printer reconnects.')})[k]||'';
const STG=()=>[t('温和(喇叭从渐强开始)','Gentle (speaker fades in)'),t('加强(喇叭满音量 + 警报器间歇)','Loud (full-volume speaker + intermittent buzzer)'),t('最强(警报器持续)','Maximum (continuous buzzer)')];
const NT=()=>['ntfy (Android / iOS / web)','Bark (iOS)','Telegram',t('自定义 Webhook(n8n 等)','Custom webhook (n8n etc.)'),t('飞书机器人','Feishu bot'),t('企业微信机器人','WeCom bot'),t('钉钉机器人','DingTalk bot')];
const NF=()=>{const kw=t('自定义关键词(机器人开了关键词校验才填)','Custom keyword (only if the bot has keyword check enabled)');const bot=t('机器人 Webhook 地址','Bot webhook URL');return [
 {url:[t('主题地址','Topic URL'),'https://ntfy.sh/'+t('你的主题名','your-topic')],key:[t('访问令牌(可选,私有主题才需要)','Access token (optional, private topics only)')]},
 {url:[t('服务器(留空 = https://api.day.app)','Server (empty = https://api.day.app)'),'https://api.day.app'],key:[t('设备 Key','Device key')]},
 {url:[t('API 地址(留空 = https://api.telegram.org)','API base (empty = https://api.telegram.org)'),''],key:['Bot Token'],extra:['Chat ID']},
 {url:['Webhook URL','https://']},
 {url:[bot,'https://…/open-apis/bot/v2/hook/…'],extra:[kw]},
 {url:[bot,'https://…/cgi-bin/webhook/send?key=…']},
 {url:[bot,'https://…/robot/send?access_token=…'],extra:[kw]}]};
const GS=g=>({RUNNING:t('打印中','Printing'),PAUSE:t('已暂停','Paused'),FINISH:t('已完成','Finished'),FAILED:t('失败','Failed'),IDLE:t('空闲','Idle'),PREPARE:t('准备中','Preparing'),SLICING:t('切片中','Slicing')})[g]||g;
let S=null,C=null,tab='status',nets=null,LOG=null;
const DBG={i:0,kind:'snap',f:'',auto:false,text:'',bad:false};
let ERRTXT='',ERRRES='',ERRDEF=2;
const esc=s=>String(s==null?'':s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
function toast(m){const e=$('#toast');e.textContent=m;e.style.display='block';clearTimeout(toast.h);toast.h=setTimeout(()=>e.style.display='none',2600)}
async function api(p,o){try{const r=await fetch(p,o);return await r.json()}catch(e){toast(t('请求失败','Request failed'));return null}}
function get(p){return p.split('.').reduce((o,k)=>o==null?o:o[k],C)}
function set(p,v){const ks=p.split('.');let o=C;for(let i=0;i<ks.length-1;i++)o=o[ks[i]];o[ks[ks.length-1]]=v}
function bind(){document.querySelectorAll('[data-k]').forEach(el=>{
  const k=el.dataset.k,v=get(k);
  if(el.type==='checkbox')el.checked=!!v;else el.value=v==null?'':v;
  el.onchange=el.oninput=(ev)=>{
    const num=el.type==='number'||el.type==='range'||el.dataset.n;
    const x=el.type==='checkbox'?el.checked:(num?Number(el.value):el.value);
    set(k,x);
    if(el.type==='range'){const o=el.closest('.row').querySelector('.rv');if(o)o.textContent=x}
    if(el.dataset.r&&ev&&ev.type==='change')render();
  }})}
const rstName=r=>({1:t('上电','power-on'),2:t('外部复位','reset pin'),3:t('软件重启','software restart'),4:t('程序崩溃(异常)','crash (panic)'),5:t('中断看门狗(INT_WDT)','interrupt watchdog (INT_WDT)'),6:t('任务看门狗(TASK_WDT)','task watchdog (TASK_WDT)'),7:t('看门狗','watchdog'),9:t('欠压(供电不足)','brownout (power sag)'),14:t('电源毛刺','power glitch'),15:t('欠压(供电不足)','brownout')})[r]||('#'+r);
const CR=[{1:t('读按键','buttons'),2:t('处理打印机事件','handling printer events'),3:t('闹钟检查','alarm-clock check'),4:t('告警控制','alarm control'),5:t('数码管','digit display'),6:t('网页服务','web server'),7:t('灯环 / 其他','LEDs / other'),10:t('写事件记录文件(闪存)','writing the log file (flash)'),11:t('记录写完','log file written')},{1:t('等待','idle'),2:t('扫描 / 打印机连接','scanning / printer links'),3:t('收打印机消息','receiving printer data'),4:t('解析打印机报文','parsing a printer report'),5:t('解析完成','parsed'),6:t('连接打印机(TLS)','connecting to a printer (TLS)'),7:t('一轮结束','round finished')},{1:t('空闲','idle'),2:t('准备播放','starting playback'),3:t('打开音频文件','opening the sound file'),4:t('播放文件','playing the file'),5:t('播放提示音','playing a tone'),6:t('已停止','stopped')},{1:t('等待','idle'),2:t('取到推送任务','got a push job'),3:t('发送中','sending'),4:t('发送完','sent'),5:t('建立连接','connecting'),6:t('发送请求','sending the request'),7:t('请求完成','request done')}];
const crumbText=c=>{if(!c)return '';const nm=[t('主循环','main loop'),t('网络','network'),t('音频','audio'),t('推送','push')];return nm.map((n,i)=>n+' = '+((CR[i][(c>>(8*i))&255])||'-')).join(' · ')};
const dur=m=>{m=Math.max(0,Math.round(m));const h=(m/60)|0,r=m%60;return h?h+t(' 时 ',' h ')+r+t(' 分',' min'):r+t(' 分',' min')};
function sigBars(r){const n=r>-55?4:r>-65?3:r>-75?2:1;return `<span class="sig">${[5,8,11,14].map((h,i)=>`<i class="${i<n?'on':''}" style="height:${h}px"></i>`).join('')}</span>`}
const GC={RUNNING:['run','var(--acc)'],PAUSE:['warn','var(--warn)'],FINISH:['ok','var(--ok)'],FAILED:['bad','var(--bad)']};
// Everything shown on a printer card is built from what the printer actually reports.
// The printer numbers its two nozzles the other way round from the ids in the report (id 1 = nozzle 1).
const nzl=(p,id)=>(p.noz&&p.noz.length>1)?p.noz.length-id:id+1;
function amsHtml(p){
  const hasExt=p.ext&&p.ext.some(e=>e.ty||e.c!=='000000');
  if(!p.connected||!((p.ams&&p.ams.length)||hasExt))return '';
  const tn=Number(p.tn);
  const tile=(tr,on,label)=>{const has=tr.e!==false&&(tr.ty||tr.c!=='000000');
    return `<div class="tray${on?' on':''}${has?'':' empty'}"><i class="sw2" style="background:${has?'#'+tr.c:'transparent'}"></i><b>${has?esc(tr.ty||'?'):(label||t('空','Empty'))}</b><small>${has&&tr.r>0?tr.r+'%':''}</small></div>`};
  let h='';
  (p.ams||[]).forEach(u=>{
    const noz=(p.noz&&p.noz.length>1&&u.ex>=0&&u.ex<2)?' · '+t('喷嘴','Nozzle')+' '+nzl(p,u.ex):'';
    const env=(u.h>=0?'💧'+u.h+'% ':'')+(u.tp>0?u.tp+'°':'');
    h+=`<div class="amsh"><span><b>AMS ${u.i+1}</b>${noz}</span><span>${env}</span></div><div class="trays">${u.t.map((tr,k)=>tile(tr,tn<16&&Math.floor(tn/4)===u.i&&tn%4===k)).join('')}</div>`});
  if(hasExt){
    h+=`<div class="amsh"><span><b>${t('外挂料盘','External spool')}</b></span><span></span></div><div class="trays">${p.ext.filter(e=>e.ty||e.c!=='000000').map(e=>tile(e,tn===e.id,'')).join('')}</div>`}
  return h}
function pcard(p,c,i){
  const g=p.gcode||'';const gc=GC[g]||['',''];
  const active=p.connected&&(g==='RUNNING'||g==='PAUSE');
  const pct=active?Math.max(0,Math.min(100,p.percent)):(p.connected&&g==='FINISH'?100:0);
  const chip=!p.connected?`<span class="chip bad">${t('未连接','Offline')}</span>`:`<span class="chip ${gc[0]}">${esc(g?GS(g):t('等待数据','Waiting for data'))}</span>`;
  const ringC=!p.connected?'#3a4656':(gc[1]||'var(--acc)');
  const T=[];
  if(active&&p.has&16&&p.layers>0)T.push([t('层数','Layer'),p.layer+' / '+p.layers]);
  if(active&&p.has&8)T.push([t('剩余','Remaining'),dur(p.remain)]);
  const tg=(c,tt)=>c+'°'+(tt>0?` <i>→ ${tt}°</i>`:'');
  if(p.connected){
    if(p.noz&&p.noz.length){
      const multi=p.noz.length>1;
      p.noz.slice().sort((a,b)=>nzl(p,a.i)-nzl(p,b.i)).forEach(n=>T.push([t('喷嘴','Nozzle')+(multi?' '+nzl(p,n.i):'')+(n.a&&multi?' · '+t('使用中','active'):''),tg(n.c,n.t),n.a&&multi?'act':'']));
    }else if(p.has&1)T.push([t('喷嘴','Nozzle'),Math.round(p.nozzle)+'°C']);
    if(p.has&2)T.push([t('热床','Bed'),tg(Math.round(p.bed),p.bedT)]);
    if(p.has&4)T.push([t('仓温','Chamber'),tg(Math.round(p.chamber),p.chamberT)]);
    if(p.since>=0)T.push([t('本状态持续','In this state'),dur(p.since/60)]);
  }
  return `<div class="card"><div class="pc"><div class="ring" style="--p:${pct};--c:${ringC}"><span>${p.connected?pct+'%':'—'}</span></div><div class="pm"><div class="nm"><span>${i+1}. ${esc(c.name||c.ip)}</span>${chip}</div><div class="job">${esc(p.connected&&p.job?p.job:c.ip)}</div></div></div>${T.length?`<div class="st">${T.map(x=>`<div class="${x[2]||''}"><small>${x[0]}</small><b>${x[1]}</b></div>`).join('')}</div>`:''}${amsHtml(p)}</div>`}
const V={
status(){const w=S.wifi;
const en=S.cfg.printers.map((c,i)=>c.enabled?i:-1).filter(i=>i>=0);
const on=en.filter(i=>S.printers[i].connected).length;
const run=en.filter(i=>S.printers[i].connected&&S.printers[i].gcode==='RUNNING').length;
return `<div class="hero"><div class="tile"><b>${on}<span style="font-size:13px"> / ${en.length}</span></b><span>${t('打印机在线','Printers online')}</span></div><div class="tile"><b>${run}</b><span>${t('正在打印','Printing')}</span></div><div class="tile"><b style="font-size:16px;padding-top:3px">${S.time?esc(S.time.slice(5)):'--'}</b><span>${t('设备时间','Device time')}</span></div></div>`+
(en.length?en.map(i=>pcard(S.printers[i],S.cfg.printers[i],i)).join(''):`<div class="card"><div class="mut">${t('还没有启用打印机,到「打印机」页添加。','No printer enabled yet. Add one on the Printers tab.')}</div></div>`)+
`<div class="card"><h3>${t('网络','Network')}</h3><div>${w.connected?`${sigBars(w.rssi)}<b>${esc(w.ssid)}</b> <span class="mut">${esc(w.ip)} · ${w.rssi} dBm</span>`:`<span class="chip bad">${t('未连接 Wi-Fi','Wi-Fi not connected')}</span>`}</div>
${w.connected?`<div class="mut" style="margin-top:6px">${t('电脑访问:','Open from a PC: ')}<code>http://${esc(w.ip)}</code> / <code>http://${esc(w.host)}</code>${S.cfg.lanAccess?'':t('(局域网访问已关闭)',' (LAN access is off)')}</div>`:''}
<div class="mut">${t('配置热点 ','Setup hotspot ')}${esc(w.ap)}${w.apOn?'':t('(当前未开启)',' (currently off)')}</div></div>
<div class="card"><h3>${t('设备','Device')}</h3><div class="mut">${t('空闲内存','Free heap')} ${(S.heap/1024)|0} KB · PSRAM ${(S.psram/1048576).toFixed(1)} MB · ${t('存储','Storage')} ${(S.fsUsed/1048576).toFixed(1)}/${(S.fsTotal/1048576).toFixed(1)} MB</div>
<div class="mut">${t('已运行','Uptime')} ${dur(S.up/60)} · ${t('上次重启原因','Last reset')}: ${rstName(S.rst)}</div>
${[4,5,6,7,9,14,15].includes(S.rst)&&S.crumb?`<div class="mut">${t('重启前各任务最后在做:','Last thing each task was doing before the reset: ')}${crumbText(S.crumb)}</div>`:''}
<div class="row">${S.setup?`<button class="g" onclick="exitSetup()">${t('退出设置模式','Exit setup mode')}</button>`:''}<button class="g" onclick="act('/api/reboot','${t('正在重启…','Rebooting…')}')">${t('重启','Reboot')}</button><button class="g" onclick="selfTest()">${t('运行自检','Run self-test')}</button><button class="g" onclick="setMute()">${S.mute?t('取消静音','Unmute'):t('静音(不响铃声)','Mute (no alarm sound)')}</button><button class="g" onclick="go('debug')">${t('调试 / 原始数据','Debug / raw data')}</button></div>${S.setup?`<div class="mut">${t('当前处于设置模式(长按设备设置键进入),退出后恢复正常监控。','Setup mode is active (entered by long-pressing the SETUP button). Exit to resume normal monitoring.')}</div>`:''}</div>`},
wifi(){return `<div class="card"><h3>${t('Wi-Fi(仅 2.4GHz)','Wi-Fi (2.4 GHz only)')}</h3>
<div class="row"><label>${t('名称 (SSID)','Network name (SSID)')}</label><input type="text" data-k="ssid" list="nl" autocapitalize="off"></div>
<datalist id="nl">${(nets||[]).map(n=>`<option value="${esc(n.ssid)}">`).join('')}</datalist>
<div class="row"><label>${t('密码','Password')}</label><input type="password" data-k="pass"></div>
<div class="row"><label>${t('设备名 (mDNS)','Device name (mDNS)')}</label><input type="text" data-k="host"></div>
<div class="row"><button class="g" onclick="scan()">${t('扫描附近网络','Scan nearby networks')}</button><span class="mut" id="sc"></span></div>
${nets?nets.map((n,i)=>`<div class="li"><span>${esc(n.ssid)} <span class="mut">${n.rssi} dBm${n.enc?' 🔒':''}</span></span><button class="g" onclick="pick(${i})">${t('选择','Select')}</button></div>`).join(''):''}</div>
<div class="mut">${t('保存后设备会用新的 Wi-Fi 重新连接。','After saving, the device reconnects with the new Wi-Fi.')}</div>
<div class="card"><h3>${t('校时','Clock sync')}</h3>
<div>${S.time?`<span class="ok">${t('已校时','Synced')}</span> · ${esc(S.time)}`:`<span class="bad">${t('未校时','Not synced')}</span>`}</div>
<div class="row"><button class="g" onclick="syncNtp()">${t('立即网络校时 (NTP)','Sync now (NTP)')}</button><button class="g" onclick="syncBrowser()">${t('用这台设备的时间校时','Use this device\'s time')}</button></div>
<div class="mut">${t('设备联网后会自动校时,没成功时每 20 秒重试一次直到成功(服务器含一个 IP 地址,DNS 不稳也能用)。如果路由器挡了 NTP,打开本页时设备还没校时的话会自动用你浏览器的时间校一次;设备没有电池时钟,断电重启后要重新校。时区在「告警」页。','After going online the device syncs automatically and retries every 20 s until it succeeds (one server is an IP address, so flaky DNS does not matter). If your router blocks NTP, opening this page while the device is unsynced sets the clock from your browser once. The device has no battery clock, so it must sync again after every power cycle. The time zone is on the Alerts tab.')}</div></div>
<div class="card"><h3>${t('局域网访问(电脑打开设置页)','LAN access (open this page from a PC)')}</h3>
<div class="sw"><label><input type="checkbox" data-k="lanAccess">${t('允许局域网内的电脑 / 手机直接打开本页(不用按设置键)','Allow computers / phones on the LAN to open this page directly (no need to press the SETUP button)')}</label></div>
<div class="row"><label>${t('访问密码(留空 = 不设密码)','Access password (empty = none)')}</label><input type="password" data-k="webPass" autocomplete="new-password"></div>
<div class="mut">${t('设了密码后,局域网里的设备打开本页要输入:用户名 <code>admin</code> + 这里的密码。连在设备热点上的手机不需要密码。本页里保存着 Wi-Fi 密码和打印机访问码,<b>强烈建议设一个访问密码</b>。长按设备上的设置按钮 1 秒进入设置模式时,局域网内的设备也可免密访问。','With a password set, devices on the LAN must log in with user name <code>admin</code> + this password. Phones on the device hotspot need no password. This page holds your Wi-Fi password and printer access codes, so <b>setting an access password is strongly recommended</b>. While the device is in setup mode (SETUP button held 1 s), LAN devices can also get in without a password.')}</div></div>
<div class="card"><h3>${t('设备热点','Device hotspot')}</h3>
<div class="sw"><label><input type="checkbox" data-k="apAlways">${t('热点常开(始终可见)','Hotspot always on (always visible)')}</label></div>
<div class="row"><label>${t('热点名称','Hotspot name')}</label><input type="text" value="Bambu-FDM-Radar" disabled></div>
<div class="row"><label>${t('热点密码(至少 8 位)','Hotspot password (min. 8 chars)')}</label><input type="text" data-k="apPass"></div>
<div class="mut">${t('手机连上热点后打开 192.168.4.1 即可设置。修改热点密码并保存后,热点会重启,需要用新密码重新连接。强烈建议把默认密码改掉。','After joining the hotspot, open 192.168.4.1. After changing the hotspot password and saving, the hotspot restarts and you must reconnect with the new password. Please change the default password.')}</div></div>`},
printer(){return `<div class="card"><h3>${t('局域网内发现的打印机','Printers found on the LAN')}</h3>${S.discovered.length?S.discovered.map((d,i)=>`<div class="li"><span>${esc(d.name||d.model||'Bambu')} <span class="mut">${esc(d.ip)} · ${esc(d.serial)}</span></span><span>${C.printers.map((_,s)=>`<button class="g" onclick="useP(${i},${s})">→ ${s+1}</button>`).join(' ')}</span></div>`).join(''):`<div class="mut">${t('暂未发现。设备和打印机需在同一局域网;也可以手动填写下面的信息。','None found yet. The device and the printer must be on the same LAN; you can also fill in the details below by hand.')}</div>`}
<div class="row"><button class="g" onclick="refresh()">${t('刷新','Refresh')}</button></div></div>`+
C.printers.map((_,i)=>`<div class="card"><h3>${t('打印机','Printer')} ${i+1}</h3><div class="sw"><label><input type="checkbox" data-k="printers.${i}.enabled">${t('启用','Enabled')}</label></div>
<div class="row"><label>${t('名称','Name')}</label><input type="text" data-k="printers.${i}.name"></div>
<div class="row"><label>${t('IP 地址','IP address')}</label><input type="text" data-k="printers.${i}.ip" inputmode="decimal"></div>
<div class="row"><label>${t('序列号','Serial number')}</label><input type="text" data-k="printers.${i}.serial"></div>
<div class="row"><label>${t('访问码 (Access Code)','Access code')}</label><input type="text" data-k="printers.${i}.code"></div></div>`).join('')},
alarm(){
const days=LANG==='en'?['Su','Mo','Tu','We','Th','Fr','Sa']:['日','一','二','三','四','五','六'];
const g=`<div class="card"><h3>${t('全局','General')}</h3>
<div class="row"><label>${t('音量','Volume')} <span class="rv">${C.volume}</span>%</label><input type="range" min="5" max="100" data-k="volume"></div>
<div class="row"><label>${t('进入"加强"阶段 (秒)','Switch to "Loud" after (s)')}</label><input type="number" min="1" data-k="stage1Sec"></div>
<div class="row"><label>${t('进入"最强"阶段 (秒)','Switch to "Maximum" after (s)')}</label><input type="number" min="2" data-k="stage2Sec"></div>
<div class="row"><label>${t('贪睡时长 (分钟)','Snooze (minutes)')}</label><input type="number" min="1" max="60" data-k="snoozeMin"></div>
<div class="row"><label>${t('失联判定 (秒)','Offline after (s)')}</label><input type="number" min="30" data-k="offlineSec"></div>
<div class="row"><label>${t('设置模式自动退出 (分钟)','Setup mode auto-exit (minutes)')}</label><input type="number" min="1" max="120" data-k="setupTimeoutMin"></div>
<div class="sw"><label><input type="checkbox" data-k="selfTest">${t('开机自检(数码管全亮、灯环转一圈、喇叭响一声)','Power-on self-test (all digits lit, LED ring sweep, a chime)')}</label><label><input type="checkbox" data-k="selfTestSound">${t('自检时出声','Self-test makes sound')}</label></div>
<div class="row"><label>${t('手动取消打印时的提示音(不算告警)','Short sound when a print is cancelled by hand (not an alarm)')}</label><select data-k="cancelSound"><option value="">${t('无(不出声)','None (silent)')}</option>${(S.sounds||[]).map(s=>`<option value="${esc(s.name)}">${esc(s.name)}</option>`).join('')}</select></div>
<div class="row"><label>${t('“只响一下”的错误码使用的提示音','Sound for error codes set to "Just one beep"')}</label><select data-k="noticeSound">${(S.sounds||[]).map(s=>`<option value="${esc(s.name)}">${esc(s.name)}</option>`).join('')}</select></div>
<div class="mut">${t('手动取消打印不会触发告警(需勾选「错误码」页的“手动取消打印不算失败 / 错误”);这里可以选一个简短的提示音,取消时只响一次,静音模式下不响。','A manual cancel never raises an alarm (the "A manual cancel is not a failure / error" box on the Error codes tab must be ticked). You can pick a short sound that plays once when it happens; it stays silent in mute mode.')}</div></div>
<div class="card"><h3>${t('免打扰时段','Do not disturb')}</h3><div class="sw"><label><input type="checkbox" data-k="quietEnabled">${t('启用','Enabled')}</label></div>
<div class="row"><label>${t('开始时间','Start time')}</label><input type="time" data-k="quietStart"></div>
<div class="row"><label>${t('结束时间','End time')}</label><input type="time" data-k="quietEnd"></div>
<div class="row"><label>${t('生效的星期(按开始当天算)','Active days (by the day the window starts)')}</label></div>
<div class="sw">${days.map((n,d)=>`<label><input type="checkbox" ${(C.quietDays>>d)&1?'checked':''} onchange="setDay(${d},this.checked)">${n}</label>`).join('')}</div>
<div class="row"><label>${t('时区 (POSIX)','Time zone (POSIX)')}</label><input type="text" data-k="tz"></div>
<div class="mut">${t('设备当前时间:','Device time: ')}${S.time||t('未校时(联网后自动校时;校时前免打扰不生效)','not synced (syncs automatically once online; do-not-disturb is inactive until then)')}</div><div class="mut">${t('免打扰期间,只有勾选"免打扰时段仍响"的事件会响,其余事件只记入「记录」页。结束时间早于开始时间表示跨午夜。手机推送(「通知」页)不受免打扰影响。','During the window only events with "Ring during do-not-disturb" will ring; the rest are only logged. An end time earlier than the start time means the window crosses midnight. Phone push (Notify tab) is not affected.')}</div></div>
<div class="card"><h3>${t('进度灯(灯环)','Progress light (LED ring)')}</h3>
<div class="sw"><label><input type="checkbox" data-k="ledProgress">${t('显示打印进度','Show print progress')}</label><label><input type="checkbox" data-k="ledQuietOff">${t('免打扰时段内关闭(灯环和时钟)','Off during do-not-disturb (ring and clock)')}</label></div>
<div class="row"><label>${t('显示方式','Mode')}</label><select data-n="1" data-k="ledMode"><option value="0">${t('多台分段显示(自动)','Segments for all active printers (auto)')}</option><option value="1">${t('只显示指定打印机','Only the selected printer')}</option></select></div>
<div class="row"><label>${t('指定的打印机','Selected printer')}</label><select data-n="1" data-k="ledPrinter">${C.printers.map((p,i)=>`<option value="${i}">${t('打印机','Printer')} ${i+1}${p.name?' · '+esc(p.name):''}</option>`).join('')}</select></div>
<div class="row"><label>${t('亮度','Brightness')} <span class="rv">${C.ledBright}</span>% ${t('(0 = 关)','(0 = off)')}</label><input type="range" min="0" max="100" data-k="ledBright"></div>
<div class="row"><label>${t('待机 / 等待灯亮度','Idle / waiting light')} <span class="rv">${C.ledIdle}</span>% ${t('(0 = 关)','(0 = off)')}</label><input type="range" min="0" max="100" data-k="ledIdle"></div>
<div class="row"><label>${t('完成灯亮度','Finished bar')} <span class="rv">${C.ledDone}</span>% ${t('(0 = 关)','(0 = off)')}</label><input type="range" min="0" max="100" data-k="ledDone"></div>
<div class="mut">${t('颜色:蓝=打印中,橙=暂停,红=失败/错误,绿=完成(保持 30 分钟)。多台同时有任务时灯环自动平分;"只显示指定打印机"时只显示选中的那台。响铃和设置模式优先于进度显示。','Colors: blue = printing, orange = paused, red = failed / error, green = finished (kept for 30 min). With several active printers the ring is split evenly; "Only the selected printer" shows just that one. Ringing and setup mode take priority over progress.')}</div>
<div class="sw"><label><input type="checkbox" data-k="muteClock">${t('静音模式下闹钟也静音','Mute mode also silences the alarm clock')}</label></div>
<div class="mut">${t('设备上的 SETUP 按钮:按住 1 到 3 秒松开 = 进入 / 退出设置模式;一直按住满 3 秒 = 进入 / 退出静音模式(按住 1 秒时灯环变蓝,满 3 秒变紫)。静音模式下灯环保持原来的进度显示但变成呼吸效果,告警不出声(灯环、数码管、记录和手机推送照常),重启后恢复;状态页也能开关静音。确认键在没响铃时按住约 0.7 秒:数码管显示打印进度(1-50 = 1 号机 50%,dn = 已完成,oF = 失联,---- = 没有任务,多台轮流显示),松开 5 秒后回到时间。','SETUP button on the device: hold 1 to 3 s and release = setup mode on / off; keep holding for 3 s = mute mode on / off (the ring turns blue at 1 s and violet at 3 s). In mute mode the ring keeps its normal progress picture but breathes, and alarms make no sound (ring, clock, log and phone push still work). It resets on reboot; you can also toggle it on the status page. With nothing ringing, hold the ACK button for about 0.7 s: the digits show print progress (1-50 = printer 1 at 50%, dn = finished, oF = offline, ---- = no job; several printers take turns) and return to the time 5 s after you let go.')}</div></div>
<div class="card"><h3>${t('数码管时钟 (TM1637)','Digit clock (TM1637)')}</h3>
<div class="sw"><label><input type="checkbox" data-k="clockEnabled">${t('启用','Enabled')}</label></div>
<div class="row"><label>${t('亮度','Brightness')} <span class="rv">${C.clockBright}</span> (0-7)</label><input type="range" min="0" max="7" data-k="clockBright"></div>
<div class="mut">${t('平时显示时间(冒号每秒闪一下)。告警时显示「事件字母 - 打印机编号」,编号会闪烁:F=失败、d=完成、P=暂停、E=错误、o=失联,例如 <code>F-2</code> 表示 2 号打印机失败。接线:VCC→3V3、GND、CLK→GPIO8、DIO→GPIO9。','Shows the time normally (colon blinks every second). During an alarm it shows "event letter - printer number" with the number flashing: F = failed, d = done, P = paused, E = error, o = offline, e.g. <code>F-2</code> means printer 2 failed. Wiring: VCC→3V3, GND, CLK→GPIO8, DIO→GPIO9.')}</div></div>`;
const opts=(S.sounds||[]).map(s=>`<option value="${esc(s.name)}">${esc(s.name)}</option>`).join('');
return g+EV().map(([k,n])=>`<div class="card"><h3>${n}</h3>
<div class="sw"><label><input type="checkbox" data-k="rules.${k}.enabled">${t('启用','Enabled')}</label><label><input type="checkbox" data-k="rules.${k}.speaker">${t('喇叭','Speaker')}</label><label><input type="checkbox" data-k="rules.${k}.buzzer">${t('警报器','Buzzer')}</label><label><input type="checkbox" data-k="rules.${k}.overrideQuiet">${t('免打扰时段仍响','Ring during do-not-disturb')}</label></div>
<div class="row"><label>${t('起始强度','Start level')}</label><select data-n="1" data-k="rules.${k}.startStage">${STG().map((s,i)=>`<option value="${i}">${s}</option>`).join('')}</select></div>
<div class="row"><label>${t('音效','Sound')}</label><select data-k="rules.${k}.sound">${opts}</select></div>
<div class="row"><label>${t('自动停止 (秒, 0=按键才停)','Auto-stop (s, 0 = until button)')}</label><input type="number" min="0" data-k="rules.${k}.autoStopSec"></div>
${HINT(k)?`<div class="mut">${HINT(k)}</div>`:''}
<div class="row"><button class="g" onclick="testEv('${k}')">${t('测试此告警(用已保存设置)','Test this alert (saved settings)')}</button></div></div>`).join('')},
err(){
const rows=C.errCodes.map((e,i)=>`<div class="li" style="flex-wrap:wrap"><input type="text" data-k="errCodes.${i}.code" placeholder="${t('错误码(十六进制)','Error code (hex)')}" style="flex:1 1 170px"><select data-n="1" data-k="errCodes.${i}.action" style="flex:0 1 120px"><option value="2">${t('仅备注','Label only')}</option><option value="0">${t('忽略','Ignore')}</option><option value="1">${t('一律响铃','Always ring')}</option><option value="3">${t('持续告警(按键才停)','Ring until button')}</option><option value="4">${t('只响一下','Just one beep')}</option></select><input type="text" data-k="errCodes.${i}.note" placeholder="${t('备注','Note')}" maxlength="15" style="flex:1 1 110px"><button class="d" onclick="delCode(${i})">${t('删','Del')}</button></div>`).join('');
return `<div class="card"><h3>${t('错误 / HMS 告警过滤','Error / HMS filter')}</h3>
<div class="row"><label>${t('HMS 响铃门槛','HMS ring threshold')}</label><select data-n="1" data-k="hmsSev"><option value="1">${t('仅致命','Fatal only')}</option><option value="2">${t('严重及以上(推荐)','Serious and above (recommended)')}</option><option value="3">${t('一般及以上','Common and above')}</option><option value="4">${t('全部(含提示)','All (incl. info)')}</option></select></div>
<div class="sw"><label><input type="checkbox" data-k="ignoreAms">${t('忽略 AMS 相关提示(致命级除外)','Ignore AMS notices (except fatal)')}</label><label><input type="checkbox" data-k="ignoreCancel">${t('手动取消打印不算失败 / 错误','A manual cancel is not a failure / error')}</label></div>
<div class="mut">${t('<b>HMS</b> 是打印机自己的"健康管理系统"通知(Bambu Handy 里看到的那些警告);<b>AMS</b> 是自动换料器,它的提示很多(烘干、湿度、耗材状态、RFID 读取等),级别低的多半不需要把人叫醒。门槛以下的 HMS 不响铃、不推送,只记入「记录」页;想知道某次误报的代码,在「记录」页点「忽略此码」即可。','<b>HMS</b> is the printer\'s own "Health Management System" (the warnings you see in Bambu Handy). <b>AMS</b> is the automatic material system; it raises many notices (drying, humidity, filament state, RFID read, ...) and low-severity ones rarely need to wake anyone. HMS below the threshold does not ring or push; it is only written to the Log tab. To silence a false alarm, press "Ignore this code" on the Log tab.')}</div></div>
<div class="card"><h3>${t('自定义错误码','Custom error codes')}</h3>${rows||`<div class="mut">${t('还没有自定义规则','No custom rules yet')}</div>`}
<div class="row"><button class="g" onclick="addCode()">+ ${t('添加','Add')}</button></div>
<div class="mut">${t('「仅备注」= 只给这个码起个名字(显示在记录和推送里),响不响铃仍按上面的规则;「忽略」= 这个码永远不响铃;「一律响铃」= 即使低于 HMS 门槛也响铃;「持续告警」= 响铃并一直响到你按设备上的确认键(不自动停止,打印机把错误清掉也不会停),适合热床异物这类必须处理的错误。「只响一下」= 不算告警:只播放一次提示音(不持续、不推送),适合喷嘴摄像头脏了这类提醒一下就行的事。格式:打印机错误码 8 位,如 <code>0300400C</code>;HMS 16 位,如 <code>0700200000020001</code>(写成 <code>HMS_0700_2000_0002_0001</code> 也行)。最多 40 条,改完点「保存设置」。','"Label only" = just names the code (shown in the log and in pushes); ringing still follows the rules above. "Ignore" = this code never rings. "Always ring" = rings even below the HMS threshold; "Ring until button" = rings and keeps ringing until you press the ACK button on the device (no auto-stop, and it does not stop when the printer clears the error), meant for errors that must be dealt with, like a foreign object on the bed. "Just one beep" = not an alarm: plays one short sound (no repeat, no push), for things that only need a nudge, like a dirty nozzle camera. Format: printer error codes are 8 hex digits, e.g. <code>0300400C</code>; HMS codes are 16, e.g. <code>0700200000020001</code> (<code>HMS_0700_2000_0002_0001</code> also works). Max. 40 entries; press "Save settings" when done.')}</div></div>
<div class="card"><h3>${t('批量导入 / 导出','Bulk import / export')}</h3>
<textarea rows="7" oninput="ERRTXT=this.value" spellcheck="false" placeholder="${t('每行一条,可直接粘贴多条,例如:\n0500806E 热床上有异物\n83918991=摄像头清理暂停\n0300A70000030001,忽略,换料','One entry per line; paste as many as you like, e.g.:\n0500806E Foreign object on the bed\n83918991=Camera cleaning pause\n0300A70000030001,ignore,AMS filament change')}" style="width:100%;background:#0d141c;color:var(--fg);border:1px solid var(--line);border-radius:10px;padding:8px 10px;font:13px/1.5 ui-monospace,Consolas,monospace">${esc(ERRTXT)}</textarea>
<div class="row"><label>${t('没写动作时默认','Default action if none given')}</label><select onchange="ERRDEF=+this.value"><option value="2" ${ERRDEF===2?'selected':''}>${t('仅备注','Label only')}</option><option value="0" ${ERRDEF===0?'selected':''}>${t('忽略','Ignore')}</option><option value="1" ${ERRDEF===1?'selected':''}>${t('一律响铃','Always ring')}</option><option value="3" ${ERRDEF===3?'selected':''}>${t('持续告警(按键才停)','Ring until button')}</option><option value="4" ${ERRDEF===4?'selected':''}>${t('只响一下','Just one beep')}</option></select></div>
<div class="row"><button class="b" onclick="importErr()">${t('导入(自动去重)','Import (de-duplicates)')}</button><button class="g" onclick="exportErr()">${t('导出当前列表到文本框','Export list to the box')}</button><button class="g" onclick="copyErr()">${t('复制文本框','Copy box')}</button></div>
<div class="mut">${ERRRES}</div>
<div class="mut">${t('格式:错误码 + 分隔符(空格 / 逗号 / 等号 / Tab 都行) + 可选的动作(忽略 / 一律响铃 / 仅备注,英文 ignore / ring / note 也行) + 备注。错误码可以写十六进制(8 位或 16 位,可带 HMS_ / 0x 前缀),也可以直接粘贴 Bambu 报文里的十进制数字(不以 0 开头的 7 到 10 位纯数字会按十进制转换,如 83918991 → 0500808F)。已存在的码会合并:写了动作或备注就更新,否则跳过;识别不了的行留在文本框里。# 开头的行会被忽略。导入后记得点「保存设置」。','Format: error code + separator (space / comma / equals / tab) + optional action (ignore / ring / note, or the Chinese words) + note. Codes can be hex (8 or 16 digits, optional HMS_ / 0x prefix), or the decimal number from the Bambu payload (a plain 7 to 10 digit number not starting with 0 is converted from decimal, e.g. 83918991 → 0500808F). Codes that already exist are merged: updated if an action or note is given, otherwise skipped. Lines that cannot be read stay in the box. Lines starting with # are ignored. Press "Save settings" after importing.')}</div></div>`},
notify(){
const nt=NT(),nf=NF();
return C.notify.map((n,i)=>{const f=nf[n.type]||nf[0];
const row=(k,d)=>d?`<div class="row"><label>${d[0]}</label><input type="text" data-k="notify.${i}.${k}" placeholder="${esc(d[1]||'')}"></div>`:'';
const last=S.notifyLast?S.notifyLast[i]:0;
const st=!last?'':(last>=200&&last<300?`<span class="ok">${t('上次发送成功','Last send OK')}</span>`:`<span class="bad">${t('上次失败(代码 ','Last send failed (code ')}${last})</span>`);
return `<div class="card"><h3>${t('通知通道','Channel')} ${i+1}</h3><div class="sw"><label><input type="checkbox" data-k="notify.${i}.enabled">${t('启用','Enabled')}</label></div>
<div class="row"><label>${t('类型','Type')}</label><select data-n="1" data-r="1" data-k="notify.${i}.type">${nt.map((x,j)=>`<option value="${j}">${x}</option>`).join('')}</select></div>
<div class="row"><label>${t('备注名','Label')}</label><input type="text" data-k="notify.${i}.name"></div>
${row('url',f.url)}${row('key',f.key)}${row('extra',f.extra)}
<div class="mut">${t('需要推送的事件:','Events to push:')}</div>
<div class="sw">${EV().map(([k,nm],b)=>`<label><input type="checkbox" ${(n.evMask>>b)&1?'checked':''} onchange="setEvBit(${i},${b},this.checked)">${nm}</label>`).join('')}</div>
<div class="row"><button class="g" onclick="testNotify(${i})">${t('发送测试(用已保存的设置)','Send test (saved settings)')}</button>${st}</div></div>`}).join('')+
`<div class="mut">${t('改完设置先点「保存设置」,再点「发送测试」。上次失败代码:-1 = 设备没连上 Wi-Fi;-2 = 地址不是 http(s)://;-4 = 必填项没填;其余负数 = 连接失败 / 超时;正数 = 对方服务器返回的 HTTP 状态码。<br>自定义 Webhook 会 POST 一段 JSON:<code>{event, event_name, printer, detail, title, message, time}</code>,n8n 的 Webhook 节点可直接接收。飞书 / 钉钉机器人如果开了「签名」校验暂不支持,请改用「关键词」校验。推送在免打扰时段内也会发送。','Press "Save settings" first, then "Send test". Failure codes: -1 = device not on Wi-Fi; -2 = URL is not http(s)://; -4 = a required field is empty; other negatives = connection failed / timeout; positive = HTTP status returned by the server.<br>A custom webhook POSTs JSON: <code>{event, event_name, printer, detail, title, message, time}</code>, which an n8n Webhook node can receive directly. Feishu / DingTalk bots with "signature" verification are not supported; use "keyword" verification instead. Pushes are also sent during do-not-disturb.')}</div>`},
log(){
if(!LOG)return `<div class="mut">${t('加载中…','Loading…')}</div>`;
const rows=LOG.map(e=>{const ev=EV().find(x=>x[0]===e.ev);const evn=ev?ev[1]:e.ev;
const pn=(C.printers[e.p]&&C.printers[e.p].name)||(t('打印机 ','Printer ')+(e.p+1));
const tag=(e.f&64)?`<span class="mut">${t('手动测试告警(网页上的测试按钮,不是真实事件)','Manual test alarm (web page test button, not a real event)')}${(e.f&2)?t(' · 未响铃(静音 / 免打扰 / 另一个告警正在响)',' · did not ring (muted / do-not-disturb / another alarm active)'):''}</span>`:(e.f&32)?`<span class="mut">${t('只响了一声提示音(不算告警)','Played one short sound (not an alarm)')}</span>`:(e.f&1)?`<span class="mut">${t('已过滤:未响铃、未推送','Filtered: no ring, no push')}</span>`:((e.f&2)?`<span class="mut">${t('未响铃(已禁用 / 免打扰 / 另一个告警正在响)','Did not ring (disabled / do-not-disturb / another alarm active)')}</span>`:((e.f&4)?`<span class="mut">${t('告警已自动停止:打印机已清除问题 / 已恢复','Alarm stopped by itself: the printer cleared the problem / recovered')}</span>`:((e.f&8)?`<span class="mut">${t('告警已自动停止:到了自动停止时间','Alarm stopped by itself: auto-stop time reached')}</span>`:((e.f&16)?`<span class="mut">${t('告警被确认键或网页停止了','Alarm stopped / snoozed with the ACK button or the web page')}</span>`:''))));
const isC=e.ev==='clock';const known=isC||(e.code&&C.errCodes.some(c=>c.code===e.code));
return `<div class="li"><span>${esc(e.t||t('(未校时)','(time not synced)'))} ${isC?'':'· '+esc(pn)+' '}· <b>${esc(evn)}</b> ${e.code?(isC?esc(e.code):'<code>'+esc(e.code)+'</code>'):''} ${esc(e.note)}<br>${tag}</span><span>${e.code&&!known?`<button class="g" onclick="ignoreCode('${esc(e.code)}')">${t('忽略此码','Ignore this code')}</button>`:''}</span></div>`}).join('');
return `<div class="card"><h3>${t('最近告警记录(最多 30 条,新的在上)','Recent alerts (max. 30, newest first)')}</h3>${rows||`<div class="mut">${t('暂无记录','No entries yet')}</div>`}
<div class="row"><button class="g" onclick="loadLog()">${t('刷新','Refresh')}</button><button class="d" onclick="clearLog()">${t('清空','Clear')}</button></div></div>
<div class="mut">${t('灰色「已过滤」的是被错误码规则拦下的,没有响铃也没有推送;如果其中有你其实想要的,到「错误码」页调整门槛或设为「一律响铃」。','Entries marked "Filtered" were stopped by the error-code rules: no ring, no push. If one of them is something you do want, adjust the threshold on the Error codes tab or set it to "Always ring".')}</div>`},
clock(){
const days=LANG==='en'?['Su','Mo','Tu','We','Th','Fr','Sa']:['日','一','二','三','四','五','六'];
const rows=C.clocks.map((a,i)=>`<div class="card"><div class="sw"><label><input type="checkbox" data-k="clocks.${i}.enabled">${t('启用','Enabled')}</label><button class="d" style="margin-left:auto" onclick="delClock(${i})">${t('删','Del')}</button></div>
<div class="row"><label>${t('时间','Time')}</label><input type="time" data-k="clocks.${i}.time"></div>
<div class="row"><label>${t('名称','Label')}</label><input type="text" maxlength="22" data-k="clocks.${i}.label" placeholder="${t('例如:起床','e.g. Wake up')}"></div>
<div class="sw">${days.map((n,d)=>`<label><input type="checkbox" ${(a.days>>d)&1?'checked':''} onchange="setClockDay(${i},${d},this.checked)">${n}</label>`).join('')}</div>
<div class="row"><button class="g" onclick="clockPreset(${i},127)">${t('每天','Every day')}</button><button class="g" onclick="clockPreset(${i},62)">${t('工作日','Weekdays')}</button><button class="g" onclick="clockPreset(${i},65)">${t('周末','Weekends')}</button><button class="g" onclick="clockPreset(${i},0)">${t('单次','Once')}</button></div>
${a.days===0?`<div class="mut">${t('单次:响过一次后自动关闭','Once: switches itself off after ringing')}</div>`:''}</div>`).join('');
return (rows||`<div class="card"><div class="mut">${t('还没有闹钟。','No alarm clocks yet.')}</div></div>`)+
`<div class="row"><button class="g" onclick="addClock()">+ ${t('添加闹钟','Add alarm clock')}</button><button class="g" onclick="testEv('clock')">${t('试响一下(用已保存设置)','Test ring (saved settings)')}</button></div>
<div class="mut">${t('闹钟按设备当前时间响(必须已校时,时区在「告警」页;当前时间:','Alarm clocks follow the device time (it must be synced; time zone is on the Alerts tab; current time: ')}${S.time||t('未校时','not synced')}${t(')。最多 6 个。不勾选任何星期 = 单次。铃声、强度和自动停止时间在「告警」页的「闹钟」卡片里设置,默认无视免打扰。响铃时按设备上的按钮可以贪睡或停止;如果该分钟正好有打印机告警在响,闹钟会等它结束后再响(仅限同一分钟内)。数码管时钟会整屏闪烁。',') . Up to 6. Tick no weekday = once. Sound, strength and auto-stop are set in the "Alarm clock" card on the Alerts tab; it ignores do-not-disturb by default. While it rings, the device button snoozes or stops it. If a printer alert is ringing at that minute, the clock waits for it to end (within the same minute). The digit clock flashes the whole time.')}</div>`},
debug(){
const en=S.cfg.printers.map((c,i)=>c.enabled?i:-1).filter(i=>i>=0);
if(!en.length)return `<div class="card"><div class="mut">${t('还没有启用打印机。','No printer enabled yet.')}</div></div>`;
if(!en.includes(DBG.i))DBG.i=en[0];
const sel=k=>DBG.kind===k?'selected':'';
return `<div class="card"><h3>${t('调试 · 原始数据','Debug · raw data')}</h3>
<div class="row"><label>${t('打印机','Printer')}</label><select onchange="DBG.i=+this.value;loadDbg()">${en.map(i=>`<option value="${i}" ${i===DBG.i?'selected':''}>${i+1}. ${esc(S.cfg.printers[i].name||S.cfg.printers[i].ip)}</option>`).join('')}</select></div>
<div class="row"><label>${t('内容','Content')}</label><select onchange="DBG.kind=this.value;loadDbg()"><option value="snap" ${sel('snap')}>${t('最近一次完整快照','Latest full snapshot')}</option><option value="msg" ${sel('msg')}>${t('最新一条消息(可能只含变化的字段)','Latest message (may hold only changed fields)')}</option><option value="parsed" ${sel('parsed')}>${t('固件解析结果','What the firmware parsed')}</option></select></div>
<div class="row"><label>${t('过滤(只显示含此文字的行)','Filter (only lines containing this text)')}</label><input type="text" value="${esc(DBG.f)}" oninput="DBG.f=this.value;drawDbg()" placeholder="temp / extruder / hms ..."></div>
<div class="sw"><label><input type="checkbox" ${DBG.auto?'checked':''} onchange="DBG.auto=this.checked">${t('自动刷新(3 秒)','Auto-refresh (3 s)')}</label></div>
<div class="row"><button class="g" onclick="loadDbg()">${t('刷新','Refresh')}</button><button class="g" onclick="copyDbg()">${t('复制全部','Copy all')}</button><button class="g" onclick="dlDbg()">${t('下载','Download')}</button></div>
<div class="mut" id="dbgi"></div>
<pre id="dbgp" style="max-height:60vh;overflow:auto;white-space:pre-wrap;word-break:break-all;font-size:12px;background:#0d141c;border:1px solid var(--line);border-radius:10px;padding:10px;margin:8px 0 0"></pre></div>
<div class="mut">${t('「完整快照」= 打印机最近一次发来的大报文(设备每 30 秒主动请求一次);「最新一条消息」通常只含刚变化的字段。比对「固件解析结果」和快照里的字段,就能看出哪个值没读对。报文不含访问码,但包含序列号等信息,贴到公开场合前请先检查。','"Full snapshot" = the latest large message from the printer (the device requests one every 30 s); "Latest message" usually holds only the fields that just changed. Compare "What the firmware parsed" with the snapshot to spot a value that is read wrongly. The payload does not contain the access code but does contain the serial number and similar, so check it before posting it publicly.')}</div>`},
fw(){return `<div class="card"><h3>${t('固件版本','Firmware version')}</h3><div>${t('当前版本','Current version')}: <b>${esc(S.fw||'?')}</b> <span class="mut" id="fwslot"></span></div>
<div class="mut">${t('更新固件不会改动你的设置、音效和错误码文本。更新过程中请勿断电;设备正在响铃时不能更新;写入约需一两分钟,完成后设备自动重启。','Updating the firmware keeps your settings, sounds and error texts. Do not power off during the update. It cannot start while an alarm is ringing. Writing takes a minute or two, then the device restarts by itself.')}</div></div>
<div class="card" id="fwst" style="display:none"></div>
<div class="card"><h3>${t('从 GitHub 更新','Update from GitHub')}</h3>
<div class="row"><label>${t('更新源','Update source')}</label><input type="text" data-k="ghRepo" placeholder="owner/repo" autocomplete="off" spellcheck="false"></div>
<div class="mut">${t('填 <code>用户名/仓库名</code>,设备会读取该仓库「最新 Release」里的 <code>version.json</code> 和 <code>firmware.bin</code>(仓库要公开)。也可以填你自己服务器上的完整网址文件夹(以 / 结尾,里面放这两个文件)。点「检查更新」时会先保存这一项。','Enter <code>user/repo</code>; the device reads <code>version.json</code> and <code>firmware.bin</code> from the latest Release of that repo (the repo must be public). You can also enter the full URL of a folder on your own server (ending in /, holding those two files). "Check for updates" saves this field first.')}</div>
<div class="row"><button class="g" id="fwchk" onclick="fwCheck()">${t('检查更新','Check for updates')}</button><button class="b" id="fwins" onclick="fwInstall()" disabled>${t('安装更新','Install update')}</button></div></div>
<div class="card"><h3>${t('上传固件文件','Upload a firmware file')}</h3>
<div class="mut">${t('选择编译出来的 <code>firmware.bin</code>(PlatformIO 在 <code>.pio/build/esp32s3/firmware.bin</code>,或从 GitHub Release 下载)。文件头会先检查是否是 ESP32-S3 的固件,不对就拒绝,不会写入。','Pick the compiled <code>firmware.bin</code> (PlatformIO puts it in <code>.pio/build/esp32s3/firmware.bin</code>, or download it from a GitHub Release). The file header is checked first: anything that is not an ESP32-S3 image is rejected and nothing is written.')}</div>
<div class="row"><input type="file" id="fwfile" accept=".bin"><button class="b" id="fwup" onclick="fwUpload()">${t('上传并更新','Upload and update')}</button></div></div>`},
sound(){return `<div class="card"><h3>${t('音效文件','Sound files')}</h3>${(S.sounds||[]).map(s=>`<div class="li"><span>${esc(s.name)} <span class="mut">${(s.size/1024)|0} KB</span></span><span><button class="g" onclick="prev('${esc(s.name)}')">${t('网页试听','Play here')}</button> <button class="g" onclick="testSp('${esc(s.name)}')">${t('设备试听','Play on device')}</button> <button class="d" onclick="delS('${esc(s.name)}')">${t('删','Del')}</button></span></div>`).join('')||`<div class="mut">${t('无','None')}</div>`}</div>
<div class="card"><h3>${t('上传新音效','Upload a sound')}</h3><div class="mut">${t('支持 mp3 / wav / m4a / ogg 等浏览器能解码的格式,会在浏览器里转成 16kHz 单声道 WAV 再上传。建议 60 秒以内。语音提示(如"打印失败了")可以自己录音或用 TTS 生成后上传。','Any format your browser can decode (mp3 / wav / m4a / ogg ...); it is converted to 16 kHz mono WAV in the browser before upload. Keep it under 60 s. Voice prompts (e.g. "print failed") can be recorded yourself or generated with TTS.')}</div>
<div class="row"><input type="file" id="f" accept="audio/*"><button class="b" onclick="upl()">${t('上传','Upload')}</button></div></div>
<div class="card"><h3>${t('警报器','Buzzer')}</h3><button class="g" onclick="testBz()">${t('测试警报器 3 秒','Test buzzer for 3 s')}</button><div class="mut">${t('按下后设备串口日志会打印 <code>Test buzzer</code>。日志有、但没声音 = 硬件问题(MOSFET 接线 / 蜂鸣器类型,有源蜂鸣器要把 pins.h 里 BUZZER_ACTIVE 改成 1)。没装蜂鸣器的话,把各告警里的「警报器」取消勾选即可。','The serial log prints <code>Test buzzer</code> when pressed. Log line but no sound = hardware issue (MOSFET wiring / buzzer type; for an active buzzer set BUZZER_ACTIVE to 1 in pins.h). No buzzer installed? Untick "Buzzer" in every alert.')}</div></div>`}
};
function render(){
  $('#nav').innerHTML=TABS().map(([k,n])=>`<button class="${k===tab?'on':''}" onclick="go('${k}')">${n}</button>`).join('');
  $('#v').innerHTML=V[tab]();bind();if(tab==='debug')drawDbg();if(tab==='fw')fwDraw();
  const hs=$('#hs');if(S){if(S.ringing){hs.className='chip bad';hs.textContent=t('报警中','ALARM')}else{hs.className=S.mute?'chip warn':'chip ok';hs.textContent=t('监控中','Monitoring')+(S.mute?' · '+t('静音','Muted'):'')}}}
function addClock(){if(C.clocks.length>=6)return toast(t('最多 6 个','Max. 6'));C.clocks.push({enabled:true,time:'07:00',days:127,label:''});render()}
function delClock(i){C.clocks.splice(i,1);render()}
function setClockDay(i,d,on){const a=C.clocks[i];a.days=on?(a.days|(1<<d)):(a.days&~(1<<d));render()}
function clockPreset(i,m){C.clocks[i].days=m;render()}
function go(x){tab=x;render();if(x==='log')loadLog();if(x==='debug')loadDbg();if(x==='fw')loadFw()}
async function loadDbg(quiet){
  if(DBG.kind==='parsed'){DBG.text=JSON.stringify(S.printers[DBG.i],null,1);DBG.bad=false;if(!quiet)await refresh();return drawDbg()}
  try{
    const r=await fetch('/api/raw?i='+DBG.i+'&kind='+DBG.kind);const x=await r.text();
    let o=null;try{o=JSON.parse(x)}catch(e){}
    DBG.bad=!o&&x.length>2;
    DBG.text=o&&Object.keys(o).length?JSON.stringify(o,null,1):(DBG.bad?x:'');
  }catch(e){DBG.text='';DBG.bad=false}
  if(quiet)drawDbg();else await refresh();
}
function drawDbg(){
  const pre=$('#dbgp');if(!pre)return;
  let lines=(DBG.text||'').split('\n');const total=lines.length;
  const f=DBG.f.trim().toLowerCase();
  if(f)lines=lines.filter(l=>l.toLowerCase().indexOf(f)>=0);
  const cut=lines.length>3000;if(cut)lines=lines.slice(0,3000);
  pre.textContent=DBG.text?lines.join('\n'):t('(暂无数据:打印机还没发来消息,或还没有完整快照。等几秒点刷新)','(No data yet: the printer has not sent a message, or no full snapshot has arrived. Wait a few seconds and press Refresh.)');
  const p=S.printers[DBG.i];
  const len=DBG.kind==='snap'?p.snapLen:p.rawLen,age=DBG.kind==='snap'?p.snapAge:p.rawAge;
  const info=$('#dbgi');
  if(info)info.textContent=(DBG.kind!=='parsed'&&len?(len/1024).toFixed(1)+' KB · '+age+t(' 秒前',' s ago'):'')+(f?' · '+lines.length+'/'+total+t(' 行匹配',' lines match'):'')+(cut?t(' · 只显示前 3000 行',' · first 3000 lines only'):'')+(DBG.bad?t(' · 报文正在更新,请再点刷新',' · payload was being updated, press Refresh'):'');
}
function copyDbg(){const ta=document.createElement('textarea');ta.value=DBG.text||'';document.body.appendChild(ta);ta.select();let ok=false;try{ok=document.execCommand('copy')}catch(e){}document.body.removeChild(ta);toast(ok?t('已复制','Copied'):t('复制失败,请用下载','Copy failed, use Download'))}
function dlDbg(){const b=new Blob([DBG.text||''],{type:'application/json'});const a=document.createElement('a');a.href=URL.createObjectURL(b);a.download='printer'+(DBG.i+1)+'_'+DBG.kind+'.json';a.click();setTimeout(()=>URL.revokeObjectURL(a.href),2000)}
async function refresh(){const d=await api('/api/state');if(d){S=d;if(!C){C=d.cfg;LANG=C.lang===1?'en':'zh';applyStatic()}if(!S.time&&!refresh.sent){refresh.sent=1;api('/api/time/set?ts='+Math.floor(Date.now()/1000),{method:'POST'}).then(()=>setTimeout(refresh,600))}render()}}
// ---- firmware update ----
const FW={st:null,timer:null,reboot:false,lost:false,saved:null};
function fwErr(k){k=k||'';
 if(k.indexOf('http:')===0){const c=k.slice(5);return t('服务器返回 HTTP ','Server answered HTTP ')+c+(c==='404'?t('(没找到文件:还没发布 Release、仓库名写错,或仓库是私有的)',' (file not found: no Release published yet, wrong repo name, or the repo is private)'):'')}
 if(k.indexOf('net')===0)return t('网络错误或超时','Network error or timeout');
 return ({busy:t('正在检查或更新中','A check or update is already running'),ringing:t('设备正在响铃,等响铃结束再试','An alarm is ringing, try again when it stops'),norepo:t('还没有填写更新源','No update source entered'),badrepo:t('更新源格式不对,应为 用户名/仓库名 或完整网址','Bad update source: use user/repo or a full URL'),wifi:t('Wi-Fi 未连接','Wi-Fi is not connected'),memory:t('设备内存不足,稍后重试','Not enough free memory, try again later'),json:t('version.json 格式不对','version.json is not valid'),size:t('固件大小不符或超出分区','Firmware size does not match or does not fit'),magic:t('这不是有效的固件文件','This is not a valid firmware file'),chip:t('这个固件不是为 ESP32-S3 编译的','This firmware was not built for the ESP32-S3'),write:t('写入闪存失败','Writing to flash failed'),hash:t('校验失败(sha256 不一致),已放弃,没有写入','Checksum mismatch (sha256), update dropped'),end:t('固件校验未通过,已放弃','The firmware image was not accepted')})[k]||k}
function fwDraw(){
 const f=FW.st||{},st=f.state||'idle',el=$('#fwst');if(!el)return;
 const run=st==='checking'||st==='downloading'||st==='uploading'||st==='done'||FW.lost;
 let m='',bar='';
 if(FW.lost)m=t('设备正在重启,等它回来…','The device is restarting, waiting for it to come back…');
 else if(st==='checking')m=t('正在检查更新…','Checking for updates…');
 else if(st==='uptodate')m='✅ '+t('已是最新版本 ','Already up to date ')+'('+esc(f.remote)+')';
 else if(st==='available')m='🆕 '+t('发现新版本 ','New version available: ')+'<b>'+esc(f.remote)+'</b> · '+t('当前 ','current ')+esc(f.ver);
 else if(st==='downloading'||st==='uploading'){const p=f.total?Math.min(100,Math.round(f.done*100/f.total)):0;
  m=(st==='downloading'?t('正在下载并写入…','Downloading and flashing…'):t('正在上传并写入…','Uploading and flashing…'))+' '+(f.total?p+'%':((f.done/1024)|0)+' KB');
  if(f.total)bar='<div style="height:8px;border-radius:4px;background:#0d141c;border:1px solid var(--line);margin-top:8px"><div style="height:100%;width:'+p+'%;background:var(--acc);border-radius:4px"></div></div>'}
 else if(st==='done')m='✅ '+t('更新完成,设备正在重启…','Update complete, the device is restarting…');
 else if(st==='error')m='<span style="color:var(--bad)">⚠️ '+esc(fwErr(f.err))+'</span>';
 if(!m&&f.pending)m='⏳ '+t('新固件试运行中:稳定运行约 1 分钟后才会确认。这期间如果崩溃或重启,设备会自动退回旧版本。','New firmware is on trial: it is confirmed after about 1 minute of stable running. If the device crashes or restarts before that, it goes back to the previous version automatically.');
 el.style.display=m?'block':'none';el.innerHTML=m?'<div>'+m+'</div>'+bar:'';
 const sl=$('#fwslot');if(sl&&f.slot)sl.textContent='· '+t('运行分区 ','running slot ')+f.slot;
 const c=$('#fwchk'),i=$('#fwins'),u=$('#fwup');
 if(c)c.disabled=run;if(u)u.disabled=run;
 if(i){i.disabled=run||st!=='available';i.textContent=st==='available'?t('安装 ','Install ')+f.remote:t('安装更新','Install update')}
}
async function loadFw(){const r=await api('/api/ota/status');if(r){FW.st=r;if(FW.saved===null)FW.saved=C.ghRepo||'';fwDraw();if(['checking','downloading','uploading','done'].indexOf(r.state)>=0)fwWatch()}}
function fwWatch(){clearInterval(FW.timer);FW.timer=setInterval(async()=>{
 let r=null;try{const x=await fetch('/api/ota/status');r=await x.json()}catch(e){}
 if(!r){if(FW.reboot){FW.lost=true;fwDraw()}return}
 if(FW.lost){clearInterval(FW.timer);location.reload();return}   // the device is back after its restart
 FW.st=r;if(r.state==='done')FW.reboot=true;
 if(tab==='fw')fwDraw();
 if(['checking','downloading','uploading','done'].indexOf(r.state)<0)clearInterval(FW.timer)},1000)}
async function fwCheck(){
 if(FW.saved!==(C.ghRepo||'')){const r=await api('/api/config',{method:'POST',body:JSON.stringify(C)});if(!(r&&r.ok))return;FW.saved=C.ghRepo||''}
 const r=await api('/api/ota/check',{method:'POST'});if(!r)return;
 if(!r.ok){await loadFw();return}
 FW.st={state:'checking'};fwDraw();fwWatch()}
async function fwInstall(){
 if(!confirm(t('开始更新固件?期间不要断电,约一两分钟,完成后设备会自动重启。','Start the firmware update? Do not power off. It takes a minute or two and the device restarts by itself afterwards.')))return;
 const r=await api('/api/ota/install',{method:'POST'});if(!r)return;
 if(!r.ok){await loadFw();return}
 FW.st={state:'downloading',done:0,total:0,ver:FW.st&&FW.st.ver};fwDraw();fwWatch()}
function fwUpload(){
 const f=$('#fwfile').files[0];if(!f)return toast(t('先选择 firmware.bin','Pick firmware.bin first'));
 if(!confirm(t('用「','Update the device with "')+f.name+t('」更新设备固件?期间不要断电,完成后设备会自动重启。','"? Do not power off. The device restarts by itself afterwards.')))return;
 const fd=new FormData();fd.append('fw',f,'firmware.bin');
 const x=new XMLHttpRequest();x.open('POST','/api/ota');
 x.upload.onprogress=e=>{FW.st={state:'uploading',done:e.loaded,total:e.total};fwDraw()};
 x.onload=()=>{let r=null;try{r=JSON.parse(x.responseText)}catch(e){}
  if(r&&r.ok){FW.st=r.st;FW.reboot=true;fwDraw();fwWatch()}else{FW.st=(r&&r.st&&r.st.state==='error')?r.st:{state:'error',err:'net'};fwDraw()}};
 x.onerror=()=>{FW.st={state:'error',err:'net'};fwDraw()};
 x.send(fd)}
async function selfTest(){toast(t('自检中:数码管全亮、灯环转一圈、喇叭响一声','Self-test: digits lit, LED ring sweep, a chime'));await api('/api/selftest',{method:'POST'})}
async function setMute(){await api('/api/mute?on='+(S.mute?0:1),{method:'POST'});toast(S.mute?t('已取消静音','Unmuted'):t('已静音','Muted'));refresh()}
async function syncNtp(){await api('/api/time/sync',{method:'POST'});toast(t('已发起网络校时,几秒后刷新…','NTP sync started, refreshing shortly\u2026'));setTimeout(refresh,4000)}
async function syncBrowser(){await api('/api/time/set?ts='+Math.floor(Date.now()/1000),{method:'POST'});toast(t('已用浏览器时间校时','Clock set from this device'));setTimeout(refresh,600)}
async function save(){const r=await api('/api/config',{method:'POST',body:JSON.stringify(C)});toast(r&&r.ok?t('已保存','Saved'):t('保存失败','Save failed'))}
async function act(u,m){await api(u,{method:'POST'});toast(m)}
function setDay(d,on){C.quietDays=on?(C.quietDays|(1<<d)):(C.quietDays&~(1<<d))}
function setEvBit(i,b,on){const n=C.notify[i];n.evMask=on?(n.evMask|(1<<b)):(n.evMask&~(1<<b))}
const cutB=(s,n)=>{let b=0,o='';for(const ch of s){const l=unescape(encodeURIComponent(ch)).length;if(b+l>n)break;b+=l;o+=ch}return o};
function parseErr(text,defAct){
  const res={add:0,upd:0,dup:0,bad:[]};
  const sep=/^[\s,，;；=＝:：\t]+/;
  text.split(/\r?\n/).forEach(raw=>{
    const line=raw.replace(/^\uFEFF/,'').trim();
    if(!line||line[0]==='#')return;
    let code=null,rest='';
    let m=line.match(/^(\d{7,10})(?![0-9A-Za-z])/);
    if(m&&m[1][0]!=='0'&&Number(m[1])<=4294967295){code=Number(m[1]).toString(16).toUpperCase().padStart(8,'0');rest=line.slice(m[0].length)}
    else{m=line.match(/^(?:HMS)?[_\s-]*(?:0[xX])?([0-9A-Fa-f]{4}(?:[\s_-]?[0-9A-Fa-f]{4}){1,3})(?![0-9A-Za-z])/i);if(m){code=m[1].replace(/[^0-9A-Fa-f]/g,'').toUpperCase();rest=line.slice(m[0].length)}}
    if(!code||(code.length!==8&&code.length!==16)){res.bad.push(raw);return}
    rest=rest.replace(sep,'');
    let act=null;
    const am=rest.match(/^(忽略|ignore|只响一下|响一下|blip|持续告警|持续|sticky|一律响铃|响铃|always|ring|仅备注|备注|label|note)(?=[\s,，;；=＝:：\t]|$)/i);
    if(am){const k=am[1].toLowerCase();act=(k==='忽略'||k==='ignore')?0:(['一律响铃','响铃','always','ring'].includes(k)?1:(['持续告警','持续','sticky'].includes(k)?3:(['只响一下','响一下','blip'].includes(k)?4:2)));rest=rest.slice(am[0].length).replace(sep,'')}
    const note=cutB(rest.trim(),47);
    const ex=C.errCodes.find(e=>e.code===code);
    if(ex){let ch=false;if(act!==null&&ex.action!==act){ex.action=act;ch=true}if(note&&ex.note!==note){ex.note=note;ch=true}ch?res.upd++:res.dup++;return}
    if(C.errCodes.length>=40){res.bad.push(raw);return}
    C.errCodes.push({code:code,action:act===null?defAct:act,note:note});res.add++;
  });
  return res}
function importErr(){
  const r=parseErr(ERRTXT,ERRDEF);
  ERRTXT=r.bad.join('\n');
  ERRRES=t('新增 ','Added ')+r.add+t(' 条,更新 ',', updated ')+r.upd+t(' 条,重复未变 ',', unchanged duplicates ')+r.dup+(r.bad.length?t(' 条,无法识别(或列表已满) ',', unreadable (or list full) ')+r.bad.length:'')+t(' 条。记得点「保存设置」。','. Remember to press "Save settings".');
  render();toast(r.add||r.upd?t('已导入,点「保存设置」生效','Imported. Press "Save settings" to apply.'):t('没有新内容','Nothing new'))}
function exportErr(){
  const an={2:t('仅备注','note'),0:t('忽略','ignore'),1:t('一律响铃','ring'),3:t('持续告警','sticky'),4:t('只响一下','blip')};
  ERRTXT=C.errCodes.filter(e=>e.code).map(e=>e.code+','+(an[e.action]||an[2])+','+(e.note||'')).join('\n');
  ERRRES='';render()}
function copyErr(){const ta=document.createElement('textarea');ta.value=ERRTXT||'';document.body.appendChild(ta);ta.select();let ok=false;try{ok=document.execCommand('copy')}catch(e){}document.body.removeChild(ta);toast(ok?t('已复制','Copied'):t('复制失败,请手动选中复制','Copy failed, select and copy by hand'))}
function addCode(){if(C.errCodes.length>=40)return toast(t('最多 40 条','Max. 40 entries'));C.errCodes.push({code:'',action:2,note:''});render()}
function delCode(i){C.errCodes.splice(i,1);render()}
function ignoreCode(c){if(C.errCodes.length>=40)return toast(t('列表已满(40 条)','List is full (40 entries)'));C.errCodes.push({code:c,action:0,note:''});render();toast(t('已加入忽略列表,点「保存设置」生效','Added to the ignore list. Press "Save settings" to apply.'))}
async function loadLog(){const r=await api('/api/log');if(r){LOG=r;if(tab==='log')render()}}
async function clearLog(){if(!confirm(t('清空告警记录?','Clear the alert log?')))return;await api('/api/log/clear',{method:'POST'});LOG=[];render()}
async function testNotify(i){await api('/api/notify/test?slot='+i,{method:'POST'});toast(t('已发出测试,几秒后刷新结果…','Test sent, refreshing the result shortly…'));setTimeout(refresh,4500)}
async function exitSetup(){await api('/api/exit',{method:'POST'});toast(t('已退出设置模式','Setup mode exited'));setTimeout(refresh,800)}
function stopAlarm(){act('/api/stop',t('已停止','Stopped'))}
function testEv(k){act('/api/test?type=event&ev='+k,t('测试中…按"停止报警"结束','Testing… press "Stop alarm" to end'))}
function testSp(n){act('/api/test?type=speaker&sound='+encodeURIComponent(n),t('播放 3 秒','Playing for 3 s'))}
function testBz(){act('/api/test?type=buzzer',t('响 3 秒','Buzzing for 3 s'))}
function prev(n){new Audio('/sounds/'+n).play()}
async function delS(n){if(!confirm(t('删除 ','Delete ')+n+'?'))return;await api('/api/sound/delete?name='+encodeURIComponent(n),{method:'POST'});refresh()}
function useP(i,s){const d=S.discovered[i],p=C.printers[s];p.enabled=true;p.name=d.name||d.model||p.name;p.ip=d.ip;p.serial=d.serial;render();toast(t('已填入打印机 ','Filled in printer ')+(s+1)+t(',请填访问码后保存',' - enter the access code and save'))}
async function scan(){
  const sc=$('#sc');if(sc)sc.textContent=t('扫描中…','Scanning…');
  for(let i=0;i<10;i++){const r=await api('/api/scan');if(r&&!r.scanning){nets=r.nets;render();return}await new Promise(r=>setTimeout(r,1200))}
  if($('#sc'))$('#sc').textContent=t('扫描失败','Scan failed')}
function pick(i){C.ssid=nets[i].ssid;render()}
async function toWav(file){
  const ab=await file.arrayBuffer();
  const AC=window.AudioContext||window.webkitAudioContext;const ctx=new AC();
  const buf=await ctx.decodeAudioData(ab);ctx.close();
  const len=Math.max(1,Math.ceil(buf.duration*16000));
  const oc=new OfflineAudioContext(1,len,16000);
  const src=oc.createBufferSource();src.buffer=buf;src.connect(oc.destination);src.start();
  const out=await oc.startRendering();
  const d=out.getChannelData(0),n=d.length;
  const dv=new DataView(new ArrayBuffer(44+n*2));
  const w=(o,s)=>{for(let i=0;i<s.length;i++)dv.setUint8(o+i,s.charCodeAt(i))};
  w(0,'RIFF');dv.setUint32(4,36+n*2,true);w(8,'WAVEfmt ');dv.setUint32(16,16,true);
  dv.setUint16(20,1,true);dv.setUint16(22,1,true);dv.setUint32(24,16000,true);dv.setUint32(28,32000,true);
  dv.setUint16(32,2,true);dv.setUint16(34,16,true);w(36,'data');dv.setUint32(40,n*2,true);
  for(let i=0;i<n;i++){const s=Math.max(-1,Math.min(1,d[i]));dv.setInt16(44+i*2,s<0?s*32768:s*32767,true)}
  return new Blob([dv],{type:'audio/wav'})}
async function upl(){
  const f=$('#f').files[0];if(!f)return toast(t('先选择文件','Choose a file first'));
  toast(t('转换并上传中…','Converting and uploading…'));
  try{
    const wav=await toWav(f);
    const nm=f.name.replace(/\.[^.]+$/,'').replace(/[^A-Za-z0-9_-]/g,'_')||('sound_'+(Date.now()%100000));
    const fd=new FormData();fd.append('f',wav,nm+'.wav');
    const r=await fetch('/api/sound',{method:'POST',body:fd});
    toast(r.ok?t('已上传','Uploaded'):t('上传失败','Upload failed'));refresh()
  }catch(e){toast(t('无法解码该文件','Cannot decode this file'))}}
setInterval(()=>{if(tab==='status')refresh()},4000);
setInterval(()=>{if(tab==='debug'&&DBG.auto)loadDbg(true)},3000);
applyStatic();
refresh();
</script></body></html>)HTML";

// Same radar icon as a file, for browsers that ask for /favicon.ico.
const char FAVICON_SVG[] PROGMEM = R"SVG(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64"><defs><linearGradient id="g" x1="0" y1="0" x2="1" y2="1"><stop offset="0" stop-color="#3b9cff"/><stop offset="1" stop-color="#7c5cff"/></linearGradient></defs><circle cx="32" cy="32" r="29" fill="#0d141c" stroke="url(#g)" stroke-width="4"/><circle cx="32" cy="32" r="18" fill="none" stroke="#2a3a4d" stroke-width="2"/><circle cx="32" cy="32" r="7" fill="none" stroke="#2a3a4d" stroke-width="2"/><path d="M32 32 L32 4 A28 28 0 0 1 56.2 18 Z" fill="#3b9cff" fill-opacity=".55"/><circle cx="44" cy="20" r="3.5" fill="#34d399"/></svg>)SVG";
