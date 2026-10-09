'use strict';
const assert=require('assert');
const fs=require('fs');
const vm=require('vm');

class ElementMock {
  constructor(name='div') {
    this.name=name; this.nodeType=1; this.children=[]; this.isConnected=true;
    this.style={}; this.attributes={}; this.textContent='';
    this.parentElement=null; this.previousElementSibling=null;
    this.lastElementChild=null;
  }
  matches(q){return this.kind==='track'&&q.includes('tracklist-row');}
  contains(node){return node===this||this.childNodes?.includes(node)||false;}
  querySelector(){return null;}
  querySelectorAll(){return [];}
  getAttribute(name){return this.attributes[name]||null;}
  setAttribute(name,value){this.attributes[name]=value;}
  prepend(node){this.children.unshift(node);}
  append(...nodes){this.children.push(...nodes);}
  appendChild(node){this.children.push(node);return node;}
  remove(){this.isConnected=false;}
  closest(){return null;}
}
const A='A'.repeat(22),B='B'.repeat(22),C='C'.repeat(22);
const make=(title,artist,uri,album='Codex VI')=>{
  const row=new ElementMock();
  row.kind='track';
  const link=new ElementMock('a');
  link.textContent=title;
  if(uri)link.attributes.href='/track/'+uri;
  const titleCell=new ElementMock('span');titleCell.textContent=title;
  const artistCell=new ElementMock('span');artistCell.textContent=artist;
  const albumCell=new ElementMock('span');albumCell.textContent=album;
  const end=new ElementMock('div'); row.lastElementChild=end;
  const menu=new ElementMock('button');
  menu.parentElement=row;
  row.childNodes=[menu];
  row.querySelector=q=>{
    if(q.includes('a[href*="/track/"]'))return uri?link:null;
    if(q.includes('.main-trackList-rowTitle'))return titleCell;
    if(q.includes('.main-trackList-rowSubTitle'))return artistCell;
    if(q.includes('.main-trackList-rowSectionVariable'))return albumCell;
    if(q.includes('.main-trackList-rowSectionEnd'))return end;
    if(q.includes('[data-testid="more-button"]'))return menu;
    return null;
  };
  row.querySelectorAll=()=>[];
  return {row,menu,end};
};
const a=make('The Magumba State','Shpongle',A);
const b=make('Empty Branes','Shpongle',B);
const c=make('Celestial Intoxication','Shpongle',null);
const d=make('Remember the Future','Shpongle',null);
const duplicate=make('Empty Branes','Shpongle',B);
const home=new ElementMock();home.textContent='Daily Mix';
const main=new ElementMock();
const rows=[a.row,b.row,c.row,d.row,duplicate.row];
main.contains=el=>rows.includes(el);
main.querySelectorAll=()=>rows;
main.querySelector=()=>a.row;

// The former arbitrary Fiber traversal would discover A for unrelated rows.
for(const t of [b,c,d]){
  t.row.__reactFiber$test={return:{memoizedProps:{item:{uri:'spotify:track:'+A}}}};
}
// The upstream-style row-scoped menu prop is honored, without touching Fiber.
// Actual Spotify can expose this when the row lacks a clickable track href.
d.row.__reactProps$test={menu:{props:{uri:'spotify:track:'+C}}};

const sgf={
  Icons:Object.fromEntries(['Error','InProgress','Processing','Warning','Done','SyncDisabled','Folder'].map(x=>[x,x])),
  statusMap:new Map(),FS:'\x1f',RS:'\x1e',
  state:{skipDownloaded:false,skipIgnored:false},send(){},
  parseHtml(html){return {html}}
};
const timers=[];
const context={
 window:{__SoggfyClassic:sgf},
 document:{
   body:main,visibilityState:'visible',
   createElement(tag){return new ElementMock(tag)},
   querySelector(q){return q==='.main-view-container__scroll-node-child'?main:null},
   querySelectorAll(){throw Error('Unscoped selector')}
 },
 Element:ElementMock,
 localStorage:{getItem:()=>'[]'},
 MutationObserver:class {observe(){}},
 setTimeout(fn){timers.push(fn);return timers.length},
 clearTimeout(){},
 setInterval(){},
 console,
 location:{pathname:'/playlist/'+'D'.repeat(22)}
};
vm.runInNewContext(fs.readFileSync('native/ui/classic_status.js','utf8'),context,{filename:'classic_status.js'});
const infos=sgf.trackInfoFromRows();
assert.strictEqual(infos.length,5,'Every real row, including repeated songs, must be returned');
const [qa,qb,qc,qd,qe]=infos;
assert.strictEqual(qa.uri,'spotify:track:'+A);
assert.strictEqual(qb.uri,'spotify:track:'+B,'Row-specific link over a parent Fiber');
assert(qc.uri.startsWith('spotify:track:sgf'),'Missing native URI gets local scoped status identity');
assert.notStrictEqual(qc.uri,qd.uri,'Different tracks must never share synthetic URI');
assert.strictEqual(qd.uri,'spotify:track:'+C,'Own row-scoped React menu props');
assert.strictEqual(qe.uri,qb.uri,'Duplicate track rows share the same status identity');
assert.strictEqual(qc.album,'Codex VI','Album extracted from playlist row, not playlist title');

sgf.statusMap.set(qa.uri,{status:'IN_PROGRESS',message:'Downloading...'});
sgf.statusMap.set(qb.uri,{status:'IN_PROGRESS',message:'Downloading...'});
sgf.statusMap.set(qc.uri,{status:'DONE',path:'Shpongle - Celestial Intoxication.mp3'});
sgf.statusMap.set(qd.uri,{status:'ERROR',message:'Invalid stream'});
sgf.renderVisibleStatuses();
assert.strictEqual(a.row.__sgf_status_ind.__sgfStatus,'IN_PROGRESS');
assert.strictEqual(b.row.__sgf_status_ind.__sgfStatus,'IN_PROGRESS');
assert.strictEqual(c.row.__sgf_status_ind.__sgfStatus,'DONE');
assert.strictEqual(d.row.__sgf_status_ind.__sgfStatus,'ERROR');
assert.strictEqual(duplicate.row.__sgf_status_ind.__sgfStatus,'IN_PROGRESS');
assert(!home.__sgf_status_ind,'Never place indicators on home cards');

// Native publication should replace an existing in-progress icon without
// navigating away, and update each duplicate occurrence of that song.
sgf.statusMap.set(qb.uri,{status:'DONE',message:'Saved',path:'Shpongle - Empty Branes.mp3'});
sgf.renderVisibleStatuses();
assert.strictEqual(b.row.__sgf_status_ind.__sgfStatus,'DONE');
assert.strictEqual(duplicate.row.__sgf_status_ind.__sgfStatus,'DONE');
assert.strictEqual(b.end.children[0],b.row.__sgf_status_ind);
assert.strictEqual(duplicate.end.children[0],duplicate.row.__sgf_status_ind);
assert.notStrictEqual(a.row.__sgf_status_ind.__sgfStatus,'DONE',
  'Completing one song must not mark the first song downloaded');

console.log('PASS: five scoped Spotify rows, distinct React identities, missing URI fallback, duplicate rows and status transitions');
