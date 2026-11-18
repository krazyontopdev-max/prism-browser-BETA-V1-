import { spawn, execFileSync } from 'node:child_process';
import { createServer } from 'node:http';
import { mkdir, readFile, writeFile } from 'node:fs/promises';
import { resolve } from 'node:path';

const pause = ms => new Promise(r => setTimeout(r, ms));
const output = { date: new Date().toISOString(), conditions: 'One visible page per browser, isolated private sessions, identical localhost content, 1180x700 CSS viewport, five alternating navigations per browser. Warm navigation timing, not OS cold startup or real-site speed.', rows: [] };
const content = '<!doctype html><title>Prism comparison fixture</title><style>body{font:16px Arial}p{height:60px}</style><input aria-label="Test input"><script>window.data=new Uint8Array(32*1024*1024);for(let i=0;i<data.length;i+=4096)data[i]=123;</script>' + '<p>Repeatable browser workload</p>'.repeat(500);
const server = createServer((req,res) => { res.writeHead(200, {'Content-Type':'text/html','Cache-Control':'no-store'}); res.end(content); });
await new Promise(r => server.listen(0,'127.0.0.1',r));
const url = `http://127.0.0.1:${server.address().port}/`;
const peers = [];
async function connect(name, executable, args, port, env) {
  const child = spawn(executable,args,{env,stdio:'ignore'});
  const peer = { name, child };
  peers.push(peer);
  let targets;
  for(let i=0;i<100;i++) {
    if(child.exitCode!==null) throw Error(`${name} exited ${child.exitCode}`);
    try { targets = await (await fetch(`http://127.0.0.1:${port}/json/list`)).json(); if(targets.some(t=>t.type==='page')) break; } catch {}
    await pause(200);
  }
  const target = targets?.find(t=>t.type==='page');
  if(!target) throw Error(`${name} debugging endpoint unavailable`);
  const ws = new WebSocket(target.webSocketDebuggerUrl);
  peer.ws=ws;
  await new Promise((r,j)=>{ws.onopen=r;ws.onerror=j;});
  let serial=0;
  const pending=new Map();
  ws.onmessage=e=>{const m=JSON.parse(e.data);const p=pending.get(m.id);if(p){pending.delete(m.id);clearTimeout(p.timer);m.error?p.reject(Error(JSON.stringify(m.error))):p.resolve(m.result);}};
  peer.send=(method,params={})=>new Promise((resolve,reject)=>{const id=++serial;const timer=setTimeout(()=>{pending.delete(id);reject(Error(`${name} ${method} timeout`));},15000);pending.set(id,{resolve,reject,timer});ws.send(JSON.stringify({id,method,params}));});
  peer.evaluate=async expression=>(await peer.send('Runtime.evaluate',{expression,returnByValue:true,awaitPromise:true})).result.value;
  await peer.send('Emulation.setDeviceMetricsOverride',{width:1180,height:700,deviceScaleFactor:1,mobile:false});
  return peer;
}
try {
  const root=resolve('build/paired-speed-'+Date.now());await mkdir(root,{recursive:true});
  const prism=await connect('Prism','C:/Users/user/Desktop/Prism-WebEngine-Lab/app-build/Prism.exe',['--private'],9341,{...process.env,QTWEBENGINE_REMOTE_DEBUGGING:'127.0.0.1:9341'});
  const chrome=await connect('Chrome','C:/Program Files/Google/Chrome/Application/chrome.exe',['--remote-debugging-port=9342','--user-data-dir='+root,'--incognito','--no-first-run','--no-default-browser-check','--window-size=1280,900','about:blank'],9342,process.env);
  for(let run=0;run<5;run++) for(const peer of run%2?[chrome,prism]:[prism,chrome]) {
    await peer.send('Page.bringToFront');
    await peer.send('Page.navigate',{url:url+run});
    let ready=false;
    for(let i=0;i<100;i++){if(await peer.evaluate(`location.href===${JSON.stringify(url+run)}&&document.readyState==='complete'`)){ready=true;break;}await pause(50);}
    if(!ready)throw Error(`${peer.name} load timeout`);
    const timing=await peer.evaluate('({loadMs:performance.getEntriesByType("navigation")[0].loadEventEnd,domMs:performance.getEntriesByType("navigation")[0].domContentLoadedEventEnd,hidden:document.hidden,width:innerWidth,height:innerHeight,ua:navigator.userAgent})');
    if(timing.hidden||timing.width!==1180||timing.height!==700)throw Error(`${peer.name} visibility/viewport mismatch`);
    const frames=await peer.evaluate('new Promise(resolve=>{const times=[];let last;function frame(t){if(last!==undefined)times.push(t-last);last=t;if(times.length<60)requestAnimationFrame(frame);else{times.sort((a,b)=>a-b);resolve({medianMs:times[30],p95Ms:times[56]});}}requestAnimationFrame(frame);})');
    output.rows.push({browser:peer.name,run,...timing,frames});
  }
  output.memory=[];
  for(const peer of peers){await peer.send('Page.bringToFront');await pause(5000);const memory=JSON.parse(execFileSync('powershell.exe',['-NoProfile','-File',resolve('tests/process-metrics.ps1'),'-RootProcessId',String(peer.child.pid)],{encoding:'utf8',windowsHide:true}));output.memory.push({browser:peer.name,...memory});}
  output.valid=true;
} catch(error) { output.valid=false;output.failure=error.message;process.exitCode=1; }
finally {
  await writeFile(resolve('build/paired-speed-results.json'),JSON.stringify(output,null,2));
  for(const peer of peers){peer.ws?.close();if(peer.child.exitCode===null){try{execFileSync('taskkill.exe',['/PID',String(peer.child.pid),'/T','/F'],{stdio:'ignore',windowsHide:true});}catch{}}}
  server.close();
}
console.log(JSON.stringify(output,null,2));
