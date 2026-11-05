const assert=require('assert'),fs=require('fs'),vm=require('vm');
const tracks=Array.from({length:257},(_,i)=>({uri:'spotify:track:'+i,name:'Song '+i,artists:[{name:'Artist'}],album:{name:'Album'}}));
function harness(items=tracks){
 const sends=[],messages=[],offsets=[],batches=[];
 const api={getMetadata:async()=>({name:'Playlist'}),getContents:async(_,options)=>{
   offsets.push(options.offset);return {items:items.slice(options.offset,options.offset+100),totalLength:items.length};}};
 const sgf={FS:'',RS:'',Icons:{},platform:{getPlaylistAPI:()=>api},
   notify:text=>messages.push(text),send:(...args)=>sends.push(args),
   requestStatuses:async page=>{batches.push(page.length);return new Map(page.map(t=>[t.uri,{status:'DONE',path:'C:/Music/'+t.title+'.mp3'}]));}};
 vm.runInNewContext(fs.readFileSync('native/ui/classic_m3u.js','utf8'),{
   window:{__SoggfyClassic:sgf},location:{pathname:'/playlist/test'},document:{querySelector:()=>null},
   console:{warn(){}},setTimeout,clearTimeout});
 return {sgf,api,sends,messages,offsets,batches};
}
(async()=>{
 const h=harness([...tracks,tracks[0]]);await h.sgf.generateM3U();
 assert.deepStrictEqual(h.offsets,[0,100,200]);assert.deepStrictEqual(h.batches,[128,128,1]);
 assert.strictEqual(h.sends.length,1);const records=h.sends[0][1].split('');
 assert.strictEqual(records.length,259);assert.strictEqual(records[1],records.at(-1),'Playlist duplicates retain order');
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
