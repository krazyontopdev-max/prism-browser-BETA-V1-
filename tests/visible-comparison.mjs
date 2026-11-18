import {spawn,execFileSync} from 'node:child_process';
import {createServer} from 'node:http';
import {readFile,writeFile,mkdir} from 'node:fs/promises';
import {resolve} from 'node:path';
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
const body="<!doctype html><title>Matched browser fixture</title><style>div{height:90px;background:#bcd;margin:8px}</style><input value='unchanged'><script>window.ticks=0;setInterval(()=>window.ticks++,1000)</script>"+'<div>Identical fixture content</div>'.repeat(150);
const server=createServer((req,res)=>{res.writeHead(200,{'Content-Type':'text/html','Cache-Control':'no-store','Connection':'close'});res.end(body)});
await new Promise(r=>server.listen(0,'127.0.0.1',r));
const base=`http://127.0.0.1:${server.address().port}/`;
const metric=pid=>JSON.parse(execFileSync('powershell.exe',['-NoProfile','-File',resolve('tests/process-metrics.ps1'),'-RootProcessId',String(pid)],{encoding:'utf8',windowsHide:true}));
async function sample(pid){const before=metric(pid);const start=performance.now();await sleep(3000);const after=metric(pid);let cpu=0,added=0;for(const [id,value] of Object.entries(after.cpuByIdentity)){if(id in before.cpuByIdentity)cpu+=Math.max(0,value-before.cpuByIdentity[id]);else added++}return {...after,cpuOneCorePercent:cpu/((performance.now()-start)/1000)*100,added,exited:Object.keys(before.cpuByIdentity).filter(id=>!(id in after.cpuByIdentity)).length}}
const output={conditions:'Visible windows; identical local URLs/content; fresh off-the-record contexts; all tabs loaded/undiscarded; last tab visible; 250 ms after each load, 5 s settle, 3 s idle sample; same PowerShell process-tree Private Bytes collector. Prism uses Release BrowserWindow harness with QtTest instrumentation. Different engine versions/native UI remain inherent differences.',base};
let chrome,prism,ws;
try{
 const file=resolve('build/visible-prism.json');
 prism=spawn(resolve('build/prism_benchmark.exe'),[file],{env:{...process.env,PATH:'C:\\Qt\\6.12.0\\msvc2022_64\\bin;'+process.env.PATH,PRISM_COMPARE_BASE:base},stdio:'ignore'});
 output.prism=[];
 for(const count of [1,5,10]){let ready;for(let n=0;n<900;n++){try{const r=JSON.parse(await readFile(file+'.ready','utf8'));if(r.tabs===count&&r.pid===prism.pid){ready=r;break}}catch{}if(prism.exitCode!==null)throw Error('Prism exited '+prism.exitCode);await sleep(100)}if(!ready)throw Error('Prism readiness timeout');output.prism.push({...ready,memory:await sample(prism.pid)});await writeFile(file+'.ack','continue');}
 await new Promise(r=>prism.once('exit',r));
 const root=resolve('build/visible-chrome-profile-'+Date.now());await mkdir(root,{recursive:true});
 const vp=output.prism[0].viewport;
 chrome=spawn('C:/Program Files/Google/Chrome/Application/chrome.exe',['--remote-debugging-port=0','--user-data-dir='+root,'--incognito','--no-first-run','--no-default-browser-check','--window-size=1180,800','about:blank'],{stdio:'ignore'});
 let port;for(let n=0;n<100;n++){try{port=(await readFile(root+'/DevToolsActivePort','utf8')).split('\n')[0];break}catch{}await sleep(100)}if(!port)throw Error('Chrome did not start');
 const version=await(await fetch('http://127.0.0.1:'+port+'/json/version')).json();output.chromeVersion=version.Browser;
 ws=new WebSocket(version.webSocketDebuggerUrl);await new Promise((r,j)=>{ws.onopen=r;ws.onerror=j});let serial=0;const pending=new Map();
 ws.onmessage=e=>{const m=JSON.parse(e.data);if(pending.has(m.id)){const p=pending.get(m.id);pending.delete(m.id);m.error?p.reject(Error(JSON.stringify(m.error))):p.resolve(m.result)}};
 const send=(method,params={},sessionId)=>new Promise((resolve,reject)=>{const id=++serial;pending.set(id,{resolve,reject});ws.send(JSON.stringify({id,method,params,sessionId}));setTimeout(()=>{if(pending.delete(id))reject(Error(method+' timeout'))},15000).unref()});
 const first=(await send('Target.getTargets')).targetInfos.find(t=>t.type==='page');
 const targets=[];output.chrome=[];
 for(const count of [1,5,10]){
  while(targets.length<count){const id=targets.length===0?first.targetId:(await send('Target.createTarget',{url:'about:blank',browserContextId:first.browserContextId,newWindow:false,background:true})).targetId;const {sessionId}=await send('Target.attachToTarget',{targetId:id,flatten:true});targets.push({id,sessionId});await send('Target.activateTarget',{targetId:id});await send('Emulation.setDeviceMetricsOverride',{width:vp.width,height:vp.height,deviceScaleFactor:vp.scale,mobile:false},sessionId);await send('Page.navigate',{url:base+(targets.length-1)},sessionId);let loaded=false;for(let n=0;n<450;n++){const r=await send('Runtime.evaluate',{expression:'document.readyState===\"complete\" && location.href.startsWith('+JSON.stringify(base)+')',returnByValue:true},sessionId);if(r.result.value){loaded=true;break}await sleep(100)}if(!loaded)throw Error('Chrome load timeout');await sleep(250)}
  await send('Target.activateTarget',{targetId:targets.at(-1).id});await sleep(5000);
  const states=[];for(const t of targets){const r=await send('Runtime.evaluate',{expression:'({url:location.href,hidden:document.hidden,discarded:document.wasDiscarded,width:innerWidth,height:innerHeight})',returnByValue:true},t.sessionId);states.push(r.result.value)}
  if(states.filter(s=>!s.hidden).length!==1 || states.some((s,i)=>s.discarded || s.url!==base+i || s.width!==vp.width || s.height!==vp.height)) throw Error('Chrome tab-state/viewport comparison invalid: '+JSON.stringify(states));
  output.chrome.push({tabs:count,states,memory:await sample(chrome.pid)});
 }
 await send('Browser.close');output.valid=true;await writeFile('build/visible-comparison.json',JSON.stringify(output,null,2));
 console.log(JSON.stringify({prism:output.prism.map(x=>[x.tabs,x.memory.privateMiB]),chrome:output.chrome.map(x=>[x.tabs,x.memory.privateMiB]),version:output.chromeVersion}));
}catch(error){output.valid=false;output.failure=error.message;await writeFile('build/visible-comparison-failed.json',JSON.stringify(output,null,2));throw error}
finally{ws?.close();server.close();if(prism&&prism.exitCode===null)prism.kill();if(chrome&&chrome.exitCode===null)chrome.kill()}
