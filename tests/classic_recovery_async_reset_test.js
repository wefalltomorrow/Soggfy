'use strict';
const assert=require('node:assert/strict');
const fs=require('node:fs'),vm=require('node:vm');

function deferred(){
  let resolve,reject;
  const promise=new Promise((yes,no)=>{resolve=yes;reject=no;});
  return {promise,resolve,reject};
}
function harness(stage='insert',queued=true){
  let now=0;
  const pending=deferred(),entered=deferred();
  const calls={queue:0,insert:0,skip:0,seek:0};
  const reports=[];
  const playback={track:{uri:'spotify:track:A',duration:150000},is_paused:false,is_playing:true};
  const window={};
  const context={window,document:{getElementById:()=>true,querySelector:()=>null},
    console:{info(){},warn(){},error(){}},URLSearchParams,encodeURIComponent,setTimeout(){}};
  for(const file of ['classic_core.js','classic_recovery.js'])
    vm.runInNewContext(fs.readFileSync('native/ui/'+file,'utf8'),context);
  const sgf=window.__SoggfyClassic;
  Object.assign(sgf.state,{downloads:true,speedSupported:true,playbackSpeed:30});
  const wait=async at=>{if(stage===at){entered.resolve();await pending.promise;}};
  sgf.player={getState:()=>playback,_queue:{
    async getQueue(){calls.queue++;await wait('queue');return {queued:queued?[{uri:'spotify:track:B'}]:[]};},
    async insertIntoQueue(){calls.insert++;await wait('insert');},
    async addToQueue(){calls.insert++;await wait('insert');}
  },async skipToNext(){calls.skip++;await wait('skip');},async seekTo(){calls.seek++;}};
  const monitor=sgf.createPlaybackRecoveryMonitor({now:()=>now,
    report:(action,uri,age,details)=>reports.push({action,uri,age,details})});
  return {sgf,playback,calls,pending,entered,monitor,reports,
    setTime:t=>{now=t;},pause:()=>{playback.is_paused=true;playback.is_playing=false;},
    resume:()=>{playback.is_paused=false;playback.is_playing=true;}};
}
async function begin(h){
  await h.monitor.tick();h.setTime(18000);
  const running=h.monitor.tick();await h.entered.promise;return {running};
}

(async()=>{
  // Exercise the REAL reset helper, not a mock that instantly resolves.
  for(const stage of ['queue','insert'])for(const queued of [true,false]){
    const h=harness(stage,queued),{running}=await begin(h);
    h.pause();h.pending.resolve();await running;
    assert.equal(h.calls.skip,0,`Pause during ${stage} must not advance playback`);
    if(stage==='queue')assert.equal(h.calls.insert,0,'No stale queue insertion after pause');
    assert.equal(h.monitor.snapshot().attempts,0,'An observed pause clears retry budget immediately');
    h.resume();h.setTime(90000);await h.monitor.tick();
    assert.equal(h.calls.skip,0,'Resume gets a fresh grace period even without a paused timer tick');
    h.setTime(107999);await h.monitor.tick();assert.equal(h.calls.skip,0);
    h.setTime(108000);await h.monitor.tick();assert.equal(h.calls.skip,1);
  }
  for(const stage of ['queue','insert'])for(const change of ['downloads','speed','support','track','unknown','player']){
    const h=harness(stage),{running}=await begin(h);
    if(change==='downloads')h.sgf.state.downloads=false;
    if(change==='speed')h.sgf.state.playbackSpeed=1;
    if(change==='support')h.sgf.state.speedSupported=false;
    if(change==='track')h.playback.track.uri='spotify:track:B';
    if(change==='unknown'){delete h.playback.is_paused;delete h.playback.is_playing;}
    if(change==='player')h.sgf.player={...h.sgf.player};
    h.pending.resolve();await running;
    assert.equal(h.calls.skip,0,`${change} during ${stage} cancels retry and fallback skip`);
    if(stage==='queue')assert.equal(h.calls.insert,0,'Cancelled lookup cannot insert a stale track');
  }
  // A pause observed while reset is pending must invalidate that operation,
  // even if playback resumes before the queue promise settles.
  {
    const h=harness(),{running}=await begin(h);
    h.pause();h.setTime(19000);await h.monitor.tick();
    h.resume();h.setTime(20000);await h.monitor.tick();
    h.pending.resolve();await running;
    assert.equal(h.calls.skip,0,'A suspended retry cannot revive after resume');
    assert(h.reports.some(r=>r.action==='pause_observed'));
    assert(h.reports.some(r=>r.action==='resume_observed'));
  }
  // Clear invalidates an outstanding operation but must retain the lock until
  // it settles; otherwise two resets can act on the same queue concurrently.
  {
    const h=harness(),{running}=await begin(h);
    h.monitor.clear();h.setTime(20000);await h.monitor.tick();
    h.setTime(40000);await h.monitor.tick();
    assert.equal(h.calls.queue,1,'Clear cannot start a second pending reset');
    h.pending.resolve();await running;assert.equal(h.calls.skip,0);
  }
  // A rejected queue operation must not bypass changed download settings.
  {
    const h=harness(),{running}=await begin(h);
    h.sgf.state.downloads=false;h.pending.reject(Error('queue failed'));await running;
    assert.equal(h.calls.skip,0,'Failed reset cannot fallback-skip with Downloads off');
  }
  // Uninterrupted recovery still requeues and advances exactly once.
  {
    const h=harness(),{running}=await begin(h);
    h.pending.resolve();await running;assert.equal(h.calls.skip,1);
    assert.equal(h.monitor.snapshot().attempts,1);
  }
  console.log('PASS: real async reset guards, cancellation, pause/resume grace and single in-flight retry');
})().catch(error=>{console.error(error);process.exitCode=1;});
