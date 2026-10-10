'use strict';
const assert=require('node:assert/strict');
const fs=require('node:fs');
const vm=require('node:vm');
const messages=[];
const window={Spicetify:{}};
const document={getElementById(){return true;},querySelector(){return null;}};
const context={window,document,URLSearchParams,encodeURIComponent,
  console:{info(msg){messages.push(String(msg));},warn(){},error(){}},
  setTimeout(){return 0;}};
vm.runInNewContext(fs.readFileSync('native/ui/classic_core.js','utf8'),context,
  {filename:'classic_core.js'});
const sgf=window.__SoggfyClassic;
assert.equal(sgf.MAX_PLAYBACK_SPEED,30,'Classic UI shares validated 30x ceiling');
const settingsSource=fs.readFileSync('native/ui/classic_settings.js','utf8');
assert.match(settingsSource,/max=sgf\.MAX_PLAYBACK_SPEED/,
  'Classic slider must use the capped maximum');
assert.match(settingsSource,/row\('Playback speed'.*slider\('playbackSpeed'\)/,
  'Playback control must use the bounded slider');
sgf.state.speedSupported=true;
sgf.state.speedImmediate=true;
sgf.applyConfig('playbackSpeed=50');
assert.equal(sgf.state.playbackSpeed,30,'Previously saved 50x clamps on config sync');
sgf.applyConfig('playbackSpeed=31');
assert.equal(sgf.state.playbackSpeed,30,'Previously saved 31x clamps on config sync');
sgf.applyConfig('playbackSpeed=10');
assert.equal(sgf.state.playbackSpeed,10,'Valid playback speed remains intact');
(async()=>{
  await sgf.setPlaybackSpeed(50);
  assert.equal(sgf.state.playbackSpeed,30,'Typed 50x cannot exceed ceiling');
  assert.ok(messages.some(v=>v.includes('playbackSpeed=30')),
    'Native message must carry the capped value');
  await sgf.setPlaybackSpeed(30);
  assert.equal(sgf.state.playbackSpeed,30);
  await sgf.setPlaybackSpeed(1);
  assert.equal(sgf.state.playbackSpeed,1);
  console.log('PASS: Classic UI 1-30x slider/config/native-message ceiling');
})().catch(e=>{process.stderr.write(String(e.stack||e)+'\n');process.exitCode=1;});
