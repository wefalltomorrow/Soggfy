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
sgf.setIgnored=(uri,value)=>{
  if(!uri)return;
  value?ignoreSet.add(uri):ignoreSet.delete(uri);
  try{localStorage.setItem(ignoreKey,JSON.stringify([...ignoreSet]));}catch{}
  sgf.refreshVisibleStatuses?.();
};

function uriFromHref(href){
  if(!href)return '';
  const m=href.match(/\/(track|episode)\/([A-Za-z0-9]+)/);
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
  const artists=[...row.querySelectorAll('a[href*="/artist/"]')].map(a=>(a.textContent||'').trim()).filter(Boolean);
  let album=(row.querySelector('a[href*="/album/"]')?.textContent||'').trim();
  if(!album){
    const section=row.closest('section,[data-testid$="-page"]');
    album=(section?.querySelector('h1')?.textContent||document.querySelector('main h1')?.textContent||'').trim();
  }
  const artist=artists[0]||'';
  const allArtists=artists.join(', ')||artist;
  return {row,uri,title,artist,album,allArtists};
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
sgf.renderVisibleStatuses=()=>{
  for(const t of sgf.trackInfoFromRows()){
    let info=sgf.statusMap.get(t.uri);
    if(sgf.isIgnored(t.uri))info={status:'IGNORED',message:'Ignored',path:''};
    let old=t.row.__sgf_status_ind;
    if(!info||!info.status){
      if(old){old.remove();delete t.row.__sgf_status_ind;}continue;
    }
    if(old?.__sgfStatus===info.status&&old?.isConnected)continue;
    old?.remove();
    const n=statusCard(info);
    const target=t.row.lastElementChild||t.row;
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
    const remove=infos.filter(x=>(sgf.state.skipIgnored&&sgf.isIgnored(x.uri))||
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
        sgf.send('ignore_current',sgf.isIgnored(item.uri)?'1':'0');
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
};
start();
})();