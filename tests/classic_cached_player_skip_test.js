'use strict';
const assert=require('node:assert/strict');
const fs=require('node:fs');
const vm=require('node:vm');

const events={},logs=[],removed=[],lookup=[];
const uri='spotify:track:0000000000000000000001';
const queueUri='spotify:track:0000000000000000000002';
const player={
  getState(){return {item:{uri,metadata:{title:'Present',artist_name:'Artist',album_title:'Album',duration:'200000'}}};},
  getEvents(){return {addListener(name,callback){events[name]=callback;}};},
  async removeFromQueue(items){removed.push(items.map(t=>({uri:t.uri,uid:t.uid})));},
  _queue:{getQueue(){return {queued:[]};}}
};
// This is the actual service registry shape which native/metadata_collector.js
// already discovers on modern Spotify; the old Platform.props path is absent.
const registry={_map:new Map([[Symbol.for('PlayerAPI'),{instance:player}]])};
const main={'__reactContainer$test':{memoizedProps:{value:{registry}}}};
const fakeDocument={
  body:{},head:{appendChild(){}},
  getElementById(){return true;},
  querySelector(selector){return selector==='#main'?main:null;}
};
const output=[];
let clock=0;
const fakeConsole={
  info(msg){output.push(String(msg));},
  log(){},warn(msg){output.push('warn: '+String(msg));}
};
const fakeWindow={__soggfyMetadataEnabled:false};
const context={
  window:fakeWindow,document:fakeDocument,console:fakeConsole,
  performance:{now(){return clock++;}},URLSearchParams,
  encodeURIComponent,localStorage:{getItem(){return '[]';}},
  location:{pathname:'/playlist/abc'},
  Element:class Element{},
  MutationObserver:class{observe(){}},
  setInterval(){},clearTimeout(){},setTimeout(fn){return 1;},
  queueMicrotask
};
vm.createContext(context);
function evaluate(p){vm.runInContext(fs.readFileSync(p,'utf8'),context,{filename:p});}
evaluate('native/ui/classic_core.js');
evaluate('native/ui/classic_status.js');
const sgf=fakeWindow.__SoggfyClassic;
sgf.state.skipDownloaded=true;
sgf.state.skipIgnored=false;
sgf.state.debug=true;
sgf.requestStatuses=async infos=>{
  lookup.push(infos.map(v=>({uri:v.uri,title:v.title,artist:v.artist,album:v.album})));
  return new Map(infos.map(i=>[i.uri,{status:'DONE',path:'Music\\Artist - Existing.mp3'}]));
};
assert.equal(fakeWindow.Spicetify,undefined,'Repro must have no injected Spicetify Platform');
assert.equal(fakeWindow.__soggfyVerifiedPlayerAPI,undefined,'No cached player before discovery');
// The native metadata collector's real traversal must find a PlayerAPI in its
// Map-backed service registry even if optional metadata enrichment is off.
evaluate('native/metadata_collector.js');
// The collector intentionally walks only ~4ms of React cache per tick.
// Real Spotify invokes it every second; let the same cursor progress here.
for(let i=0;i<40&&!fakeWindow.__soggfyVerifiedPlayerAPI;i++)
  fakeWindow.__floggfyPoll();
assert.equal(fakeWindow.__soggfyVerifiedPlayerAPI,player,
  'Collector must expose the verified cached PlayerAPI despite metadata disabled');
(async()=>{
 await sgf.initPlayer();
 assert.equal(sgf.player,player,'Classic UI must use actual cached Spotify player');
 assert.equal(typeof events.queue_update,'function','Original queue_update listener installed');
 assert.ok(output.some(s=>s.includes('queue player ready source=verified-cached-player skipDownloaded=1')),
   'Readiness diagnostics must show the actual source and enabled skip toggle');
 const future={
   uri:queueUri,uid:'queue-entry-1',
   name:'Existing',artists:[{name:'Artist'}],album:{name:'Album'}
 };
 await events.queue_update({data:{nextUp:[future]}});
 assert.equal(lookup.length,1,'Original queue_update requests on-disk status exactly once');
 assert.equal(lookup[0][0].title,'Existing');
 assert.equal(removed.length,1,'Player.removeFromQueue invoked for DONE file');
 assert.equal(removed[0][0].uri,queueUri);
 assert.equal(removed[0][0].uid,'queue-entry-1');
 // Already checked files do not cause new requests on the same queue.
 await events.queue_update({data:{nextUp:[future]}});
 assert.equal(lookup.length,1,'Per-URI status cache preserved');
 console.log('PASS: cached Spotify player registry -> native collector -> original queue_update -> file status -> removeFromQueue');
})().catch(e=>{process.stderr.write(String(e.stack||e)+'\n');process.exitCode=1;});