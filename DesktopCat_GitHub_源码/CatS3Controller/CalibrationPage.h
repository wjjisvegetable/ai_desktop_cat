#pragma once
const char CALIBRATION_PAGE[] PROGMEM=R"CATPAGE(
<!doctype html><html lang="zh-CN"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>桌宠校准</title><style>body{font:16px system-ui;max-width:760px;margin:30px auto;padding:18px;color:#213547}input,select,button{font:inherit;padding:8px;margin:5px}input[type=number]{width:100px}label{display:block;margin:10px 0}button{cursor:pointer}pre{white-space:pre-wrap;background:#f2f5f8;padding:16px}</style>
<h1>桌宠舵机校准</h1><p><a href="/remote">进入遥控页面</a></p>
<p>每次只接一个舵机，先拆下舵盘或解除机构负载。首次接通 PWM 会跳到所记录的位置，软件无法读取实际机械角度。中位、限位和方向需逐路确认。</p>
<p>试动 5 秒后自动关闭该路输出。停止只保持位置，关闭 PWM 后机构会失去保持力。</p>
<button id="release">关闭全部 PWM</button><button id="stop">停止并保持位置</button>
<label>通道 <select id="id"></select></label>
<label>最小脉宽 <input id="min" type="number" value="1400"> μs</label>
<label>中位脉宽 <input id="center" type="number" value="1500"> μs</label>
<label>最大脉宽 <input id="max" type="number" value="1600"> μs</label>
<label><input id="reverse" type="checkbox">反向</label>
<label><input id="enabled" type="checkbox">用于动作</label>
<label><input id="confirmed" type="checkbox">已逐路核验中位和机械限位</label>
<button id="save">保存该路配置</button>
<label>试动目标 <input id="pulse" type="number" value="1500"> μs <button id="test">单路试动</button></label>
<p>先保存范围，再试动；最后确认校准并保存。默认 1400–1600 μs 仅是保守起点，不代表实际机构安全范围。通道 6、7 为备用，未安装则保持关闭。</p>
<pre id="msg">正在读取…</pre>
<script>
const $=id=>document.getElementById(id);let s;
async function load(){const r=await fetch('/api/status');if(!r.ok)throw Error(await r.text());s=await r.json();if(!$('id').options.length)s.servos.forEach(v=>$('id').add(new Option(v.id+' '+v.name,v.id)));fill();}
function fill(){const v=s.servos[+$('id').value];['min','center','max'].forEach(k=>$(k).value=v[k]);$('reverse').checked=v.reverse;$('enabled').checked=v.enabled;$('confirmed').checked=v.calibrated;$('pulse').value=v.center;$('msg').textContent=JSON.stringify(s,null,2);}
async function post(path,data){try{const r=await fetch(path,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(data)});const t=await r.text();if(!r.ok)throw Error(t);$('msg').textContent=t;}catch(e){$('msg').textContent=e.message;}}
$('id').onchange=fill;
$('save').onclick=()=>post('/api/calibration',{id:$('id').value,min:$('min').value,center:$('center').value,max:$('max').value,enabled:+$('enabled').checked,reverse:+$('reverse').checked,confirmed:+$('confirmed').checked});
$('test').onclick=()=>{if(confirm('确认已解除负载，并允许当前通道接通 PWM？'))post('/api/test',{id:$('id').value,pulse:$('pulse').value});};
$('release').onclick=()=>post('/api/action',{name:'release'});$('stop').onclick=()=>post('/api/action',{name:'stop'});
load().catch(e=>$('msg').textContent=e.message);
</script></html>
)CATPAGE";
