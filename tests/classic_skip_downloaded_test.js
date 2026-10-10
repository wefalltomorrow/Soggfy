'use strict';
const assert=require('assert');
const fs=require('fs');
const vm=require('vm');

// This tests the upstream Soggfy PlayerStateTracker queue_update contract
// against the modern Spotify ContextTrack / ProvidedTrack variants.
const listeners={};
const logs=[],removed=[],requests=[];
let nativeQueueCalls=0;
const player={
  getEvents(){return {addListener(name,fn){listeners[name]=fn;}};},
  _queue:{getQueue(){nativeQueueCalls++;return {queued:[]};}},
  async removeFromQueue(tracks){removed.push(tracks.map(t=>({...t}))); }
};
const sgf={
 Icons:{},FS:'\x1f',RS:'\x1e',
 statusMap:new Map(),state:{skipDownloaded:true,skipIgnored:false,debug:true},
 player, currentState:()=>null,
 send:(key,value)=>logs.push(key+':'+value),
 isTrackIgnored:item=>!!item?.album?.ignored,
 parseHtml:x=>x,installPlaybackStallRecovery(){}
};
const win={__SoggfyClassic:sgf,Spicetify:{}};
const context={
 window:win,
 document:{body:{},querySelector:()=>null,visibilityState:'visible'},
 Element:class Element{},localStorage:{getItem:()=>'[]'},
 MutationObserver:class{observe(){}},
 setTimeout(){return 1;},clearTimeout(){},setInterval(){},
 console,location:{pathname:'/playlist/test'}
};
vm.runInNewContext(fs.readFileSync('native/ui/classic_status.js','utf8'),context,
  {filename:'classic_status.js'});
sgf.installPlayerListeners();
const uri=n=>'spotify:track:'+String(n).padStart(22,'A');
const original=(n,{downloaded=true,uid='uid-'+n}={})=>({
 uri:uri(n),uid,name:'Song '+n,
 artists:[{name:'Band'}],album:{name:'Album'},
 downloaded
});
const modern=(n,{downloaded=true,uid='modern-'+n}={})=>({
 uri:uri(n),uid,
 metadata:{title:'Song '+n,artist_name:'Band',album_title:'Album'},
 downloaded
});
const answer=(items)=>new Map(items.map(item=>[item.uri,
  {status:item.track?.downloaded===false?'':'DONE',path:'Music\\Band\\'+item.title+'.mp3'}]));
let lookup=answer;
sgf.requestStatuses=async infos=>{
 requests.push(infos.map(t=>({uri:t.uri,title:t.title,album:t.album,artist:t.artist})));
 return lookup(infos);
};

(async()=>{
 // Event data takes precedence over a stale getQueue().queued = [].
 const a=original('one'),b=original('two',{downloaded:false});
 lookup=infos=>new Map(infos.map(t=>[t.uri,{status:t.uri===a.uri?'DONE':''}]));
 await listeners.queue_update({data:{nextUp:[a,b]}});
 assert.strictEqual(nativeQueueCalls,0,'Must use original queue_update.nextUp');
 assert.strictEqual(removed.length,1);
 assert.strictEqual(removed[0].length,1);
 assert.strictEqual(removed[0][0].uri,a.uri);
 assert.strictEqual(removed[0][0].uid,a.uid,'Pass queue UID to PlayerAPI');
 assert.strictEqual(requests[0].length,2);
 assert.strictEqual(requests[0][0].album,'Album');

 // Upstream cache: avoid needlessly rescanning unchanged upcoming songs.
 await listeners.queue_update({data:{nextUp:[a,b]}});
 assert.strictEqual(requests.length,1,'Repeated queue event uses confirmed status cache');

 // A timeout/partial response must not poison the queue cache.
 const c=modern('three');
 let failedOnce=true;
 lookup=infos=>{
   if(failedOnce){failedOnce=false;return new Map();}
   return new Map(infos.map(t=>[t.uri,{status:'DONE'}]));
 };
 await listeners.queue_update({data:{nextUp:[c]}});
 assert.strictEqual(removed.length,2,'Cached completed track a can still be removed again');
 // The last removal above comes from prior repeated queue, not this incomplete lookup.
 await listeners.queue_update({data:{nextUp:[c]}});
 assert.strictEqual(requests.length,3,'Unanswered native status retried');
 assert.strictEqual(removed.at(-1)[0].uid,c.uid);

 // Modern Spotify's ProvidedTrack metadata and Spicetify.Queue.nextTracks.
 const d=modern('four');
 win.Spicetify.Queue={nextTracks:[d]};
 lookup=infos=>new Map(infos.map(x=>[x.uri,{status:'DONE'}]));
 await sgf.checkQueue();
 assert.strictEqual(removed.at(-1)[0].uri,d.uri);
 assert.strictEqual(requests.at(-1)[0].album,'Album');

 // Modern Spotify's raw player state fallback.
 delete win.Spicetify.Queue;
 const e=modern('five');
 sgf.currentState=()=>({next_tracks:[e]});
 await sgf.checkQueue();
 assert.strictEqual(removed.at(-1)[0].uid,e.uid);

 // Spotify's _queue.getQueue.nextUp is supported when no event/state source.
 sgf.currentState=()=>null;
 const f=original('six');
 player._queue.getQueue=()=>({nextUp:[f],queued:[]});
 await sgf.checkQueue();
 assert.strictEqual(removed.at(-1)[0].uid,f.uid);

 // Same URI can occur twice; preserve individual UIDs when removing.
 const g=original('seven'),g2={...g,uid:'duplicate-uid'};
 await sgf.checkQueue({nextUp:[g,g2]});
 const duplicates=removed.at(-1);
 assert.strictEqual(duplicates.length,2);
 assert.deepStrictEqual(duplicates.map(t=>t.uid),[g.uid,g2.uid]);

 // Ignore-list selection uses the original full queue metadata.
 sgf.state.skipDownloaded=false;sgf.state.skipIgnored=true;
 const ignored={...original('eight'),album:{name:'Album',ignored:true}};
 const ignoredReq=requests.length;
 await sgf.checkQueue({nextUp:[ignored]});
 assert.strictEqual(requests.length,ignoredReq,'Ignore-only mode must not look up disk');
 assert.strictEqual(removed.at(-1)[0].uid,ignored.uid);

 // No known queue shape must not be treated as a checked empty queue.
 sgf.state.skipDownloaded=true;sgf.state.skipIgnored=false;
 player._queue.getQueue=()=>({somethingUnrelated:[original('nine')]});
 const priorRequests=requests.length;
 await sgf.checkQueue();
 assert.strictEqual(requests.length,priorRequests);
 const i=modern('nine');
 await sgf.checkQueue({nextUp:[i]});
 assert.strictEqual(removed.at(-1)[0].uri,i.uri,'Retry after unavailable snapshot');

 // The native protocol supports 128 tracks per status request.
 const many=Array.from({length:130},(_,idx)=>modern('bulk'+idx));
 const before=requests.length;
 lookup=infos=>new Map(infos.map(x=>[x.uri,{status:''}]));
 await sgf.checkQueue({nextUp:many});
 assert.strictEqual(requests.length-before,2,'Do not truncate larger nextUp lists');
 assert.strictEqual(requests.at(-2).length,128);
 assert.strictEqual(requests.at(-1).length,2);

 console.log('PASS: original queue_update nextUp, modern metadata, UID removal, incomplete response retries, fallback sources, cache and 128-item batches');
})().catch(e=>{console.error(e);process.exitCode=1});
