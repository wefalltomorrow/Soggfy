(() => {
'use strict';
const sgf=window.__SoggfyClassic;
if(!sgf||sgf.__contextLoaded)return;
sgf.__contextLoaded=true;

let contextInfo=null;
document.addEventListener('contextmenu',event=>{
  const row=event.target?.closest?.('div[data-testid="tracklist-row"],.main-trackList-trackListRow,div[role="row"]');
  contextInfo=row?sgf.trackInfoFromRows?.([row])?.[0]:null;
},true);

function addItem(menu,label,icon,handler,reference){
  const li=(reference?.closest?.('li')||reference)?.cloneNode?.(true);
  if(!li)return null;
  const span=li.querySelector('span');if(span)span.textContent=label;
  const button=li.querySelector('button')||li;
  button.onclick=e=>{e.preventDefault();e.stopPropagation();handler();menu.closest('[role="menu"],#context-menu')?.remove?.();};
  const svg=li.querySelector('svg');
  if(svg&&icon){
    const repl=sgf.parseHtml(icon);svg.innerHTML=repl.innerHTML;svg.setAttribute('viewBox',repl.getAttribute('viewBox')||'0 0 24 24');
  }
  reference.closest?.('li')?.insertAdjacentElement('afterend',li);
  return li;
}
function handleMenu(menu){
  if(!menu||menu.__sgfHandled)return;
  const ref=menu.querySelector('button,[role="menuitem"]');if(!ref)return;
  menu.__sgfHandled=true;

  if(sgf.state.liftQueue){
    const queue=[...menu.querySelectorAll('li,[role="menuitem"]')].find(x=>/add to queue/i.test(x.textContent||''));
    if(queue&&queue.parentElement===menu)menu.insertBefore(queue,menu.firstChild);
  }

  if(contextInfo?.uri){
    const ignored=sgf.isIgnored?.(contextInfo.uri);
    addItem(menu,(ignored?'Unignore ':'Ignore ')+(contextInfo.uri.includes(':episode:')?'episode':'track'),
      sgf.Icons.Block,()=>{
        sgf.setIgnored?.(contextInfo.uri,!ignored);
        sgf.send('ignore_current',(!ignored&&sgf.currentState()?.item?.uri===contextInfo.uri)?'1':'0');
        sgf.notify((ignored?'Unignored ':'Ignored ')+contextInfo.title);
      },ref);
  }
}
const obs=new MutationObserver(()=>{
  const menus=document.querySelectorAll('#context-menu ul,[role="menu"]');
  for(const menu of menus)handleMenu(menu);
});
const start=()=>{if(!document.body){setTimeout(start,100);return;}obs.observe(document.body,{childList:true,subtree:true});};
start();
})();