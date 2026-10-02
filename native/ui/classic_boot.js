(() => {
'use strict';
const sgf=window.__SoggfyClassic;
if(!sgf||sgf.__booted)return;
sgf.__booted=true;

let topbar=null;
const remount=()=>{
  if(topbar?.isConnected)return;
  topbar=sgf.mountTopbar?.(topbar)||sgf.mountTopbar?.();
};
const obs=new MutationObserver(remount);
const start=async()=>{
  if(!document.body){setTimeout(start,100);return;}
  obs.observe(document.body,{childList:true,subtree:true});
  remount();
  sgf.send('sync','1');
  setTimeout(()=>sgf.send('sync','1'),1000);
  await sgf.initPlayer?.();
  const uri=sgf.currentState()?.item?.uri;
  if(uri)sgf.send('ignore_current',sgf.isTrackIgnored?.(sgf.currentState()?.item)?'1':'0');
};
start();
})();