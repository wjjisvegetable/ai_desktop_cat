import http from 'node:http';
import { timingSafeEqual } from 'node:crypto';
import { pathToFileURL } from 'node:url';

export function actionForText(input) {
  const s = input.trim().replace(/[。！？!?,，\s]/g, '').toLowerCase();
  const map = new Map([
    ['点头','nod'], ['摇头','shake'], ['招手','wave'], ['挥手','wave'],
    ['动耳朵','ears'], ['停止','stop'], ['停下','stop'], ['回中位','center'],
    ['nod','nod'], ['shake','shake'], ['wave','wave'], ['ears','ears'],
    ['stop','stop'], ['center','center']
  ]);
  return map.get(s) || null;
}

function sameToken(received, expected) {
  if (!expected || !received) return false;
  const a = Buffer.from(received), b = Buffer.from(expected);
  return a.length === b.length && timingSafeEqual(a,b);
}
function send(res, status, data) {
  res.writeHead(status, {'Content-Type':'application/json; charset=utf-8','Cache-Control':'no-store'});
  res.end(JSON.stringify(data));
}
async function readBody(req, limit=16384) {
  let bytes=0; const chunks=[];
  for await (const chunk of req) {
    bytes+=chunk.length;
    if(bytes>limit) { const error=new Error('请求过长');error.status=413;throw error; }
    chunks.push(chunk);
  }
  return Buffer.concat(chunks).toString('utf8');
}
export function makeHandler({apiKey=process.env.OPENAI_API_KEY,localToken=process.env.DESKTOPCAT_PROXY_TOKEN,model=process.env.OPENAI_MODEL||'gpt-4.1-mini',fetchImpl=fetch}={}) {
  return async (req,res) => {
    if(req.method==='GET' && req.url==='/health') {send(res,200,{ok:true,cloudConfigured:!!apiKey});return;}
    if(req.method!=='POST'||req.url!=='/ask') {send(res,404,{error:'Not found'});return;}
    const supplied=(req.headers.authorization||'').replace(/^Bearer /i,'');
    if(!sameToken(supplied,localToken)) {send(res,401,{error:'Unauthorized'});return;}
    if(!apiKey) {send(res,503,{error:'设置 OPENAI_API_KEY 后才能调用云端'});return;}
    try {
      const body=JSON.parse(await readBody(req));
      const text=body?.text;
      if(typeof text!=='string'||text.trim().length<1||text.length>400) {send(res,400,{error:'text 需要 1–400 字'});return;}
      const history=body?.history??[];
      if(!Array.isArray(history)||history.length>20||history.some((item)=>
        !item||!['user','assistant'].includes(item.role)||typeof item.content!=='string'||
        item.content.length<1||item.content.length>1000)) {
        send(res,400,{error:'history 最多 20 条，每条需要 role 和 1–1000 字 content'});return;
      }
      const upstream=await fetchImpl('https://api.openai.com/v1/responses',{
        method:'POST',
        headers:{'Authorization':`Bearer ${apiKey}`,'Content-Type':'application/json'},
        body:JSON.stringify({
          model,store:false,
          instructions:'你是一只桌面招财猫。用简短自然的中文回应用户，不要声称已经执行物理动作。',
          input:[...history.map(item=>({role:item.role,content:item.content})),
            {role:'user',content:text.trim()}]
        }),
        signal:AbortSignal.timeout(30000)
      });
      if(!upstream.ok) {send(res,502,{error:`云端请求失败 (${upstream.status})`});return;}
      const response=await upstream.json();
      const reply=(response.output||[]).flatMap(item=>item.content||[])
        .filter(item=>item.type==='output_text').map(item=>item.text||'').join('').trim();
      if(!reply) {send(res,502,{error:'云端没有返回文字'});return;}
      // Only the exact, local whitelist decides physical actions; model text never becomes a command.
      send(res,200,{reply:reply.slice(0,1000),action:actionForText(text)});
    } catch(error) {
      send(res,error.status||((error instanceof SyntaxError)?400:502),
        {error:error.status?error.message:(error instanceof SyntaxError?'JSON 格式错误':'云端连接或响应错误')});
    }
  };
}

if(process.argv[1] && import.meta.url===pathToFileURL(process.argv[1]).href) {
  const host=process.env.DESKTOPCAT_HOST||'127.0.0.1';
  const port=Number(process.env.DESKTOPCAT_PORT||8787);
  if(!process.env.DESKTOPCAT_PROXY_TOKEN || process.env.DESKTOPCAT_PROXY_TOKEN.length<16) {
    throw new Error('先设置至少 16 字符的 DESKTOPCAT_PROXY_TOKEN');
  }
  http.createServer(makeHandler()).listen(port,host,()=>{
    console.log(`DesktopCat voice proxy listening on ${host}:${port}`);
  });
}
