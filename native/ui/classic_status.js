(() => {
'use strict';
const sgf=window.__SoggfyClassic;
if(!sgf||sgf.__statusLoaded)return;
sgf.__statusLoaded=true;

const STATUS_ICON={
  ERROR:sgf.Icons.Error,IN_PROGRESS:sgf.Icons.InProgress,CONVERTING:sgf.Icons.Processing,
  WARN:sgf.Icons.Warning,DONE:sgf.Icons.Done,IGNORED:sgf.Icons.SyncDisabled,
  MISSING:sgf.Icons.Error
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
function deepFindUri(root){
  const stack=[root],seen=new Set();let budget=500;
  while(stack.length&&budget-->0){
    const v=stack.pop();if(!v||typeof v!=='object'||seen.has(v))continue;seen.add(v);
    for(const [k,x] of Object.entries(v)){
      if(typeof x==='string'&&/^spotify:(track|episode):/.test(x))return x;
      if(x&&typeof x==='object'&&!seen.has(x))stack.push(x);
    }
  }
  return '';
}
function reactUri(row){
  for(const e of [row,...row.querySelectorAll('[data-testid="more-button"],button')]){
    const key=Object.keys(e).find(k=>k.startsWith('__reactProps$')||k.startsWith('__reactFiber$'));
    if(key){const uri=deepFindUri(e[key]);if(uri)return uri;}
  }
  return '';
}
function rowInfo(row){
  if(!(row instanceof Element))return null;
  const trackLink=row.querySelector('a[href*="/track/"],a[href*="/episode/"],[data-testid="internal-track-link"]');
  const uri=uriFromHref(trackLink?.getAttribute?.('href')||'')||reactUri(row);
  if(!uri)return null;
  const title=(trackLink?.textContent||row.querySelector('[data-testid="tracklist-row"] [dir="auto"]')?.textContent||'').trim();
  if(!title)return null;
  const artistLinks=[...row.querySelectorAll('a[href*="/artist/"]')];
  const artists=artistLinks.map(a=>(a.textContent||'').trim()).filter(Boolean);
  const artistUris=artistLinks.map(a=>uriFromHref(a.getAttribute('href')||'')).filter(x=>x.startsWith('spotify:artist:'));
  const albumLink=row.querySelector('a[href*="/album/"]');
  let album=(albumLink?.textContent||'').trim();
  const albumHref=albumLink?.getAttribute?.('href')||'';
  const albumMatch=albumHref.match(/\/album\/([A-Za-z0-9]+)/);
  const albumUri=albumMatch?'spotify:album:'+albumMatch[1]:'';
  const pageMatch=location.pathname.match(/\/(playlist|album|artist)\/([A-Za-z0-9]+)/);
  const contextUri=pageMatch?'spotify:'+pageMatch[1]+':'+pageMatch[2]:'';
  if(!album){
    const section=row.closest('section,[data-testid$="-page"]');
    album=(section?.querySelector('h1')?.textContent||document.querySelector('main h1')?.textContent||'').trim();
  }
  const artist=artists[0]||'';
  const allArtists=artists.join(', ')||artist;
  const ignoreUris=[uri,albumUri,contextUri,...artistUris].filter(Boolean);
  return {row,uri,title,artist,album,allArtists,albumUri,contextUri,artistUris,ignoreUris};
}
sgf.trackInfoFromRows=rows=>{
  const source=rows?[...rows]:[...document.querySelectorAll('div[data-testid="tracklist-row"],.main-trackList-trackListRow,div[role="row"]')];
  const out=[],seen=new Set();
  for(const row of source){
    const info=rowInfo(row);if(!info||seen.has(info.uri))continue;
    seen.add(info.uri);out.push(info);
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
window.__soggfyReceiveStatuses=payload=>{
  const map=decodeBatch(payload);
  const job=sgf.statusActive;
  if(job){clearTimeout(job.timer);sgf.statusActive=null;job.resolve(map);}
  for(const [uri,value] of map)sgf.statusMap.set(uri,value);
  sgf.renderVisibleStatuses?.();
  sgf.pumpStatusQueue();
};

// A missing native status means no matching local file, not 'hide the badge'.
// Original Soggfy used a red X for these songs and a green tick for saved songs.
sgf.displayTrackStatus=info=>info?.status ? info : {status:'MISSING',message:'Not downloaded',path:''};

function statusCard(info){
  const n=document.createElement('div');n.className='sgf-status-indicator';
  const label=info.message||(info.status==='DONE'?'Downloaded':info.status==='MISSING'?'Not downloaded':info.status);
  n.title=label;
  n.setAttribute('aria-label',label);
  n.__sgfToken=[info.status,info.path||'',info.message||''].join('\x1f');
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
sgf.renderVisibleStatuses=()=>{
  for(const t of sgf.trackInfoFromRows()){
    let info=sgf.displayTrackStatus(sgf.statusMap.get(t.uri));
    if((t.ignoreUris||[t.uri]).some(uri=>sgf.isIgnored(uri)))info={status:'IGNORED',message:'Ignored',path:''};
    const old=t.row.__sgf_status_ind;
    const token=[info.status,info.path||'',info.message||''].join('\x1f');
    if(old?.__sgfToken===token&&old?.isConnected)continue;
    old?.remove();
    const n=statusCard(info);
    // Spotify 1.3.x uses a dedicated trailing duration cell. Prefer it to
    // an arbitrary last child (often an overlay or an invisible button).
    const target=t.row.querySelector('.main-trackList-rowSectionEnd')||
      t.row.querySelector('[data-testid="tracklist-row-duration"]')?.parentElement||
      t.row.lastElementChild||t.row;
    target.prepend(n);t.row.__sgf_status_ind=n;
  }
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
    });
    events?.addListener?.('queue_update',()=>sgf.checkQueue());

    // Match original Soggfy: only rebuild a track when Spotify itself reports
    // playback_stuck. Do not reset every natural transition.
    const client=events?._client;
    client?.getError?.({},err=>{
      try{
        const state=sgf.currentState?.();
        if(err?.message==='playback_stuck' &&
           (!err?.data?.playback_id||err.data.playback_id===state?.playbackId)){
          sgf.resetCurrentTrack?.(false);
        }
      }catch{}
    });
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
};
start();

// Statuses can change after asynchronous conversion with no playlist DOM
// mutation. Periodically reconcile visible rows with native disk/live status.
setInterval(()=>{
  if(document.visibilityState==='hidden'||sgf.statusActive||sgf.statusQueue.length)return;
  if(document.querySelector('div[data-testid="tracklist-row"],.main-trackList-trackListRow,div[role="row"]'))
    sgf.refreshVisibleStatuses();
},3000);
})();