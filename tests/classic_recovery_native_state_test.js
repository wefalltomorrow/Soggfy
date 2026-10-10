'use strict';
const assert=require('assert');
const fs=require('fs');
const vm=require('vm');
let now=0,playing=true,uri='spotify:track:stalled',attempts=0,skips=0;
const log=[],timers=[];
const sgf={
 state:{downloads:true,speedSupported:true,speedImmediate:true,playbackSpeed:50},
 send:(key,value)=>log.push(key+':'+value),
 currentState:()=>({track:{uri,duration:197903},duration:197903,
   is_paused:!playing,is_playing:playing})
};
const document={querySelector:()=>null};
const context={window:{__SoggfyClassic:sgf},document,console,
 setInterval(fn,ms){timers.push({fn,ms})}};
vm.runInNewContext(fs.readFileSync('native/ui/classic_recovery.js','utf8'),context,
 {filename:'classic_recovery.js'});
assert.strictEqual(sgf.readRecoveryState(sgf.currentState()).uri,uri);
assert.strictEqual(sgf.readRecoveryState(sgf.currentState()).mode,'playing');
assert.strictEqual(sgf.readRecoveryState(sgf.currentState()).durationMs,197903);
const watcher=sgf.createPlaybackRecoveryMonitor({
 now:()=>now,getState:()=>sgf.currentState(),
 reset:async()=>{attempts++;return true;},
 skip:async()=>{skips++},
 report:(action)=>log.push(action)
});
(async()=>{
 now=0;await watcher.tick();
 now=16000;await watcher.tick();
 assert.strictEqual(attempts,0,'No early retry before 18s watchdog deadline');
 now=18000;await watcher.tick();
 assert.strictEqual(attempts,1,'First retry for raw snake_case Spotify state');
 now=36000;await watcher.tick();
 assert.strictEqual(attempts,2,'Second retry after 18 seconds');
 now=54000;await watcher.tick();
 assert.strictEqual(skips,1,'Skip after two retries when stuck');
 now=72000;await watcher.tick();
 assert.strictEqual(skips,1,'At most one skip for same track');

 uri='spotify:track:paused';playing=false;
 now=73000;await watcher.tick();
 now=120000;await watcher.tick();
 assert.strictEqual(attempts,2,'Manual pause never triggers a retry');
 playing=true;
 now=121000;await watcher.tick();
 now=139000;await watcher.tick();
 assert.strictEqual(attempts,3,'Resume allows later recovery');

 const camel=sgf.readRecoveryState({
  item:{uri:'spotify:track:camel',duration:100000},
  isPaused:false,isPlaying:true
 });
 assert.strictEqual(camel.uri,'spotify:track:camel');
 assert.strictEqual(camel.mode,'playing');
 assert.strictEqual(camel.durationMs,100000);
 assert.strictEqual(sgf.readRecoveryState({
  track:{uri:'spotify:track:pause',duration:100000},
  is_paused:true,is_playing:false
 }).mode,'paused');

 document.querySelector=selector=>selector.includes('control-button-pause')?{}:null;
 assert.strictEqual(sgf.readRecoveryState({track:{uri:'spotify:track:unknown'}}).mode,'playing',
   'Fallback to actual transport pause button only when flags unavailable');
 document.querySelector=selector=>selector.includes('control-button-play')?{}:null;
 assert.strictEqual(sgf.readRecoveryState({track:{uri:'spotify:track:unknown'}}).mode,'paused');
 document.querySelector=()=>null;
 assert.strictEqual(sgf.readRecoveryState({track:{uri:'spotify:track:unknown'}}).mode,'unknown');

 sgf.installPlaybackStallRecovery();
 assert(timers.some(t=>t.ms===1500));
 assert(log.some(t=>t.includes('watchdog_initialized')),'Diagnostics identify active recovery');
 console.log('PASS: native Spotify snake_case state, pauses, RC51 50x stall recovery, retries and bounded skip');
})().catch(error=>{console.error(error);process.exitCode=1});
