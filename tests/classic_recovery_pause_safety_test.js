'use strict';
const assert=require('node:assert/strict');
const fs=require('node:fs'),vm=require('node:vm');
const reported=[];
const sgf={state:{downloads:true,speedSupported:true,playbackSpeed:30}};
const context={window:{__SoggfyClassic:sgf},console,
  document:{querySelector:()=>null},setInterval:()=>0};
vm.runInNewContext(fs.readFileSync('native/ui/classic_recovery.js','utf8'),context);
const report=(action,uri,age,details)=>reported.push({action,uri,age,details});

// A user can pause Spotify after a watchdog tick begins but before its reset.
// No retry or skip is permissible once the live state is paused.
(async()=>{
 let now=0,mode='playing',reads=0,resetCount=0,skipCount=0,flip=false;
 const getState=()=>({item:{uri:'spotify:track:pause-race',duration:150000},
   is_paused:mode==='paused',is_playing:mode==='playing',position:12000});
 const read=()=>{
   reads++;
   if(flip&&reads===2)mode='paused';
   return sgf.readRecoveryState(getState());
 };
 const first=sgf.createPlaybackRecoveryMonitor({
   now:()=>now,getState,read,report,
   reset:async()=>{resetCount++;return true;},
   skip:async()=>{skipCount++}
 });
 await first.tick(); // establishes initial track identity
 now=18000;reads=0;flip=true;
 await first.tick();
 assert.equal(resetCount,0,'Pause between first read and retry blocks reset');
 assert.equal(skipCount,0,'Pause race never skips');
 assert(reported.some(x=>x.action==='recovery_aborted_state_changed' &&
   x.details.includes('phase=before_retry')));
 flip=false;now=19500;
 await first.tick();
 assert(reported.some(x=>x.action==='pause_observed' &&
   x.details.includes('source=player-flags')),'Pause transition recorded');
 const observedPauses=reported.filter(x=>x.action==='pause_observed').length;
 now=27000;await first.tick();
 assert.equal(reported.filter(x=>x.action==='pause_observed').length,observedPauses,
   'Stable pause does not flood the log');
 mode='playing';now=28000;await first.tick();
 assert(reported.some(x=>x.action==='resume_observed'),'Resume transition recorded');
 now=45999;await first.tick();
 assert.equal(resetCount,0,'Paused time does not count toward next recovery');
 now=46000;await first.tick();
 assert.equal(resetCount,1,'Subsequent sustained playing stall may retry');

 // A failed asynchronous reset can leave Spotify paused; do not immediately
 // skip the song based on a stale playing state.
 let lateNow=0,paused=false,lateRetries=0,lateSkips=0;
 const lateState=()=>({track:{uri:'spotify:track:after-async',duration:150000},
   is_paused:paused,is_playing:!paused});
 const second=sgf.createPlaybackRecoveryMonitor({
   now:()=>lateNow,getState:lateState,report,
   reset:async()=>{lateRetries++;paused=true;return false;},
   skip:async()=>{lateSkips++}
 });
 await second.tick();lateNow=18000;await second.tick();
 assert.equal(lateRetries,1,'Reset attempted while actively playing');
 assert.equal(lateSkips,0,'A pause after failed async reset blocks the fallback skip');
 assert(reported.some(x=>x.action==='recovery_aborted_state_changed' &&
   x.details.includes('phase=before_skip')));
 lateNow=50000;await second.tick();
 assert.equal(lateSkips,0,'Paused song is never skipped');
 assert.equal(second.snapshot().attempts,0,'Pausing clears the old retry budget');

 const conflict=sgf.readRecoveryState({item:{uri:'spotify:track:conflicting'},
   isPaused:true,isPlaying:true});
 assert.equal(conflict.mode,'paused');
 assert.equal(conflict.source,'conflicting-flags');
 const initialPause=sgf.createPlaybackRecoveryMonitor({
   now:()=>lateNow,
   getState:()=>({item:{uri:'spotify:track:initial-pause',duration:120000},
     is_paused:true,is_playing:false}),
   report,reset:async()=>{throw Error('Must never reset initial pause');}
 });
 await initialPause.tick();
 assert(reported.some(x=>x.action==='track_started_paused'),
   'Tracks first observed paused have diagnostic coverage');
 lateNow+=90000;await initialPause.tick();
 assert.equal(initialPause.snapshot().attempts,0);
 console.log('PASS: RC62 pause transition diagnostics, async race safety, and retry reset');
})().catch(error=>{console.error(error);process.exitCode=1;});
