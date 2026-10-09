'use strict';

// Regression test for original Rafiuth/Soggfy status-indicator behaviour.
// A status badge belongs in a genuine Spotify track-list row's trailing
// duration section. Nothing is drawn for an absent download: red X = ERROR.

const fs=require('fs');
const vm=require('vm');
const assert=require('assert');

class ElementMock {
  constructor(tag='div'){
    this.tag=tag;this.nodeType=1;this.isConnected=true;
    this.children=[];this.style={};this.attributes={};
    this.lastElementChild=null;this.textContent='';this.href='';
    this.kind='other';
  }
  matches(selector){
    return this.kind==='track' &&
      selector.includes('[data-testid="tracklist-row"]');
  }
  querySelector(selector){return null;}
  querySelectorAll(selector){return [];}
  contains(node){return this===node;}
  closest(){return null;}
  getAttribute(k){return k==='href'?this.href:this.attributes[k]||null;}
  append(...nodes){this.children.push(...nodes);}
  appendChild(node){this.children.push(node);return node;}
  prepend(...nodes){this.children.unshift(...nodes);}
  remove(){this.isConnected=false;}
  setAttribute(key,value){this.attributes[key]=value;}
}
const trackId='A'.repeat(22);
const realUri='spotify:track:'+trackId;
const trackLink=new ElementMock('a');trackLink.href='/track/'+trackId;trackLink.textContent='Wonderwall';
const artistLink=new ElementMock('a');artistLink.href='/artist/'+'B'.repeat(22);artistLink.textContent='Oasis';
const albumLink=new ElementMock('a');albumLink.href='/album/'+'C'.repeat(22);albumLink.textContent="(What's the Story) Morning Glory?";
const durationCell=new ElementMock();
const trackRow=new ElementMock();
trackRow.kind='track';trackRow.lastElementChild=durationCell;
trackRow.querySelector=selector=>{
  if(selector.includes('a[href*="/track/"]'))return trackLink;
  if(selector.includes('a[href*="/album/"]'))return albumLink;
  if(selector.includes('.main-trackList-rowSectionEnd'))return durationCell;
  return null;
};
trackRow.querySelectorAll=selector=>
  selector.includes('a[href*="/artist/"]')?[artistLink]:[];

// These two used to receive spurious red Xs in RC48/49.
const homeCard=new ElementMock();homeCard.textContent='Daily Mix';
const genericRoleRow=new ElementMock();genericRoleRow.textContent='Playlist heading';
const sidebarPlaylist=new ElementMock();sidebarPlaylist.textContent='Tunes';
const noUriRow=new ElementMock();noUriRow.kind='track';
noUriRow.querySelector=selector=>{
  if(selector.includes('a[href*="/track/"]'))return null;
  if(selector.includes('.main-trackList-rowTitle'))return {textContent:'Badly indexed song'};
  return null;
};

const main=new ElementMock('main');
main.querySelectorAll=()=>[trackRow,noUriRow];
main.querySelector=selector=>selector.includes('tracklist-row')?trackRow:null;
main.contains=row=>[trackRow,noUriRow,genericRoleRow,homeCard].includes(row);
const intervals=[];
let refreshes=0;
const sgf={
  Icons:Object.fromEntries(['Error','InProgress','Processing','Warning','Done','SyncDisabled','Folder'].map(n=>[n,n])),
  FS:String.fromCharCode(31),RS:String.fromCharCode(30),
  statusMap:new Map(),
  state:{skipDownloaded:false,skipIgnored:false},
  send(){},parseHtml(svg){return {html:svg};}
};
const context={
  window:{__SoggfyClassic:sgf},
  document:{
    body:new ElementMock('body'),visibilityState:'visible',
    createElement(tag){return new ElementMock(tag);},
    querySelector(selector){
      return selector==='.main-view-container__scroll-node-child'?main:null;
    },
    querySelectorAll(){throw new Error('Global DOM scan is forbidden');}
  },
  Element:ElementMock,
  localStorage:{getItem:()=>'[]'},
  MutationObserver:class{observe(){}},
  setTimeout(){return ++refreshes;},clearTimeout(){},
  setInterval(callback,ms){intervals.push({callback,ms});},
  console,
  location:{pathname:'/playlist/'+'D'.repeat(22)}
};

vm.runInNewContext(fs.readFileSync('native/ui/classic_status.js','utf8'),context,{filename:'classic_status.js'});

// Actual track URI is used; cards/generic ARIA rows and URI-less tracks ignored.
const found=sgf.trackInfoFromRows([homeCard,genericRoleRow,sidebarPlaylist,noUriRow,trackRow]);
assert.strictEqual(found.length,1,'Only real track-list rows with a real URI qualify');
assert.strictEqual(found[0].uri,realUri,'Use the true Spotify track identity');

// The original app does not mark every file absent from disk as an ERROR.
sgf.renderVisibleStatuses();
assert.strictEqual(trackRow.__sgf_status_ind,undefined,
  'Missing status means no icon, not a giant red cross');
assert.strictEqual(homeCard.__sgf_status_ind,undefined);
assert.strictEqual(genericRoleRow.__sgf_status_ind,undefined);
assert.strictEqual(sidebarPlaylist.__sgf_status_ind,undefined);

// A completed download shows a green check inside its real trailing cell.
sgf.statusMap.set(realUri,{status:'DONE',path:'C:\\Music\\Oasis - Wonderwall.mp3',message:'Saved'});
sgf.renderVisibleStatuses();
let node=trackRow.__sgf_status_ind;
assert(node,'Completed track should show badge');
assert.strictEqual(node.__sgfStatus,'DONE');
assert.strictEqual(node.children[1].html,'Done','Green check icon');
assert.strictEqual(durationCell.children[0],node,'Indicator placed only in duration cell');
assert.strictEqual(trackRow.children.length,0,'Never prepend directly to the whole track row');
sgf.renderVisibleStatuses();
assert.strictEqual(trackRow.__sgf_status_ind,node,'No needless redraws');

// A genuine failed capture is the only reason to display a red cross.
sgf.statusMap.set(realUri,{status:'ERROR',message:'Invalid Vorbis tags',path:''});
sgf.renderVisibleStatuses();
let error=trackRow.__sgf_status_ind;
assert.strictEqual(error.__sgfStatus,'ERROR');
assert.strictEqual(error.children[1].html,'Error','Red cross reserved for actual errors');
assert.strictEqual(error.title,'Invalid Vorbis tags');
assert.strictEqual(node.isConnected,false,'Replaced badge removed');

// A new failure reason should update the tooltip.
sgf.statusMap.set(realUri,{status:'ERROR',message:'Retry failed',path:''});
sgf.renderVisibleStatuses();
assert.notStrictEqual(trackRow.__sgf_status_ind,error);
assert.strictEqual(trackRow.__sgf_status_ind.title,'Retry failed');

// When live status is cleared, icon must disappear instead of inventing MISSING.
sgf.statusMap.delete(realUri);
sgf.renderVisibleStatuses();
assert.strictEqual(trackRow.__sgf_status_ind,undefined);
assert.strictEqual(homeCard.__sgf_status_ind,undefined);

// Refresh remains available when asynchronous conversion finishes.
const tick=intervals.find(i=>i.ms===3000);
assert(tick,'Periodic reconciliation should remain installed');
const before=refreshes;
tick.callback();
assert(refreshes>before,'Status reconciliation runs on visible tracks');
console.log('PASS: original Soggfy track-only indicators, no missing crosses, correct duration placement');
