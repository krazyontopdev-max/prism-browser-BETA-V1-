import {spawn, execFileSync} from 'node:child_process';
import {createServer} from 'node:http';
import {mkdir, readFile, writeFile} from 'node:fs/promises';
import {resolve} from 'node:path';
const sleep = ms => new Promise(r => setTimeout(r, ms));
const root = resolve('build/chrome-benchmark-profile-' + Date.now());
await mkdir(root, {recursive:true});
const body = "<title>Prism workload</title><style>div{height:150px;background:linear-gradient(20deg,#abc,#def);margin:8px}</style><input value='keep me'><script>window.ticks=0;setInterval(()=>{window.ticks++;let s=0;for(let i=0;i<50000;i++)s+=Math.sqrt(i);},100);</script>" + '<div>Scroll workload</div>'.repeat(100);
const server = createServer((req,res) => {res.writeHead(200,{'Content-Type':'text/html','Connection':'close'});res.end(body);});
await new Promise(r=>server.listen(0,'127.0.0.1',r));
const chrome = spawn('C:/Program Files/Google/Chrome/Application/chrome.exe', ['--headless=new','--remote-debugging-port=0','--user-data-dir='+root,'--no-first-run','--no-default-browser-check','--window-size=1180,800','about:blank'], {windowsHide:true,stdio:'ignore'});
let browser;
const pending = new Map(); let serial=0;
try {
    let port;
    for(let i=0;i<100;i++) {try {port=(await readFile(root+'/DevToolsActivePort','utf8')).split('\n')[0];break;}catch{} await sleep(100);}
    if(!port) throw new Error('Chrome did not start');
    const version=await (await fetch('http://127.0.0.1:'+port+'/json/version')).json();
    browser=new WebSocket(version.webSocketDebuggerUrl);
    await new Promise((r,j)=>{browser.onopen=r;browser.onerror=j;});
    browser.onmessage=event=>{const message=JSON.parse(event.data);if(message.id&&pending.has(message.id)){const {resolve,reject}=pending.get(message.id);pending.delete(message.id);message.error?reject(message.error):resolve(message.result);}};
    const send=(method,params={},sessionId)=>new Promise((resolve,reject)=>{const id=++serial;pending.set(id,{resolve,reject});browser.send(JSON.stringify({id,method,params,sessionId}));setTimeout(()=>{if(pending.delete(id))reject(new Error('CDP timeout '+method));},15000).unref();});
    const metrics=()=>JSON.parse(execFileSync('powershell.exe',['-NoProfile','-File',resolve('tests/process-metrics.ps1'),'-RootProcessId',String(chrome.pid)],{encoding:'utf8',windowsHide:true}));
    const targets=[]; const rows=[];
    const existing=(await send('Target.getTargets')).targetInfos.find(x=>x.type==='page');
    const session=await send('Target.attachToTarget',{targetId:existing.targetId,flatten:true});
    await send('Page.navigate',{url:`http://127.0.0.1:${server.address().port}/0`},session.sessionId);
    targets.push(existing.targetId);
    for(const count of [1,5,10]) {
        while(targets.length<count){const {targetId}=await send('Target.createTarget',{url:`http://127.0.0.1:${server.address().port}/${targets.length}`});targets.push(targetId);}
        await sleep(2500);
        const before=metrics();const start=performance.now();await sleep(3000);const after=metrics();
        const elapsed=(performance.now()-start)/1000;
        const delta=Object.entries(after.cpuByPid).reduce((sum,[pid,cpu])=>sum+(pid in before.cpuByPid ? Math.max(0,cpu-before.cpuByPid[pid]) : 0),0);
        rows.push({tabs:count,...after,idleCpuOneCorePercent:delta/elapsed*100,cpuNote:'Only processes present at both samples; exited processes excluded.'});
    }
    await writeFile('build/chrome-results.json',JSON.stringify({browser:version.Browser,conditions:'Isolated fresh profile, headless Chromium content; no Prism Widgets UI. Not an equal browser benchmark.',workload:rows},null,2));
    await send('Browser.close');
} finally {
    browser?.close(); server.close();
    if(chrome.exitCode===null) chrome.kill();
}
