const assert=require('assert'),fs=require('fs'),vm=require('vm');
const tracks=Array.from({length:257},(_,i)=>({uri:'spotify:track:'+i,name:'Song '+i,artists:[{name:'Artist'}],album:{name:'Album'}}));
function harness(items=tracks,ack='saved'){
 const window={};
 const sends=[],messages=[],offsets=[],batches=[],saveTimers=[];
 const api={getMetadata:async()=>({name:'Playlist'}),getContents:async(_,options)=>{
   offsets.push(options.offset);return {items:items.slice(options.offset,options.offset+100),totalLength:items.length};}};
 const sgf={FS:'',RS:'',Icons:{},platform:{getPlaylistAPI:()=>api},
   notify:text=>messages.push(text),send:(...args)=>{sends.push(args);if(ack)Promise.resolve().then(()=>window.__soggfyM3UResult(args[1].split(String.fromCharCode(29))[0]+String.fromCharCode(29)+ack));},
   requestStatuses:async page=>{batches.push(page.length);return new Map(page.map(t=>[t.uri,{status:'DONE',path:'C:/Music/'+t.title+'.mp3'}]));}};
 window.__SoggfyClassic=sgf;
 vm.runInNewContext(fs.readFileSync('native/ui/classic_m3u.js','utf8'),{
   window,location:{pathname:'/playlist/test'},document:{querySelector:()=>null},
   console:{warn(){}},setTimeout:(fn,ms)=>{if(ms===600000){saveTimers.push(fn);return 0;}return setTimeout(fn,ms);},clearTimeout});
 return {sgf,api,sends,messages,offsets,batches,window,saveTimers};
}
(async()=>{
 const h=harness([...tracks,tracks[0]]);await h.sgf.generateM3U();
 assert.deepStrictEqual(h.offsets,[0,100,200]);assert.deepStrictEqual(h.batches,[128,128,1]);
 assert.strictEqual(h.sends.length,1);const records=h.sends[0][1].split(String.fromCharCode(29))[1].split('');
 assert.strictEqual(records.length,259);assert.strictEqual(records[1],records.at(-1),'Playlist duplicates retain order');
 assert(h.messages.at(-1).startsWith('Saved 258'));
 for(const result of ['canceled','failed']){
   const h=harness(tracks,result);await h.sgf.generateM3U();
   assert(!h.messages.some(x=>x.startsWith('Saved ')));
   assert(h.messages.at(-1).includes(result==='canceled'?'canceled':'could not be saved'));
 }
 const waiting=harness(tracks,null),inflight=waiting.sgf.generateM3U();
 while(!waiting.sends.length)await new Promise(resolve=>setImmediate(resolve));
 await waiting.sgf.generateM3U();assert.strictEqual(waiting.sends.length,1,'Save dialog allows duplicate request');
 waiting.window.__soggfyM3UResult('foreign'+String.fromCharCode(29)+'saved');
 assert(!waiting.messages.some(x=>x.startsWith('Saved ')));
 const request=waiting.sends[0][1].split(String.fromCharCode(29))[0];
 waiting.window.__soggfyM3UResult(request+String.fromCharCode(29)+'saved');await inflight;
 assert(waiting.messages.at(-1).startsWith('Saved '));
 const timeout=harness(tracks,null),timed=timeout.sgf.generateM3U();
 while(!timeout.sends.length)await new Promise(resolve=>setImmediate(resolve));
 timeout.saveTimers[0]();await timed;
 assert(timeout.messages.at(-1).includes('Save result not received'));
 timeout.window.__soggfyM3UResult(timeout.sends[0][1].split(String.fromCharCode(29))[0]+String.fromCharCode(29)+'saved');
 assert(!timeout.messages.some(x=>x.startsWith('Saved ')),'Late reply changed timed-out result');
 const sendFailure=harness();sendFailure.sgf.send=()=>{throw Error('bridge unavailable');};
 await sendFailure.sgf.generateM3U();assert.strictEqual(sendFailure.messages.at(-1),'bridge unavailable');
 await sendFailure.sgf.generateM3U();assert.strictEqual(sendFailure.messages.at(-1),'bridge unavailable');
 const partial=harness();partial.sgf.requestStatuses=async()=>new Map();await partial.sgf.generateM3U();
 assert.strictEqual(partial.sends.length,0);assert(partial.messages.at(-1).includes('incomplete'));
 const failed=harness();failed.api.getContents=async()=>{throw Error('unavailable');};
 const truncated=harness();truncated.api.getContents=async()=>({items:[],totalLength:257});
 await truncated.sgf.generateM3U();assert.strictEqual(truncated.sends.length,0);assert(truncated.messages.at(-1).includes('incomplete'));
 await failed.sgf.generateM3U();assert.strictEqual(failed.sends.length,0);assert.strictEqual(failed.messages.at(-1),'unavailable');
 const busy=harness();let release;
 busy.api.getMetadata=()=>new Promise(resolve=>{release=resolve;});
 const first=busy.sgf.generateM3U();await Promise.resolve();await busy.sgf.generateM3U();
 release({name:'Busy'});await first;assert.strictEqual(busy.sends.length,1);
 const oversized=harness(Array.from({length:10001},()=>tracks[0]));
 await oversized.sgf.generateM3U();assert.strictEqual(oversized.sends.length,0);assert(oversized.messages.at(-1).includes('10,000'));
 console.log('PASS: complete M3U pagination, 128-status batches, duplicates, partial failures, concurrent clicks and limits');
})().catch(error=>{console.error(error);process.exitCode=1;});
