'use strict';
const assert=require('node:assert/strict');
const fs=require('node:fs');
const vm=require('node:vm');

const output=[];
const player={getEvents(){return {addListener(){}};}};
const platform={getPlayerAPI(){return player;},getSettingsAPI(){return null;}};
let root=null;
const main={};
const window={Spicetify:{}};
let timerCount=0;
const document={
  getElementById(){return true;}, // style already present
  querySelector(selector){return selector==='#main'?main:null;}
};
const context={window,document,URLSearchParams,console,
  setTimeout(fn){timerCount++;queueMicrotask(fn);}
};
vm.runInNewContext(fs.readFileSync('native/ui/classic_core.js','utf8'),context,
  {filename:'classic_core.js'});
const sgf=window.__SoggfyClassic;
sgf.send=(key,value)=>output.push(key+'='+value);
sgf.state.debug=true;
sgf.state.skipDownloaded=true;
function setRoot(node){root=node;main['__reactContainer$test']=node;}

(async()=>{
  // Regression: RC57 child||sibling||return path bounces between the first
  // child and its parent, never visiting the next branch.
  const first={child:{child:{return:null}}};
  first.child.child.return=first.child;
  const sibling={stateNode:{props:{children:{props:{children:{props:{platform}}}}}}};
  first.child.sibling=sibling;
  setRoot(first);
  assert.equal(await sgf.getPlatform(),platform,
    'Must traverse sibling branch instead of cycling in first child/return');

  // Upstream keeps polling. RC57 gave up after 200 tries and never
  // installed player/queue listeners in a slow-loading Spotify client.
  setRoot({});
  timerCount=0;
  await new Promise(resolve=>{
    const previous=context.setTimeout;
    context.setTimeout=fn=>{
      timerCount++;
      if(timerCount===205)window.Spicetify.Platform=platform;
      queueMicrotask(fn);
    };
    sgf.getPlatform().then(value=>{
      assert.equal(value,platform);
      assert.ok(timerCount>200,'Must wait beyond RC57 200-try cutoff');
      context.setTimeout=previous;
      resolve();
    }).catch(e=>{context.setTimeout=previous;throw e;});
  });

  // The normal init path installs queue_update listeners only after
  // getPlayerAPI succeeds, and must report actual configured skip state.
  let listenerCalls=0;
  sgf.installPlayerListeners=()=>{listenerCalls++};
  await sgf.initPlayer();
  assert.equal(sgf.player,player);
  assert.equal(listenerCalls,1);
  assert.ok(output.some(x=>x.includes('queue player ready source=platform skipDownloaded=1')),
    'Log PlayerAPI readiness and configured skip toggle');
  assert.ok(output.some(x=>x.includes('queue platform pending')),
    'Log PlayerAPI search without silently returning null');
  console.log('PASS: original Soggfy PlayerAPI polling, sibling React search and queue listener initialization');
})().catch(e=>{console.error(e);process.exitCode=1;});