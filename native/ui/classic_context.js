(() => {
'use strict';
const sgf=window.__SoggfyClassic;
if(!sgf||sgf.__contextLoaded)return;
sgf.__contextLoaded=true;

let contextInfo=null,contextUris=[];
function collectContextUris(target){
  const row=target?.closest?.('div[data-testid="tracklist-row"],.main-trackList-trackListRow,div[role="row"]');
  contextInfo=row?sgf.trackInfoFromRows?.([row])?.[0]:null;
  const uris=new Set(contextInfo?.ignoreUris||[]);
  let node=target;
  for(let depth=0;node&&depth<8;depth++,node=node.parentElement){
    for(const key of Object.keys(node)){
      if(!key.startsWith('__reactProps$')&&!key.startsWith('__reactFiber$'))continue;
      const root=node[key],stack=[root],seen=new Set();let budget=350;
      while(stack.length&&budget-->0){
        const v=stack.pop();
        if(!v||typeof v!=='object'||seen.has(v))continue;
        seen.add(v);
        for(const [k,x] of Object.entries(v)){
          if(typeof x==='string'&&/^spotify:(track|episode|album|playlist|artist):/.test(x)&&/uri|context/i.test(k)){
            uris.add(x);
          }else if(Array.isArray(x)&&/uris/i.test(k)){
            for(const u of x)if(typeof u==='string'&&u.startsWith('spotify:'))uris.add(u);
          }else if(x&&typeof x==='object'&&!seen.has(x)){
            stack.push(x);
          }
        }
      }
    }
  }
  if(!uris.size){
    const m=location.pathname.match(/\/(playlist|album|artist)\/([A-Za-z0-9]+)/);
    if(m)uris.add('spotify:'+m[1]+':'+m[2]);
  }
  contextUris=[...uris];
}
document.addEventListener('contextmenu',event=>collectContextUris(event.target),true);

function addItem(menu,label,icon,handler,reference){
  const template=reference?.closest?.('li')||reference;
  const li=template?.cloneNode?.(true);
  if(!li)return null;

  const span=li.querySelector('span');
  if(span)span.textContent=label;

  const button=li.querySelector('button')||li;
  button.classList?.remove?.('QgtQw2NJz7giDZxap2BB');
  button.onclick=event=>{
    event.preventDefault();
    event.stopPropagation();
    handler();
    menu.closest('[role="menu"],#context-menu')?.remove?.();
  };

  const svg=li.querySelector('svg');
  if(svg&&icon){
    const replacement=sgf.parseHtml(icon);
    svg.innerHTML=replacement.innerHTML;
    svg.setAttribute('viewBox',replacement.getAttribute('viewBox')||'0 0 24 24');
  }

  template.insertAdjacentElement('afterend',li);
  return li;
}

function handleMenu(menu){
  if(!menu||menu.__sgfHandled)return;
  const ref=menu.querySelector('button,[role="menuitem"]');
  if(!ref)return;
  menu.__sgfHandled=true;

  if(sgf.state.liftQueue){
    const queue=[...menu.querySelectorAll('li,[role="menuitem"]')]
      .find(item=>/add to queue/i.test(item.textContent||''));
    if(queue&&queue.parentElement===menu)menu.insertBefore(queue,menu.firstChild);
  }

  if(sgf.hasM3UContext?.()){
    addItem(menu,'Generate M3U',sgf.Icons.SaveAs,()=>sgf.generateM3U?.(),ref);
  }

  if(contextUris.length){
    const ignored=contextUris.some(uri=>sgf.isIgnored?.(uri));
    const resource=contextUris[0].split(':')[1]||'item';
    const label=resource+(contextUris.length>1?'s':'');
    addItem(
      menu,
      (ignored?'Unignore ':'Ignore ')+label,
      sgf.Icons.Block,
      ()=>{
        sgf.setIgnoredMany?.(contextUris,!ignored);
        const current=sgf.currentState?.()?.item;
        sgf.send('ignore_current',sgf.isTrackIgnored?.(current)?'1':'0');
        sgf.notify((ignored?'Unignored ':'Ignored ')+(contextInfo?.title||label));
      },
      ref
    );
  }
}

const observer=new MutationObserver(()=>{
  const menus=document.querySelectorAll('#context-menu ul,[role="menu"]');
  for(const menu of menus)handleMenu(menu);
});

const start=()=>{
  if(!document.body){setTimeout(start,100);return;}
  observer.observe(document.body,{childList:true,subtree:true});
};
start();
})();