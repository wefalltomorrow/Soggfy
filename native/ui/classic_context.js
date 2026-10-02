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
      if(!key.startsWith('__reactProps

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

  if(sgf.hasM3UContext?.()){
    addItem(menu,'Generate M3U',sgf.Icons.SaveAs,()=>sgf.generateM3U?.(),ref);
  }

  if(contextUris.length){
    const ignored=contextUris.some(uri=>sgf.isIgnored?.(uri));
    const primary=contextUris.find(uri=>/^spotify:(track|episode):/.test(uri))||contextUris[0];
    const type=(primary.split(':')[1]||'item')+(contextUris.length>1?'s':'');
    addItem(menu,(ignored?'Unignore ':'Ignore ')+type,
      sgf.Icons.Block,()=>{
        sgf.setIgnoredMany?.(contextUris,!ignored);
        const current=sgf.currentState?.()?.item;
        sgf.send('ignore_current',sgf.isTrackIgnored?.(current)?'1':'0');
        sgf.notify((ignored?'Unignored ':'Ignored ')+(contextInfo?.title||type));
      },ref);
  }
}
const obs=new MutationObserver(()=>{
  const menus=document.querySelectorAll('#context-menu ul,[role="menu"]');
  for(const menu of menus)handleMenu(menu);
});
const start=()=>{if(!document.body){setTimeout(start,100);return;}obs.observe(document.body,{childList:true,subtree:true});};
start();
})();)&&!key.startsWith('__reactFiber

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

  if(sgf.hasM3UContext?.()){
    addItem(menu,'Generate M3U',sgf.Icons.SaveAs,()=>sgf.generateM3U?.(),ref);
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
})();))continue;
      const root=node[key],stack=[root],seen=new Set();let budget=350;
      while(stack.length&&budget-->0){
        const v=stack.pop();if(!v||typeof v!=='object'||seen.has(v))continue;seen.add(v);
        for(const [k,x] of Object.entries(v)){
          if(typeof x==='string'&&/^spotify:(track|episode|album|playlist|artist):/.test(x) &&
             /uri|context/i.test(k))uris.add(x);
          else if(Array.isArray(x)&&/uris/i.test(k))for(const u of x)if(typeof u==='string'&&u.startsWith('spotify:'))uris.add(u);
          else if(x&&typeof x==='object'&&!seen.has(x))stack.push(x);
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

  if(sgf.hasM3UContext?.()){
    addItem(menu,'Generate M3U',sgf.Icons.SaveAs,()=>sgf.generateM3U?.(),ref);
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