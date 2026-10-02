(() => {
'use strict';
const sgf=window.__SoggfyClassic;
if(!sgf||sgf.__contextLoaded)return;
sgf.__contextLoaded=true;

let contextInfo=null,contextUris=[];

const isSpotifyUri=value =>
  typeof value==='string' &&
  /^spotify:(track|episode|album|playlist|artist):[A-Za-z0-9]+$/.test(value);

function selectionFromObject(root){
  const stack=[root],seen=new Set();let budget=500;
  while(stack.length&&budget-->0){
    const value=stack.pop();
    if(!value||typeof value!=='object'||seen.has(value))continue;
    seen.add(value);

    for(const props of [value,value.props,value.memoizedProps,value.pendingProps]){
      if(!props||typeof props!=='object')continue;

      const contextUri=[
        props.contextUri,props.reference?.uri,props.context?.uri
      ].find(isSpotifyUri)||'';
      const directUri=[props.uri,props.item?.uri].find(isSpotifyUri)||'';
      const uris=Array.isArray(props.uris)?props.uris.filter(isSpotifyUri):[];

      // Old Sprinkles semantics:
      // a context URI plus a direct URI means one track/episode inside a
      // collection, therefore Ignore applies only to the direct item.
      if(contextUri&&directUri)return {contextUri,trackUris:[directUri]};
      if(uris.length)return {contextUri,trackUris:[...new Set(uris)]};
      if(props.item?.uri&&isSpotifyUri(props.item.uri))
        return {contextUri,trackUris:[props.item.uri]};
      if(contextUri)return {contextUri,trackUris:[]};
      if(directUri)return {contextUri:directUri,trackUris:[]};
    }

    for(const child of Object.values(value))
      if(child&&typeof child==='object'&&!seen.has(child))stack.push(child);
  }
  return null;
}

function collectContextUris(target){
  const row=target?.closest?.(
    'div[data-testid="tracklist-row"],.main-trackList-trackListRow,div[role="row"]'
  );
  contextInfo=row?sgf.trackInfoFromRows?.([row])?.[0]:null;

  // A row menu always targets that row, matching legacy Sprinkles exactly.
  if(contextInfo?.uri){
    contextUris=[contextInfo.uri];
    return;
  }

  let selection=null;
  for(let node=target,depth=0;node&&depth<8&&!selection;node=node.parentElement,depth++){
    for(const key of Object.keys(node)){
      if(!key.startsWith('__reactProps$')&&!key.startsWith('__reactFiber$'))continue;
      selection=selectionFromObject(node[key]);
      if(selection)break;
    }
  }

  if(selection){
    contextUris=selection.trackUris?.length
      ? [...new Set(selection.trackUris)]
      : (selection.contextUri?[selection.contextUri]:[]);
    return;
  }

  const match=location.pathname.match(/\/(playlist|album|artist)\/([A-Za-z0-9]+)/);
  contextUris=match?['spotify:'+match[1]+':'+match[2]]:[];
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

  if(sgf.hasM3UContext?.())
    addItem(menu,'Generate M3U',sgf.Icons.SaveAs,()=>sgf.generateM3U?.(),ref);

  if(contextUris.length){
    const ignored=contextUris.some(uri=>sgf.isIgnored?.(uri));
    const resource=contextUris[0].split(':')[1]||'item';
    const label=resource+(contextUris.length>1?'s':'');
    addItem(menu,(ignored?'Unignore ':'Ignore ')+label,sgf.Icons.Block,()=>{
      sgf.setIgnoredMany?.(contextUris,!ignored);
      const current=sgf.currentState?.()?.item;
      sgf.send('ignore_current',sgf.isTrackIgnored?.(current)?'1':'0');
      sgf.notify((ignored?'Unignored ':'Ignored ')+(contextInfo?.title||label));
    },ref);
  }
}

const observer=new MutationObserver(()=>{
  for(const menu of document.querySelectorAll('#context-menu ul,[role="menu"]'))
    handleMenu(menu);
});

const start=()=>{
  if(!document.body){setTimeout(start,100);return;}
  observer.observe(document.body,{childList:true,subtree:true});
};
start();
})();