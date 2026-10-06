(() => {
'use strict';
const sgf=window.__SoggfyClassic;
if(!sgf||sgf.__statusLoaded)return;
sgf.__statusLoaded=true;

const STATUS_ICON={
  ERROR:sgf.Icons.Error,IN_PROGRESS:sgf.Icons.InProgress,CONVERTING:sgf.Icons.Processing,
  WARN:sgf.Icons.Warning,DONE:sgf.Icons.Done,IGNORED:sgf.Icons.SyncDisabled
};
const ignoreKey='soggfy.ignorelist.v1';
let ignoreSet=new Set();
try{ignoreSet=new Set(JSON.parse(localStorage.getItem(ignoreKey)||'[]'));}catch{}
sgf.isIgnored=uri=>!!uri&&ignoreSet.has(uri);
sgf.ignoreCandidates=item=>{
  if(!item)return [];
  const metadata=item.metadata||item.contextTrack?.metadata||{};
  const artists=item.artists||item.album?.artists||[];
  const values=[
    item.uri,item.contextTrack?.uri,item.album?.uri,item.albumUri,
    metadata.context_uri,item.contextUri,
    ...artists.map?.(a=>a?.uri)||[]
  ].filter(v=>typeof v==='string'&&v.startsWith('spotify:'));
  return [...new Set(values)];
};
sgf.isTrackIgnored=item=>sgf.ignoreCandidates(item).some(uri=>ignoreSet.has(uri));
sgf.setIgnored=(uri,value)=>{
  if(!uri)return;
  value?ignoreSet.add(uri):ignoreSet.delete(uri);
  try{localStorage.setItem(ignoreKey,JSON.stringify([...ignoreSet]));}catch{}
  sgf.refreshVisibleStatuses?.();
};
sgf.setIgnoredMany=(uris,value)=>{
  for(const uri of uris||[])if(uri)value?ignoreSet.add(uri):ignoreSet.delete(uri);
  try{localStorage.setItem(ignoreKey,JSON.stringify([...ignoreSet]));}catch{}
  sgf.refreshVisibleStatuses?.();
};

function uriFromHref(href){
  if(!href)return '';
  const m=href.match(/\/(track|episode|artist|album|playlist)\/([A-Za-z0-9]+)/);
  return m?'spotify:'+m[1]+':'+m[2]:'';
}
function canonicalRow(row){
  if(!(row instanceof Element))return null;
  if(row.matches('div[data-testid="tracklist-row"],.main-trackList-trackListRow'))return row;
  const inner=row.querySelector('div[data-testid="tracklist-row"],.main-trackList-trackListRow');
  if(inner)return inner;
  const first=row.firstElementChild;
  if(row.matches('div[role="row"]')&&first&&
     first.querySelector?.('a[href*="/track/"],a[href*="/episode/"],[data-testid="more-button"],.main-trackList-rowMoreButton'))
    return first;
  return row;
}
function reactRoots(row){
  const roots=[];
  const nodes=[row,...row.querySelectorAll('[data-testid="more-button"],.main-trackList-rowMoreButton,button')];
  for(const e of nodes){
    for(const key of Object.keys(e)){
      if(key.startsWith('__reactProps$')||key.startsWith('__reactFiber$'))roots.push(e[key]);
    }
  }
  return roots;
}
function artistNames(value){
  if(!Array.isArray(value))return [];
  return value.map(a=>typeof a==='string'?a:(a?.name||a?.title||''))
    .map(x=>String(x||'').trim()).filter(Boolean);
}
function reactTrackData(row,preferredUri=''){
  let best=null,bestScore=-1;
  for(const root of reactRoots(row)){
    const stack=[root],seen=new Set();let budget=1200;
    while(stack.length&&budget-->0){
      const v=stack.pop();
      if(!v||typeof v!=='object'||seen.has(v))continue;
      seen.add(v);
      const directUri=typeof v.uri==='string'?v.uri:
        (typeof v.contextTrack?.uri==='string'?v.contextTrack.uri:'');
      if(/^spotify:(track|episode):/.test(directUri)){
        const metadata=v.metadata||v.contextTrack?.metadata||{};
        const title=String(v.name||v.title||metadata.title||metadata.name||
          v.contextTrack?.name||v.contextTrack?.title||'').trim();
        let artists=artistNames(v.artists);
        if(!artists.length)artists=artistNames(v.album?.artists);
        if(!artists.length)artists=artistNames(v.contextTrack?.artists);
        if(!artists.length&&metadata.artist_name)artists=[String(metadata.artist_name).trim()].filter(Boolean);
        const album=String(v.album?.name||v.album?.title||metadata.album_title||
          metadata.album_name||v.contextTrack?.album?.name||'').trim();
        const albumUri=String(v.album?.uri||metadata.album_uri||v.contextTrack?.album?.uri||'');
        const artistUris=(Array.isArray(v.artists)?v.artists:
          (Array.isArray(v.album?.artists)?v.album.artists:[]))
          .map(a=>a?.uri).filter(x=>typeof x==='string'&&x.startsWith('spotify:artist:'));
        const score=(directUri===preferredUri?100:0)+20+(title?8:0)+(artists.length?6:0)+(album?4:0);
        if(score>bestScore){
          best={uri:directUri,title,artists,album,albumUri,artistUris};
          bestScore=score;
        }
      }
      for(const x of Object.values(v)){
        if(x&&typeof x==='object'&&!seen.has(x))stack.push(x);
      }
    }
  }
  return best;
}
function rowInfo(inputRow){
  const row=canonicalRow(inputRow);
  if(!row)return null;
  const trackLink=row.querySelector('a[href*="/track/"],a[href*="/episode/"],[data-testid="internal-track-link"]');
  const hrefUri=uriFromHref(trackLink?.getAttribute?.('href')||'');
  const domTitle=(trackLink?.textContent||'').trim();
  const artistLinks=[...row.querySelectorAll('a[href*="/artist/"]')];
  let artists=artistLinks.map(a=>(a.textContent||'').trim()).filter(Boolean);
  let artistUris=artistLinks.map(a=>uriFromHref(a.getAttribute('href')||'')).filter(x=>x.startsWith('spotify:artist:'));
  const albumLink=row.querySelector('a[href*="/album/"]');
  let album=(albumLink?.textContent||'').trim();
  const albumHref=albumLink?.getAttribute?.('href')||'';
  const albumMatch=albumHref.match(/\/album\/([A-Za-z0-9]+)/);
  const needReact=!hrefUri||!domTitle||!artists.length||!album;
  const react=needReact?reactTrackData(row,hrefUri):null;
  const uri=hrefUri||react?.uri||'';
  if(!uri)return null;
  const title=(domTitle||react?.title||
    row.querySelector('[data-testid="internal-track-link"],[dir="auto"]')?.textContent||'').trim();
  if(!title)return null;
  if(!artists.length&&react?.artists?.length)artists=react.artists;
  if(!artistUris.length&&react?.artistUris?.length)artistUris=react.artistUris;
  if(!album&&react?.album)album=react.album.trim();
  const albumUri=albumMatch?'spotify:album:'+albumMatch[1]:
    (react?.albumUri?.startsWith?.('spotify:album:')?react.albumUri:'');
  const pageMatch=location.pathname.match(/\/(playlist|album|artist)\/([A-Za-z0-9]+)/);
  const contextUri=pageMatch?'spotify:'+pageMatch[1]+':'+pageMatch[2]:'';
  const pageTitle=(row.closest('section,[data-testid$="-page"]')?.querySelector('h1')?.textContent||
    document.querySelector('main h1')?.textContent||'').trim();
  if(!album&&pageMatch?.[1]==='album')album=pageTitle;
  if(!artists.length&&pageMatch?.[1]==='artist'&&pageTitle)artists=[pageTitle];
  const artist=artists[0]||'';
  const allArtists=artists.join(', ')||artist;
  const ignoreUris=[uri,albumUri,contextUri,...artistUris].filter(Boolean);
  return {row,uri,title,artist,album,allArtists,albumUri,contextUri,artistUris,ignoreUris};
}
sgf.trackInfoFromRows=rows=>{
  const source=rows?[...rows]:[...document.querySelectorAll('div[data-testid="tracklist-row"],.main-trackList-trackListRow,div[role="row"]')];
  const out=[],seenUri=new Set(),seenRow=new Set();
  for(const candidate of source){
    const row=canonicalRow(candidate);if(!row||seenRow.has(row))continue;
    seenRow.add(row);
    const info=rowInfo(row);if(!info||seenUri.has(info.uri))continue;
    seenUri.add(info.uri);out.push(info);
  }
  return out;
};
function encodeBatch(infos){
  return infos.slice(0,128).map(i=>[i.uri,i.title,i.artist,i.album,i.allArtists].join(sgf.FS)).join(sgf.RS);
}
function decodeBatch(payload){
  const map=new Map();
  for(const record of String(payload||'').split(sgf.RS)){
    if(!record)continue;
    const [uri,status,path,message]=record.split(sgf.FS);
    if(uri)map.set(uri,{status:status||'',path:path||'',message:message||''});
  }
  return map;
}
sgf.statusQueue=[];
sgf.statusActive=null;
sgf.requestStatuses=infos=>new Promise(resolve=>{
  const clean=(infos||[]).filter(i=>i?.uri&&i?.title).slice(0,128);
  if(!clean.length){resolve(new Map());return;}
  sgf.statusQueue.push({infos:clean,resolve});
  sgf.pumpStatusQueue();
});
sgf.pumpStatusQueue=()=>{
  if(sgf.statusActive||!sgf.statusQueue.length)return;
  sgf.statusActive=sgf.statusQueue.shift();
  sgf.send('status_batch',encodeBatch(sgf.statusActive.infos));
  sgf.statusActive.timer=setTimeout(()=>{
    const job=sgf.statusActive;sgf.statusActive=null;
    job?.resolve(new Map());sgf.pumpStatusQueue();
  },10000);
};
let lastStatusDiag='';
window.__soggfyReceiveStatuses=payload=>{
  const map=decodeBatch(payload);
  const job=sgf.statusActive;
  if(job){clearTimeout(job.timer);sgf.statusActive=null;job.resolve(map);}
  for(const [uri,value] of map)sgf.statusMap.set(uri,value);
  const rendered=sgf.renderVisibleStatuses?.()||0;
  const statuses=[...map.values()].filter(v=>v.status).length;
  const diag='rows='+(job?.infos?.length||0)+' results='+map.size+' statuses='+statuses+' rendered='+rendered;
  if(diag!==lastStatusDiag){lastStatusDiag=diag;console.info('FLOGGFY_STATUS:classic '+diag);}
  sgf.pumpStatusQueue();
};

function statusCard(info){
  const n=document.createElement('div');n.className='sgf-status-indicator';
  const card=document.createElement('div');card.className='sgf-status-indicator-card';
  if(info.status==='DONE'&&info.path){
    const b=document.createElement('button');b.className='sgf-status-browse-button';
    b.innerHTML=sgf.Icons.Folder+'<span style="padding-left:4px;font-size:16px;color:#ddd">Open Folder</span>';
    b.onclick=e=>{e.stopPropagation();sgf.send('open_folder',info.path);};
    card.appendChild(b);
  }else{
    const s=document.createElement('span');s.style.cssText='font-size:16px;color:#ddd';
    s.textContent=info.message||info.status||'';card.appendChild(s);
  }
  n.append(card,sgf.parseHtml(STATUS_ICON[info.status]||sgf.Icons.Warning));
  n.__sgfStatus=info.status;return n;
}
function mountStatusIndicator(row,node){
  const target=row.lastElementChild;
  if(target&&target!==row){target.prepend(node);return;}
  const more=row.querySelector('[data-testid="more-button"],.main-trackList-rowMoreButton');
  if(more?.parentElement){more.insertAdjacentElement('beforebegin',node);return;}
  row.appendChild(node);
}
sgf.renderVisibleStatuses=()=>{
  let rendered=0;
  for(const t of sgf.trackInfoFromRows()){
    let info=sgf.statusMap.get(t.uri);
    if((t.ignoreUris||[t.uri]).some(uri=>sgf.isIgnored(uri)))info={status:'IGNORED',message:'Ignored',path:''};
    let old=t.row.__sgf_status_ind;
    if(!info||!info.status){
      if(old){old.remove();delete t.row.__sgf_status_ind;}continue;
    }
    if(old?.__sgfStatus===info.status&&old?.isConnected){rendered++;continue;}
    old?.remove();
    const n=statusCard(info);
    mountStatusIndicator(t.row,n);t.row.__sgf_status_ind=n;rendered++;
  }
  return rendered;
};
let refreshTimer=0;
sgf.refreshVisibleStatuses=()=>{
  clearTimeout(refreshTimer);
  refreshTimer=setTimeout(async()=>{
    const infos=sgf.trackInfoFromRows();
    const statuses=await sgf.requestStatuses(infos);
    for(const [uri,value] of statuses)sgf.statusMap.set(uri,value);
    sgf.renderVisibleStatuses();
  },120);
};

async function queueSnapshot(){
  try{
    const q=await sgf.player?._queue?.getQueue?.();
    const list=q?.queued||q?.nextUp||q?.next||[];
    return Array.isArray(list)?list:[];
  }catch{return [];}
}
function queuedInfo(track){
  const uri=track?.uri||track?.contextTrack?.uri||'';
  const title=track?.name||track?.metadata?.title||track?.contextTrack?.metadata?.title||'';
  const artists=(track?.artists||track?.album?.artists||[]).map?.(a=>a?.name).filter?.(Boolean)||[];
  const artist=artists[0]||track?.metadata?.artist_name||'';
  const allArtists=artists.join(', ')||artist;
  const album=track?.album?.name||track?.metadata?.album_title||'';
  return {uri,title,artist,allArtists,album,track};
}
let queueBusy=false,lastQueueSig='';
sgf.checkQueue=async()=>{
  if(queueBusy||(!sgf.state.skipDownloaded&&!sgf.state.skipIgnored)||!sgf.player)return;
  queueBusy=true;
  try{
    const list=await queueSnapshot();
    const infos=list.map(queuedInfo).filter(x=>x.uri&&x.title);
    const sig=infos.map(x=>x.uri).join('|')+'|'+Number(sgf.state.skipDownloaded)+'|'+Number(sgf.state.skipIgnored);
    if(sig===lastQueueSig){queueBusy=false;return;}
    lastQueueSig=sig;
    let statuses=new Map();
    if(sgf.state.skipDownloaded)statuses=await sgf.requestStatuses(infos);
    const remove=infos.filter(x=>(sgf.state.skipIgnored&&sgf.isTrackIgnored(x.track))||
      (sgf.state.skipDownloaded&&statuses.get(x.uri)?.status==='DONE')).map(x=>x.track);
    if(remove.length){
      if(typeof sgf.player.removeFromQueue==='function')await sgf.player.removeFromQueue(remove);
      else if(typeof sgf.player._queue?.removeFromQueue==='function')await sgf.player._queue.removeFromQueue(remove);
      lastQueueSig='';
    }
  }catch(e){console.warn('Soggfy queue skip failed',e);}
  finally{queueBusy=false;}
};

sgf.installPlayerListeners=()=>{
  try{
    const events=sgf.player?.getEvents?.()||sgf.player?._events;
    events?.addListener?.('update',({data}={})=>{
      const item=data?.item||sgf.currentState()?.item;
      if(item?.uri){
        sgf.send('ignore_current',sgf.isTrackIgnored(item)?'1':'0');
        setTimeout(()=>sgf.refreshVisibleStatuses(),80);
      }
      if(sgf.state.playbackSpeed!==1)sgf.setPlaybackSpeed?.(sgf.state.playbackSpeed);
    });
    events?.addListener?.('queue_update',()=>sgf.checkQueue());
  }catch{}
  setInterval(()=>sgf.checkQueue(),1800);
};

const observer=new MutationObserver(mutations=>{
  let rows=false;
  for(const m of mutations)for(const n of m.addedNodes){
    if(n?.nodeType===1&&(n.matches?.('div[role="row"],div[data-testid="tracklist-row"]')||n.querySelector?.('div[role="row"],div[data-testid="tracklist-row"]'))){rows=true;break;}
  }
  if(rows)sgf.refreshVisibleStatuses();
});
const start=()=>{
  if(!document.body){setTimeout(start,100);return;}
  observer.observe(document.body,{childList:true,subtree:true});
  sgf.refreshVisibleStatuses();
  setInterval(()=>{
    if(document.visibilityState==='hidden'||sgf.statusActive||sgf.statusQueue.length)return;
    if(sgf.trackInfoFromRows().length)sgf.refreshVisibleStatuses();
  },1500);
};
start();
})();