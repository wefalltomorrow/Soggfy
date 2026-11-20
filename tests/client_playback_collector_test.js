const fs=require('fs'),vm=require('vm'),assert=require('assert');
const output=[],state={playbackId:'1234567890ABCDEF1234567890ABCDEF',isPaused:false,isBuffering:false,isActiveDevice:true,
 item:{uri:'spotify:track:0000000000000000000001',metadata:{title:'Song',artist_name:'Artist',album_title:'Album',duration:'120000'}}};
let throwing=false,elapsed=0;
const wallTime=100000;
const player={getState(){if(throwing)throw Error('unavailable');return state;},getEvents(){return {};}};
const context={window:{__soggfyMetadataEnabled:false,Spicetify:{Player:player}},document:{querySelector:()=>null},
 Date:{now:()=>wallTime},console:{info:x=>output.push(x)},performance:{now:()=>elapsed},setInterval(){},encodeURIComponent};
vm.runInNewContext(fs.readFileSync('native/metadata_collector.js','utf8'),context);
const tick=()=>{elapsed+=1000;context.window.__floggfyPoll();};
const last=()=>output.filter(x=>x.startsWith('FLOGGFY_PLAYBACK_V1:')).at(-1);
assert(last().includes('instance=1234567890abcdef1234567890abcdef'));
assert(last().includes('paused=0&buffering=0&local=1'));
assert(!output.some(x=>x.startsWith('FLOGGFY_METADATA_V1:')));
state.isPaused=true;tick();assert(last().includes('paused=1'));
state.isPaused=false;state.isBuffering=true;tick();assert(last().includes('buffering=1'));
state.isActiveDevice=false;tick();assert(last().includes('local=0'));
const track=state.item;state.item={uri:'spotify:ad:interstitial'};tick();assert.equal(last(),'FLOGGFY_PLAYBACK_V1:v=1');
state.item=track;state.playbackId='abcdef1234567890abcdef1234567890';tick();assert(last().includes('instance='+state.playbackId));
for(const bad of ['0'.repeat(32),'x'.repeat(32),'abc',42]){state.playbackId=bad;tick();assert(!last().includes('instance='));}
state.item.metadata.title='x'.repeat(5000);tick();assert.equal(last(),'FLOGGFY_PLAYBACK_V1:v=1');
state.item.metadata.title='Song';throwing=true;tick();assert.equal(last(),'FLOGGFY_PLAYBACK_V1:v=1');
throwing=false;tick();assert(last().includes('title=Song'));
// Positions are milliseconds in PlayerAPI, seconds in the native protocol.
state.item.metadata.duration='120000';state.isActiveDevice=true;
state.playbackId='1234567890abcdef1234567890abcdef';
const position=()=>new URLSearchParams(last().split(':').slice(1).join(':')).get('position');
for(const speed of [1,2,10,30]){
 state.speed=speed;state.timestamp=wallTime-1000;state.positionAsOfTimestamp=15000;
 state.isPaused=false;state.isBuffering=false;tick();assert.equal(Number(position()),15+speed);
 state.isPaused=true;tick();assert.equal(Number(position()),15);
 state.isPaused=false;state.isBuffering=true;tick();assert.equal(Number(position()),15);
 state.isBuffering=false;state.positionAsOfTimestamp=119000;tick();assert.equal(Number(position()),120);
 state.positionAsOfTimestamp=0;state.timestamp=wallTime;tick();assert.equal(Number(position()),0);
}
for(const invalid of [NaN,Infinity,-1,undefined]){state.positionAsOfTimestamp=invalid;tick();assert.equal(position(),null);}
state.positionAsOfTimestamp=15000;
for(const stamp of [wallTime+1,wallTime-86400001,NaN,undefined]){state.timestamp=stamp;tick();assert.equal(position(),null);}
state.timestamp=wallTime;
for(const speed of [0,31,Infinity,NaN]){state.speed=speed;tick();assert.equal(position(),null);}
console.log('PASS: position units, speeds 1/2/10/30, paused/buffered snapshots, duration clamp and invalid clocks');
console.log('PASS: identity heartbeat with metadata off, pause/buffering/remote, ads, malformed IDs and unavailable snapshots');
