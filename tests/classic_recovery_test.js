'use strict';

const fs=require('fs');
const vm=require('vm');
const assert=require('assert');

let time=0,allowed=true,track='spotify:track:A',paused=false,speed=50;
let attempts=0,skips=0;
const reports=[],intervals=[];
const sgf={state:{downloads:true,speedSupported:true,speedImmediate:true,playbackSpeed:50},
    send(k,v){reports.push(k+':'+v);}};
const context={
  window:{__SoggfyClassic:sgf},
  setInterval(fn,ms){intervals.push({fn,ms});return intervals.length;},
  console
};
vm.runInNewContext(fs.readFileSync('native/ui/classic_recovery.js','utf8'),context,
    {filename:'classic_recovery.js'});

const state=()=>({item:{uri:track,duration:200000},isPaused:paused});
const monitor=sgf.createPlaybackRecoveryMonitor({
    now:()=>time,getState:state,allowed:()=>allowed,speed:()=>speed,
    async reset(){attempts++;return true;},
    async skip(){skips++;},
    report:(action,uri)=>reports.push(action+':'+uri)
});
async function tick(t){time=t;await monitor.tick();}
(async()=>{
  await tick(0);
  await tick(17000);
  assert.strictEqual(attempts,0,'No premature retry during accelerated playback');
  await tick(18000);
  assert.strictEqual(attempts,1,'First stalled-track retry after grace period');
  assert.strictEqual(skips,0);

  // The recovery must not reset the track immediately again.
  await tick(18001);
  assert.strictEqual(attempts,1);

  await tick(36000);
  assert.strictEqual(attempts,2,'Second retry after a fresh grace period');
  await tick(54000);
  assert.strictEqual(skips,1,'Repeatedly stuck track falls back to Next');
  await tick(90000);
  assert.strictEqual(skips,1,'Only one skip per track identity');

  track='spotify:track:B';
  await tick(91000);
  assert.strictEqual(monitor.snapshot().attempts,0,'New track starts with fresh retry budget');

  paused=true;
  await tick(125000);
  await tick(160000);
  assert.strictEqual(attempts,2,'Intentional pause cannot cause retries');
  assert.strictEqual(skips,1);
  paused=false;
  await tick(160500);
  assert.strictEqual(attempts,2,'Resume does not immediately retry after a pause');
  await tick(178500);
  assert.strictEqual(attempts,3,'Stall after resuming can be retried');

  allowed=false;
  await tick(180000);
  assert.strictEqual(monitor.snapshot().uri,'','Disabling downloads suspends and clears monitor');
  await tick(240000);
  assert.strictEqual(skips,1);

  allowed=true;
  track='spotify:track:C';
  await tick(250000);
  track='spotify:track:D';
  await tick(251000);
  assert.strictEqual(attempts,3,'Normal track changes never trigger recovery');

  // A failed requeue must not retry endlessly and should move forward once.
  const failures=sgf.createPlaybackRecoveryMonitor({
    now:()=>time,getState:state,allowed:()=>true,speed:()=>50,
    reset:async()=>false,skip:async()=>{skips++},report:()=>{}
  });
  await failures.tick();
  time+=18000;
  await failures.tick();
  assert.strictEqual(skips,2,'Failed requeue advances instead of hanging');
  time+=18000;
  await failures.tick();
  assert.strictEqual(skips,2,'Failed requeue does not repeatedly skip');

  sgf.installPlaybackStallRecovery();
  assert(intervals.some(i=>i.ms===1500),'Watchdog installed only after player initialization');
  console.log('PASS: accelerated stalled-track retries, bounded skip, pause safety and normal transitions');
})().catch(e=>{console.error(e);process.exitCode=1;});
