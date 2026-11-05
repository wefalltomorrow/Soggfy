'use strict';
const assert=require('assert'),fs=require('fs'),vm=require('vm');
function harness(){
 let now=100000,nextTimer=0;
 const timers=new Map(),sent=[],removed=[];
 const sgf={Icons:{},FS:'\x1f',RS:'\x1e',statusMap:new Map(),state:{skipDownloaded:true},
   send:(k,v)=>sent.push([k,v]),player:{async removeFromQueue(x){removed.push(x);}},currentState:()=>null};
 const window={__SoggfyClassic:sgf};
 vm.runInNewContext(fs.readFileSync('native/ui/classic_status.js','utf8'),{
   window,document:{body:{},querySelector:()=>null},location:{pathname:''},Element:class{},
   localStorage:{getItem:()=>'[]'},MutationObserver:class{observe(){}},console:{warn(){}},
   Date:class extends Date{static now(){return now;}},
   setTimeout(fn,ms){timers.set(++nextTimer,{fn,ms});return nextTimer;},clearTimeout:id=>timers.delete(id),setInterval(){}
 });
 return {sgf,window,sent,removed,timers,advance:ms=>now+=ms,
   expire(){const [id,timer]=[...timers].find(([,t])=>t.ms===10000);timers.delete(id);timer.fn();},
   reply(payload,rows=''){window.__soggfyReceiveStatuses(payload.split('\x1d')[0]+'\x1d'+rows);}};
}
const info=n=>({uri:'spotify:track:'+n,title:'Song '+n,artist:'Artist',album:'Album',allArtists:'Artist'});
const row=(i,status)=>[i.uri,status,'path',''].join('\x1f');
const queued=n=>({uri:info(n).uri,uid:'uid-'+n,name:info(n).title,artists:[{name:'Artist'}],album:{name:'Album'}});
const tick=async()=>{for(let i=0;i<24;i++)await Promise.resolve();};
(async()=>{
 const h=harness(),{sgf}=h,a=info('A'),b=info('B');
 const pa=sgf.requestStatuses([a]),first=h.sent[0][1];
 assert.strictEqual(sgf.requestStatuses([a]),pa,'Identical work shares one promise');
 const pb=sgf.requestStatuses([b]);h.expire();assert.strictEqual((await pa).size,0);
 const second=h.sent[1][1];h.reply(first,row(a,'DONE'));
 assert(sgf.statusActive,'Late A cannot complete B');assert.strictEqual(sgf.statusMap.size,0);
 h.reply(second,row(a,'DONE')+'\x1e'+row(b,'DONE'));
 const answer=await pb;assert.strictEqual(answer.size,1);assert(answer.has(b.uri));
 const empty=sgf.requestStatuses([a]);h.reply(h.sent.at(-1)[1]);assert.strictEqual((await empty).size,0);
 sgf.send=()=>{throw Error('send failed');};assert.strictEqual((await sgf.requestStatuses([a])).size,0);assert(!sgf.statusActive);
 sgf.send=(k,v)=>h.sent.push([k,v]);
 const pending=Array.from({length:30},(_,n)=>sgf.requestStatuses([info('bound'+n)]));
 assert.strictEqual(sgf.statusQueue.length,sgf.statusLimits.pending);
 sgf.invalidateStatusCache();assert((await Promise.all(pending)).every(m=>m.size===0));
 for(let i=0;i<1030;i++){const item=info('map'+i),p=sgf.requestStatuses([item]);h.reply(h.sent.at(-1)[1],row(item,'DONE'));await p;}
 assert.strictEqual(sgf.statusMap.size,sgf.statusLimits.map);
 const old=sgf.requestStatuses([a]),oldWire=h.sent.at(-1)[1];h.window.__soggfyStatusRevision(1);h.window.__soggfyStatusRevision(2);
 assert.strictEqual((await old).size,0);h.reply(oldWire,row(a,'DONE'));assert.strictEqual(sgf.statusMap.size,0);

 const q=harness();let requests=0,status='',fail=false;
 q.sgf.requestStatuses=async items=>{requests++;return new Map(items.map(i=>[i.uri,{status}]));};
 q.sgf.player.removeFromQueue=async x=>{if(fail)throw Error('retry');q.removed.push(x);};
 await q.sgf.checkQueue({nextUp:[queued('one')]});status='DONE';
 await q.sgf.checkQueue({nextUp:[queued('one')]});assert.strictEqual(requests,1);
 q.advance(3000);fail=true;await q.sgf.checkQueue({nextUp:[queued('one')]});assert.strictEqual(requests,2);
 status='';fail=false;await q.sgf.checkQueue({nextUp:[queued('one')]});assert.strictEqual(requests,3);assert.strictEqual(q.removed.length,0);
 status='DONE';q.sgf.invalidateStatusCache();await q.sgf.checkQueue({nextUp:[queued('one')]});assert.strictEqual(q.removed.length,1);
 q.advance(3000);status='';await q.sgf.checkQueue({nextUp:[queued('one')]});assert.strictEqual(q.removed.length,1);

 // A newer queue supersedes a suspended lookup, preserving its different UID.
 const r=harness();let resolve;
 r.sgf.requestStatuses=()=>new Promise(done=>{resolve=done;});
 const running=r.sgf.checkQueue({nextUp:[queued('old')]});await tick();
 await r.sgf.checkQueue({nextUp:[queued('new')]});
 resolve(new Map([[info('old').uri,{status:'DONE'}]]));await running;
 assert.strictEqual(r.removed.length,0);
 const replay=[...r.timers.values()].find(t=>t.ms===0);assert(replay);replay.fn();await tick();
 resolve(new Map([[info('new').uri,{status:'DONE'}]]));await tick();
 assert.strictEqual(r.removed[0][0].uid,'uid-new');
 // Changing the player while waiting also prevents stale removals.
 r.sgf.invalidateStatusCache();const swap=r.sgf.checkQueue({nextUp:[queued('swap')]});await tick();
 r.sgf.player={async removeFromQueue(){throw Error('wrong player');}};
 resolve(new Map([[info('swap').uri,{status:'DONE'}]]));await swap;assert.strictEqual(r.removed.length,1);
 // Disabled events cannot later be replayed against a new player.
 const disabled=harness();disabled.sgf.state.skipDownloaded=false;
 await disabled.sgf.checkQueue({nextUp:[queued('obsolete')]});
 disabled.sgf.player={_queue:{getQueue:async()=>({nextUp:[queued('current')]})},
   async removeFromQueue(items){disabled.removed.push(items);}};
 disabled.sgf.state.skipDownloaded=true;
 disabled.sgf.requestStatuses=async items=>new Map(items.map(i=>[i.uri,{status:'DONE'}]));
 await disabled.sgf.checkQueue();assert.strictEqual(disabled.removed[0][0].uid,'uid-current');
 // A pending event saved during active work must also belong to the player consuming it.
 const owner=harness();let complete;
 owner.sgf.requestStatuses=()=>new Promise(done=>{complete=done;});
 const active=owner.sgf.checkQueue({nextUp:[queued('active')]});await tick();
 await owner.sgf.checkQueue({nextUp:[queued('pending')]});
 owner.sgf.player={_queue:{getQueue:async()=>({nextUp:[queued('replacement')]})},
   async removeFromQueue(items){owner.removed.push(items);}};
 complete(new Map());await active;
 owner.sgf.requestStatuses=async items=>new Map(items.map(i=>[i.uri,{status:'DONE'}]));
 await owner.sgf.checkQueue();assert.strictEqual(owner.removed[0][0].uid,'uid-replacement');
 // All visible rows are paged, and duplicate URIs consume no extra slots.
 const visible=harness(),infos=Array.from({length:257},(_,i)=>info('visible'+i));
 visible.sgf.trackInfoFromRows=()=>[...infos,infos[0]];visible.sgf.renderVisibleStatuses=()=>{};
 visible.sgf.refreshVisibleStatuses();
 const refresh=[...visible.timers.values()].find(t=>t.ms===120).fn();
 let pages=0;
 while(visible.sgf.statusActive){
   ++pages;const job=visible.sgf.statusActive;
   visible.reply(visible.sent.at(-1)[1],job.infos.map(i=>row(i,'DONE')).join('\x1e'));await tick();
 }
 await refresh;assert.strictEqual(pages,3);assert.strictEqual(visible.sgf.statusMap.size,257);
 // Invalidation between pages cancels the rest of the obsolete refresh.
 visible.sgf.refreshVisibleStatuses();
 const obsoleteRefresh=[...visible.timers.values()].filter(t=>t.ms===120).at(-1).fn();
 const sends=visible.sent.length;visible.sgf.invalidateStatusCache();await obsoleteRefresh;
 assert.strictEqual(visible.sent.length,sends);
 // A queue read that never settles must not disable all subsequent work.
 const hung=harness();let finishRead;
 hung.sgf.player._queue={getQueue:()=>new Promise(resolve=>{finishRead=resolve;})};
 const stuck=hung.sgf.checkQueue();await tick();hung.expire();await stuck;
 hung.sgf.requestStatuses=async items=>new Map(items.map(i=>[i.uri,{status:'DONE'}]));
 await hung.sgf.checkQueue({nextUp:[queued('after-timeout')]});assert.strictEqual(hung.removed.length,1);
 finishRead({nextUp:[queued('late')]});await tick();assert.strictEqual(hung.removed.length,1);
 // Replacement player can progress immediately; old finally cannot unlock its work.
 const swapped=harness();let oldRead,newRead;
 swapped.sgf.player._queue={getQueue:()=>new Promise(resolve=>{oldRead=resolve;})};
 const oldRun=swapped.sgf.checkQueue();await tick();
 swapped.sgf.player={_queue:{getQueue:()=>new Promise(resolve=>{newRead=resolve;})},
   removeFromQueue:async items=>swapped.removed.push(items)};
 swapped.sgf.requestStatuses=async items=>new Map(items.map(i=>[i.uri,{status:'DONE'}]));
 const newRun=swapped.sgf.checkQueue();await tick();assert(newRead);
 oldRead({nextUp:[queued('old-player')]});await oldRun;
 await swapped.sgf.checkQueue(); // Must not start a second read on this player.
 newRead({nextUp:[queued('new-player')]});await newRun;assert.strictEqual(swapped.removed.length,1);
 // A timed-out mutation remains reserved, including after invalidation.
 const mutation=harness();let finishRemove,calls=0;
 mutation.sgf.requestStatuses=async items=>new Map(items.map(i=>[i.uri,{status:'DONE'}]));
 mutation.sgf.player.removeFromQueue=items=>{calls++;mutation.removed.push(items);
   return calls===1?new Promise(resolve=>{finishRemove=resolve;}):Promise.resolve();};
 const removal=mutation.sgf.checkQueue({nextUp:[queued('reserved')]});await tick();
 mutation.expire();await removal;mutation.sgf.invalidateStatusCache();
 await mutation.sgf.checkQueue({nextUp:[queued('reserved'),{...queued('reserved'),uid:'another-copy'}]});
 assert.strictEqual(calls,2);assert.strictEqual(mutation.removed[1].length,1);
 assert.strictEqual(mutation.removed[1][0].uid,'another-copy');
 finishRemove();await tick();mutation.advance(3000);
 await mutation.sgf.checkQueue({nextUp:[queued('reserved')]});assert.strictEqual(calls,3);
 console.log('PASS: correlated statuses, pagination, queue timeout recovery, player replacement, reserved mutations and stale completions');
})().catch(e=>{console.error(e);process.exitCode=1;});
