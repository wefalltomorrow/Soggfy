'use strict';
const assert=require('assert'),fs=require('fs'),vm=require('vm');

function harness({route='/playlist/routerId',documentPath='/index.html',historyAvailable=true,initiallyIgnored=false,menuPresent=true}={}){
 let observer,contextMenuListener,generated=0,closed=false,ignored=initiallyIgnored;
 const ignoredCalls=[],messages=[],sends=[];
 const inserted=[];
 function makeItem(){
  const span={textContent:''};
  const button={classList:{remove(){}},onclick:null};
  const item={textContent:'Existing action',span,button,
   closest(selector){return selector==='li'?this:null;},
   cloneNode(){return makeItem();},
   querySelector(selector){
    if(selector==='span')return span;
    if(selector==='button')return button;
    return null;
   },
   insertAdjacentElement(position,node){assert.strictEqual(position,'afterend');inserted.push(node);}
  };
  return item;
 }
 const reference=makeItem();
 const menu={__sgfHandled:false,
  querySelector(selector){return selector==='button,[role="menuitem"]'?reference:null;},
  querySelectorAll(){return [];},
  closest(){return {remove(){closed=true;}};}
 };
 const document={body:{},
  addEventListener(name,listener,capture){if(name==='contextmenu'&&capture)contextMenuListener=listener;},
  querySelectorAll(){return menuPresent?[menu]:[];}
 };
 const window={};
 if(historyAvailable)window.Spicetify={Platform:{History:{location:{pathname:route}}}};
 const currentRoute=()=>window.Spicetify?.Platform?.History?.location?.pathname||documentPath;
 const sgf={state:{liftQueue:false},Icons:{},
  hasM3UContext:()=>['/playlist/','/album/'].some(prefix=>currentRoute().includes(prefix)),
  generateM3U:()=>{generated++;},
  trackInfoFromRows:()=>[],isIgnored:()=>ignored,
  setIgnoredMany:(uris,value)=>{ignoredCalls.push({uris,value});ignored=value;},
  currentState:()=>null,send:(...args)=>sends.push(args),notify:message=>messages.push(message),
  parseHtml(){return {innerHTML:'',getAttribute:()=>null};}
 };
 window.__SoggfyClassic=sgf;
 class MutationObserver{constructor(callback){observer=callback;}observe(){}}
 vm.runInNewContext(fs.readFileSync('native/ui/classic_context.js','utf8'),{
  window,document,location:{pathname:documentPath},MutationObserver,setTimeout(fn){fn();}
 });
 return {menu,inserted,runObserver:()=>observer(),contextMenuListener,
  generated:()=>generated,ignoredCalls,messages,sends,closed:()=>closed};
}

const playlist=harness();
playlist.contextMenuListener({target:{closest:()=>null,parentElement:null}});
playlist.runObserver();
assert.deepStrictEqual(playlist.inserted.map(item=>item.span.textContent),
 ['Generate M3U','Ignore playlist'],
 'router-backed playlist context menu offers M3U and playlist ignore actions');
playlist.inserted[0].button.onclick({preventDefault(){},stopPropagation(){}});
assert.strictEqual(playlist.generated(),1,'Generate M3U invokes the export action');
assert.strictEqual(playlist.closed(),true,'the selected context menu closes');
playlist.inserted[1].button.onclick({preventDefault(){},stopPropagation(){}});
assert.deepStrictEqual(JSON.parse(JSON.stringify(playlist.ignoredCalls)),[{
 uris:['spotify:playlist:routerId'],value:true
}],'router-backed collection ignore action targets the playlist URI');
assert.deepStrictEqual(JSON.parse(JSON.stringify(playlist.sends)),[['ignore_current','0']],
 'ignore action refreshes the current-track state after changing the collection');
assert.deepStrictEqual(playlist.messages,['Ignored playlist']);

const alreadyIgnored=harness({initiallyIgnored:true});
alreadyIgnored.contextMenuListener({target:{closest:()=>null,parentElement:null}});
alreadyIgnored.runObserver();
assert(alreadyIgnored.inserted.some(item=>item.span.textContent==='Unignore playlist'),
 'an ignored router-backed collection offers Unignore');
alreadyIgnored.inserted.find(item=>item.span.textContent==='Unignore playlist')
 .button.onclick({preventDefault(){},stopPropagation(){}});
assert.deepStrictEqual(JSON.parse(JSON.stringify(alreadyIgnored.ignoredCalls)),[{
 uris:['spotify:playlist:routerId'],value:false
}],'unignore action clears the playlist ignore state');
assert.deepStrictEqual(alreadyIgnored.messages,['Unignored playlist']);

const fallback=harness({route:'',historyAvailable:false,documentPath:'/album/fallbackId'});
fallback.contextMenuListener({target:{closest:()=>null,parentElement:null}});
fallback.runObserver();
assert.deepStrictEqual(fallback.inserted.map(item=>item.span.textContent),
 ['Generate M3U','Ignore album'],
 'document pathname remains a fallback when Spicetify history is unavailable');
fallback.inserted[1].button.onclick({preventDefault(){},stopPropagation(){}});
assert.deepStrictEqual(JSON.parse(JSON.stringify(fallback.ignoredCalls)),[{
 uris:['spotify:album:fallbackId'],value:true
}],'document-path fallback supplies the correct album URI');

const unrelated=harness({route:'',documentPath:'/index.html'});
unrelated.contextMenuListener({target:{closest:()=>null,parentElement:null}});
unrelated.runObserver();
assert.strictEqual(unrelated.inserted.length,0,'an unrelated embedded index menu gains no collection actions');

console.log('PASS: router-backed Classic context actions, ignore/unignore and pathname fallback');
