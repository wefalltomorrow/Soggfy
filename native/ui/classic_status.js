(() => {
'use strict';
const sgf=window.__SoggfyClassic;
if(!sgf||sgf.__statusLoaded)return;
sgf.__statusLoaded=true;

const STATUS_ICON={
  ERROR:sgf.Icons.Error,IN_PROGRESS:sgf.Icons.InProgress,CONVERTING:sgf.Icons.Processing,
  WARN:sgf.Icons.Warning,DONE:sgf.Icons.Done,IGNORED:sgf.Icons.SyncDisabled
};
// Match original Sprinkles: indicators belong ONLY to Spotify track-list rows
// inside the main view. Generic ARIA rows also occur in cards, home, and sidebar.
const TRACK_ROW='div[data-testid="tracklist-row"],.main-trackList-trackListRow';
sgf.mainTrackView=()=>document.querySelector('.main-view-container__scroll-node-child')||
  document.querySelector('[data-testid="main-view"]')||
  document.querySelector('main');
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
const trueTrackUri=uri=>/^spotify:(track|episode):[A-Za-z0-9]{10,}$/.test(uri||'');

// Original Rafiuth/Soggfy resolves the URI through the *same row's* React
// menu props. Recursive Fiber searching wrongly returns the first playlist
// track's URI for unrelated rows.
function upstreamMenuProps(row,menu){
  const key=Object.keys(row).find(k=>k.startsWith('__reactProps$'));
  if(!key||!menu||!row.contains(menu))return null;
  const path=[];
  for(let el=menu;el&&el!==row;el=el.parentElement){
    let offset=0;
    for(let sibling=el;sibling;sibling=sibling.previousElementSibling)
      if(key in sibling)offset++;
    if(!offset)return null;
    path.push(offset);
  }
  let state=row[key];
  const fragment=Symbol.for('react.fragment');
  for(let n=path.length-1;n>=0&&state;n--){
    const children=state.children?.type===fragment?
      state.children.props?.children:state.children;
    const list=Array.isArray(children)?children:[children];
    let count=0,next=null;
    for(const child of list){
      if(!child||typeof child!=='object')continue;
      const nested=child.type===fragment?child.props?.children:null;
      count+=Array.isArray(nested)?nested.length:1;
      if(count>=path[n]){next=child;break;}
    }
    state=next?.props||null;
  }
  return state;
}
function uriFromTrackState(state){
  const choices=[
    state?.menu?.props?.uri,state?.menu?.uri,state?.uri,
    state?.item?.uri,state?.track?.uri,state?.trackUri,
    state?.data?.uri,state?.contextTrack?.uri,state?.props?.uri
  ];
  return choices.find(trueTrackUri)||'';
}
function reactUri(row){
  const menu=row.querySelector('[data-testid="more-button"],.main-trackList-rowMoreButton');
  if(menu){
    const uri=uriFromTrackState(upstreamMenuProps(row,menu));
    if(uri)return uri;
  }
  for(const el of [menu,row]){
    if(!el)continue;
    for(const k of Object.keys(el).filter(k=>k.startsWith('__reactProps$'))){
      const uri=uriFromTrackState(el[k]);
      if(uri)return uri;
    }
  }
  return '';
}
// When Spotify does not expose a URI in a track-list row, this deterministic
// key is used ONLY for requesting filename/native status of that specific
// track. We never traverse a parent Fiber or treat home cards as tracks.
function localQueryUri(title,artist,album){
  const value=[title,artist,album].join('\x1f');
  let h=2166136261,h2=2246822507;
  for(let i=0;i<value.length;i++)h=Math.imul(h^value.charCodeAt(i),16777619)>>>0;
  for(let i=value.length-1;i>=0;i--)h2=Math.imul(h2^value.charCodeAt(i),3266489909)>>>0;
  return 'spotify:track:sgf'+h.toString(16).padStart(8,'0')+h2.toString(16).padStart(8,'0');
}
function rowInfo(row){
  if(!(row instanceof Element)||!row.matches(TRACK_ROW))return null;
  const trackLink=row.querySelector('a[href*="/track/"],a[href*="/episode/"],[data-testid="internal-track-link"]');
  // On Spotify 1.3.x the row itself normally carries data-testid=tracklist-row.
  // A selector prefixed with that attribute does not match its own children.
  const title=(trackLink?.textContent||
    row.querySelector('.main-trackList-rowTitle,[data-testid="tracklist-row-title"],[dir="auto"]')?.textContent||'').trim();
  if(!title)return null;
  const artistLinks=[...row.querySelectorAll('a[href*="/artist/"]')];
  let artists=artistLinks.map(a=>(a.textContent||'').trim()).filter(Boolean);
  if(!artists.length){
    const artistText=(row.querySelector('.main-trackList-rowSubTitle,[data-testid="tracklist-row-artist"]')?.textContent||'').trim();
    if(artistText)artists=[artistText];
  }
  const nativeUri=uriFromHref(trackLink?.getAttribute?.('href')||'')||reactUri(row);
  const artistUris=artistLinks.map(a=>uriFromHref(a.getAttribute('href')||'')).filter(x=>x.startsWith('spotify:artist:'));
  const albumLink=row.querySelector('a[href*="/album/"]');
  let album=(albumLink?.textContent||'').trim();
  const albumHref=albumLink?.getAttribute?.('href')||'';
  const albumMatch=albumHref.match(/\/album\/([A-Za-z0-9]+)/);
  const albumUri=albumMatch?'spotify:album:'+albumMatch[1]:'';
  const pageMatch=location.pathname.match(/\/(playlist|album|artist)\/([A-Za-z0-9]+)/);
  const contextUri=pageMatch?'spotify:'+pageMatch[1]+':'+pageMatch[2]:'';
  if(!album){
    album=(row.querySelector('[data-testid="tracklist-row-album"],.main-trackList-rowSectionVariable')?.textContent||'').trim();
    // A playlist title must never be mistaken for a track's album.
    if(!album&&location.pathname.startsWith('/album/'))
      album=(row.closest('section')?.querySelector('h1')?.textContent||'').trim();
  }
  const artist=artists[0]||'';
  const allArtists=artists.join(', ')||artist;
  if(!nativeUri&&!artist)return null;
  const uri=nativeUri||localQueryUri(title,artist,album);
  const ignoreUris=[nativeUri,albumUri,contextUri,...artistUris].filter(Boolean);
  return {row,uri,title,artist,album,allArtists,albumUri,contextUri,artistUris,ignoreUris};
}
sgf.trackInfoFromRows=rows=>{
  const view=sgf.mainTrackView();
  if(!view)return [];
  const source=rows?[...rows].filter(row=>row?.matches?.(TRACK_ROW)&&view.contains(row)):
    [...view.querySelectorAll(TRACK_ROW)];
  const out=[];
  for(const row of source){
    const info=rowInfo(row);
    if(info)out.push(info);
  }
  // A duplicated song still has multiple visible rows: each gets its badge.
  return out;
};

const STATUS_LIMITS={batch:128,pending:16,map:1024,queue:2048,timeout:10000,missingAge:3000,doneAge:3000,visibleAge:15000};
sgf.statusLimits=STATUS_LIMITS;
const statusSession=Date.now().toString(36)+'-'+Math.random().toString(36).slice(2);
let statusSequence=0,statusEpoch=0,nativeRevision=null;
function encodeBatch(infos){
  const field=value=>String(value||'').replace(/[\x1d-\x1f]/g,' ');
  return infos.slice(0,STATUS_LIMITS.batch).map(i=>[i.uri,i.title,i.artist,i.album,i.allArtists].map(field).join(sgf.FS)).join(sgf.RS);
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
function finishStatus(job,map){
  clearTimeout(job.timer);
  if(sgf.statusActive===job)sgf.statusActive=null;
  job.resolve(map);
}
sgf.invalidateStatusCache=()=>{
  ++statusEpoch;
  pendingQueueEvent=null;
  sgf.statusMap.clear();queueStatusCache.clear();
  if(sgf.statusActive)finishStatus(sgf.statusActive,new Map());
  for(const job of sgf.statusQueue.splice(0))finishStatus(job,new Map());
  sgf.refreshVisibleStatuses?.();
};
window.__soggfyStatusRevision=revision=>{
  if(nativeRevision!==null&&nativeRevision!==revision)sgf.invalidateStatusCache();
  nativeRevision=revision;
};
sgf.requestStatuses=infos=>{
  const clean=(infos||[]).filter(i=>i?.uri&&i?.title).slice(0,STATUS_LIMITS.batch);
  if(!clean.length)return Promise.resolve(new Map());
  const payload=encodeBatch(clean);
  const duplicate=[sgf.statusActive,...sgf.statusQueue].find(job=>job?.payload===payload&&job.epoch===statusEpoch);
  if(duplicate)return duplicate.promise;
  if(sgf.statusQueue.length>=STATUS_LIMITS.pending)finishStatus(sgf.statusQueue.shift(),new Map());
  const job={id:statusSession+'-'+(++statusSequence),payload,infos:clean,epoch:statusEpoch};
  job.promise=new Promise(resolve=>{job.resolve=resolve;});
  sgf.statusQueue.push(job);
  sgf.pumpStatusQueue();
  return job.promise;
};
sgf.pumpStatusQueue=()=>{
  if(sgf.statusActive||!sgf.statusQueue.length)return;
  const job=sgf.statusActive=sgf.statusQueue.shift();
  job.timer=setTimeout(()=>{
    if(sgf.statusActive!==job)return;
    finishStatus(job,new Map());sgf.pumpStatusQueue();
  },STATUS_LIMITS.timeout);
  try{sgf.send('status_batch',job.id+'\x1d'+job.payload);}
  catch{finishStatus(job,new Map());sgf.pumpStatusQueue();}
};
window.__soggfyReceiveStatuses=payload=>{
  const split=String(payload).indexOf('\x1d');
  if(split<0)return;
  const job=sgf.statusActive;
  if(!job||payload.slice(0,split)!==job.id||job.epoch!==statusEpoch)return;
  const wanted=new Set(job.infos.map(info=>info.uri));
  const map=decodeBatch(payload.slice(split+1));
  const now=Date.now();
  for(const [uri,value] of map){
    if(!wanted.has(uri)){map.delete(uri);continue;}
    value.checkedAt=now;
    sgf.statusMap.delete(uri);sgf.statusMap.set(uri,value);
    const cached=queueStatusCache.get(uri);
    if(cached&&cached.done!==(value.status==='DONE'))queueStatusCache.delete(uri);
  }
  while(sgf.statusMap.size>STATUS_LIMITS.map)sgf.statusMap.delete(sgf.statusMap.keys().next().value);
  finishStatus(job,map);
  sgf.renderVisibleStatuses?.();
  sgf.pumpStatusQueue();
};

function statusCard(info){
  const n=document.createElement('div');n.className='sgf-status-indicator';
  const label=info.message||(info.status==='DONE'?'Downloaded':info.status);
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
    let info=sgf.statusMap.get(t.uri);
    if(info?.checkedAt!==undefined&&Date.now()-info.checkedAt>STATUS_LIMITS.visibleAge){sgf.statusMap.delete(t.uri);info=null;}
    if((t.ignoreUris||[t.uri]).some(uri=>sgf.isIgnored(uri)))info={status:'IGNORED',message:'Ignored',path:''};
    const old=t.row.__sgf_status_ind;
    // Original Soggfy shows a red X for ERROR, not for every absent file.
    if(!info?.status){
      if(old){old.remove();delete t.row.__sgf_status_ind;}
      continue;
    }
    const token=[info.status,info.path||'',info.message||''].join('\x1f');
    if(old?.__sgfToken===token&&old?.isConnected)continue;
    old?.remove();
    const n=statusCard(info);
    // Only the trailing cell of a verified track row; never prepend to row
    // itself, sidebar content, cards, track headings or playlist artwork.
    const target=t.row.querySelector('.main-trackList-rowSectionEnd')||
      t.row.querySelector('[data-testid="tracklist-row-duration"]')?.parentElement||
      t.row.lastElementChild;
    if(!target||target===t.row)continue;
    target.prepend(n);t.row.__sgf_status_ind=n;
  }
};
let refreshTimer=0,lastStatusDiag=0,visibleRevision=0;
sgf.refreshVisibleStatuses=()=>{
  const revision=++visibleRevision;
  clearTimeout(refreshTimer);
  refreshTimer=setTimeout(async()=>{
    const rows=sgf.trackInfoFromRows();
    // Resolve each URI once, starting with the viewport when row geometry is available.
    const inView=info=>{const r=info.row?.getBoundingClientRect?.();return !r||
      (r.bottom>=0&&r.top<=(window.innerHeight||Infinity));};
    const infos=[...new Map([...rows.filter(inView),...rows.filter(i=>!inView(i))]
      .filter(i=>i?.uri&&i?.title).map(i=>[i.uri,i])).values()];
    const epoch=statusEpoch,path=location.pathname,statuses=new Map();
    for(let i=0;i<infos.length;i+=STATUS_LIMITS.batch){
      if(epoch!==statusEpoch||revision!==visibleRevision||path!==location.pathname)return;
      const page=await sgf.requestStatuses(infos.slice(i,i+STATUS_LIMITS.batch));
      if(epoch!==statusEpoch||revision!==visibleRevision||path!==location.pathname)return;
      for(const [uri,value] of page)statuses.set(uri,value);
    }
    sgf.renderVisibleStatuses();
    // A low-frequency diagnostic makes it possible to distinguish failed
    // row discovery from a stale or unreturned native status response.
    const now=Date.now();
    if(sgf.state.debug && now-lastStatusDiag>=20000){
      lastStatusDiag=now;
      const counts={};
      for(const value of statuses.values())counts[value.status||'NONE']=(counts[value.status||'NONE']||0)+1;
      sgf.send('status_diag','rows='+infos.length+' responses='+statuses.size+
        ' local_ids='+infos.filter(x=>x.uri.startsWith('spotify:track:sgf')).length+
        ' states='+JSON.stringify(counts));
    }
  },120);
};

// Upstream Soggfy (Sprinkles/src/player-state-tracker.ts) operates on
// the queue_update event's data.nextUp. The event is authoritative: do
// not silently replace its tracks with a separate/stale queued snapshot.
// Spotify 1.3.x can also expose nextTracks and next_tracks.
async function queueSnapshot(eventData,player){
  const candidates=[
    ['event.nextUp',eventData?.nextUp],
    ['event.queue.nextUp',eventData?.queue?.nextUp],
    ['event.nextTracks',eventData?.nextTracks],
    ['Spicetify.Queue.nextTracks',window.Spicetify?.Queue?.nextTracks],
    ['player.next_tracks',sgf.currentState?.()?.next_tracks]
  ];
  for(const [source,list] of candidates)
    if(Array.isArray(list))return {list,source};
  try{
    const snapshot=await player?._queue?.getQueue?.();
    for(const key of ['nextUp','nextTracks','queued','next']){
      if(Array.isArray(snapshot?.[key]))
        return {list:snapshot[key],source:'getQueue.'+key};
    }
  }catch(e){
    if(sgf.state.debug)sgf.send('status_diag','queue snapshot error '+String(e).slice(0,100));
  }
  return {list:null,source:'unavailable'};
}
function queuedInfo(entry){
  // A modern ProvidedTrack is a ContextTrack with metadata, whereas an
  // older queue_update entry includes name, artists and album objects.
  const track=entry?.contextTrack||entry?.track||entry?.item||entry;
  const meta=track?.metadata||entry?.metadata||{};
  const uri=track?.uri||entry?.uri||'';
  const title=track?.name||entry?.name||meta.title||'';
  const artistRecords=track?.artists||entry?.artists||track?.album?.artists||[];
  const artists=Array.isArray(artistRecords)?
    artistRecords.map(a=>typeof a==='string'?a:a?.name).filter(Boolean):[];
  const artist=artists[0]||meta.artist_name||meta.album_artist_name||'';
  const allArtists=artists.join(', ')||artist;
  const album=track?.album?.name||entry?.album?.name||meta.album_title||'';
  const uid=entry?.uid??track?.uid;
  return {uri,title,artist,allArtists,album,
    // Original Soggfy removes queued ContextTracks, preserving a per-entry UID.
    track:uid==null?{uri}:{uri,uid},ignoreTarget:track};
}
// Cache confirmed results briefly; settings/publication changes invalidate them.
// Keep each queue UID and never cache an unanswered lookup.
const queueStatusCache=new Map();
let queueRun=null,lastQueueDiagnostic=0,queueRevision=0,pendingQueueEvent=null;
// A timeout cannot cancel a player mutation. Keep its exact entries reserved
// until the underlying promise settles, including across status invalidation.
const queueRemovals=new WeakMap();
const queueEntryKey=track=>JSON.stringify([track.uri,track.uid??null]);
function boundedQueueCall(call){
  return new Promise((resolve,reject)=>{
    const timer=setTimeout(()=>reject(Error('Player queue operation timed out')),STATUS_LIMITS.timeout);
    Promise.resolve().then(call).then(value=>{clearTimeout(timer);resolve(value);},error=>{clearTimeout(timer);reject(error);});
  });
}
function queueDiagnostic(details,force=false){
  const now=Date.now();
  if(!sgf.state.debug||(!force&&now-lastQueueDiagnostic<20000))return;
  lastQueueDiagnostic=now;
  sgf.send('status_diag','queue '+details.slice(0,430));
}
sgf.checkQueue=async eventData=>{
  const policy=()=>[!!sgf.state.skipDownloaded,!!sgf.state.skipIgnored].join(',');
  if(eventData!==undefined){
    ++queueRevision;
    pendingQueueEvent={data:eventData,player:sgf.player,epoch:statusEpoch,policy:policy(),time:Date.now()};
  }
  if((!sgf.state.skipDownloaded&&!sgf.state.skipIgnored)||!sgf.player){pendingQueueEvent=null;return;}
  if(queueRun?.player===sgf.player&&queueRun?.epoch===statusEpoch)return;
  if(pendingQueueEvent){
    const pending=pendingQueueEvent;pendingQueueEvent=null;
    const age=Date.now()-pending.time;
    eventData=pending.player===sgf.player&&pending.epoch===statusEpoch&&pending.policy===policy()&&
      age>=0&&age<STATUS_LIMITS.doneAge?pending.data:undefined;
  }
  const epoch=statusEpoch,player=sgf.player,revision=queueRevision;
  const run=queueRun={player,epoch};
  const current=()=>queueRun===run&&epoch===statusEpoch&&player===sgf.player&&revision===queueRevision;
  try{
    const {list,source}=await boundedQueueCall(()=>queueSnapshot(eventData,player));
    if(!current())return;
    // A missing snapshot is not an empty queue: retry on the next event/poll.
    if(!list){queueDiagnostic('source=unavailable');return;}
    const infos=list.map(queuedInfo).filter(info=>info.uri);
    const uris=new Set(infos.map(info=>info.uri));
    for(const uri of queueStatusCache.keys())if(!uris.has(uri))queueStatusCache.delete(uri);
    const unique=new Map();
    for(const info of infos)
      if(info.title&&info.artist&&info.album&&!unique.has(info.uri))unique.set(info.uri,info);
    if(sgf.state.skipDownloaded){
      const key=info=>JSON.stringify([info.title,info.artist,info.album,info.allArtists]);
      const pending=[...unique.values()].filter(info=>{
        const cached=queueStatusCache.get(info.uri);
        return !cached||cached.key!==key(info)||Date.now()<cached.time||
          Date.now()-cached.time>=(cached.done?STATUS_LIMITS.doneAge:STATUS_LIMITS.missingAge);
      });
      // The native status batch format has a 128-track limit.
      for(let i=0;i<pending.length;i+=STATUS_LIMITS.batch){
        const page=pending.slice(i,i+STATUS_LIMITS.batch);
        const statuses=await sgf.requestStatuses(page);
        if(!current())return;
        for(const info of page){
          // A timeout/partial response must not poison the queue cache.
          if(statuses.has(info.uri))
            queueStatusCache.set(info.uri,{done:statuses.get(info.uri)?.status==='DONE',time:Date.now(),key:key(info)});
          else queueStatusCache.delete(info.uri);
        }
      }
    }
    if(!current())return;
    let inFlight=queueRemovals.get(player);
    if(!inFlight){inFlight=new Set();queueRemovals.set(player,inFlight);}
    const remove=infos.filter(info=>
      (sgf.state.skipIgnored&&sgf.isTrackIgnored(info.ignoreTarget))||
      (sgf.state.skipDownloaded&&queueStatusCache.get(info.uri)?.done===true&&Date.now()-queueStatusCache.get(info.uri).time<STATUS_LIMITS.doneAge))
      .map(info=>info.track).filter(track=>!inFlight.has(queueEntryKey(track)))
      .slice(0,Math.max(0,STATUS_LIMITS.queue-inFlight.size));
    if(remove.length){
      const removeFn=player.removeFromQueue||player._queue?.removeFromQueue;
      if(typeof removeFn==='function'){
        // Failed removals must trigger a fresh disk lookup on the next attempt.
        const removedCache=remove.map(track=>[track.uri,queueStatusCache.get(track.uri)]);
        for(const track of remove)queueStatusCache.delete(track.uri);
        for(const track of remove)inFlight.add(queueEntryKey(track));
        await boundedQueueCall(()=>Promise.resolve().then(()=>{
          if(!current())throw Error('Queue changed before removal');
          return removeFn.call(typeof player.removeFromQueue==='function'?player:player._queue,remove);
        })
          .finally(()=>{for(const track of remove)inFlight.delete(queueEntryKey(track));}));
        if(current())for(const [uri,cached] of removedCache)if(cached)queueStatusCache.set(uri,cached);
        queueDiagnostic('removed='+remove.length+' source='+source,true);
      }else{
        queueDiagnostic('removeFromQueue unavailable source='+source,true);
      }
    }else{
      queueDiagnostic('source='+source+' upcoming='+infos.length+
        ' queryable='+unique.size+' matched='+[...queueStatusCache.values()].filter(value=>value.done).length+
        ' unresolved='+[...unique.keys()].filter(uri=>!queueStatusCache.has(uri)).length);
    }
  }catch(e){
    queueDiagnostic('failed '+String(e).slice(0,160),true);
    console.warn('Soggfy queue skip failed',e);
  }finally{
    while(queueStatusCache.size>STATUS_LIMITS.queue)queueStatusCache.delete(queueStatusCache.keys().next().value);
    if(queueRun===run){
      queueRun=null;
      if(pendingQueueEvent)setTimeout(()=>sgf.checkQueue(),0);
    }
  }
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
    events?.addListener?.('queue_update',({data}={})=>sgf.checkQueue(data));

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
  sgf.installPlaybackStallRecovery?.();
};

const observer=new MutationObserver(mutations=>{
  let rows=false;
  for(const m of mutations)for(const n of m.addedNodes){
    if(n?.nodeType===1&&(n.matches?.(TRACK_ROW)||n.querySelector?.(TRACK_ROW))){rows=true;break;}
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
  if(sgf.mainTrackView()?.querySelector(TRACK_ROW))sgf.refreshVisibleStatuses();
},3000);
})();
