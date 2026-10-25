(() => {
'use strict';
const sgf=window.__SoggfyClassic;
if(!sgf||sgf.__m3uLoaded)return;
sgf.__m3uLoaded=true;

const clean=value=>String(value??'').replace(/[\x1e\x1f\r\n]/g,' ').trim();
const MAX_TRACKS=10000;
let exporting=false;
async function bounded(call){
  let timer;
  try{return await Promise.race([Promise.resolve().then(call),new Promise((_,reject)=>{
    timer=setTimeout(()=>reject(Error('Playlist lookup timed out')),15000);
  })]);}finally{clearTimeout(timer);}
}
async function pages(fetch){
  const items=[];
  // Continue even after a short page: API implementations can cap page size.
  for(let page=0;page<102;page++){
    const result=await bounded(()=>fetch(items.length));
    const next=result?.items;
    if(!Array.isArray(next))throw Error('Unsupported playlist response');
    items.push(...next);
    if(items.length>MAX_TRACKS)throw Error('M3U export supports up to 10,000 tracks');
    if(Number.isFinite(result.total)&&items.length>=result.total)return items;
    if(!next.length){
      if(Number.isFinite(result.total)&&items.length<result.total)
        throw Error('Playlist response incomplete; please retry export');
      return items;
    }
  }
  throw Error('Playlist pagination did not finish');
}
const collectionFromLocation=()=>{
  const m=location.pathname.match(/\/(playlist|album)\/([A-Za-z0-9]+)/);
  return m?{type:m[1],uri:'spotify:'+m[1]+':'+m[2]}:null;
};
const durationMs=item=>{
  const v=item?.duration?.milliseconds??item?.duration_ms??item?.durationMs??item?.track?.duration?.milliseconds??item?.track?.duration_ms??0;
  return Math.max(0,Number(v)||0);
};
const toInfo=item=>{
  const x=item?.track||item;
  const artists=(x?.artists||x?.album?.artists||[]).map?.(a=>a?.name).filter?.(Boolean)||[];
  const metadata=x?.metadata||{};
  const uri=x?.uri||item?.uri||'';
  const title=x?.name||metadata.title||'';
  const artist=artists[0]||metadata.artist_name||'';
  const allArtists=artists.join(', ')||artist;
  const album=x?.album?.name||metadata.album_title||'';
  return {uri,title,artist,allArtists,album,durationMs:durationMs(x)};
};
async function playlistTracks(uri){
  try{
    const api=sgf.platform?.getPlaylistAPI?.()||window.Spicetify?.Platform?.PlaylistAPI;
    if(!api)return null;
    const meta=await bounded(()=>api.getMetadata?.(uri));
    const items=await pages(async offset=>{
      const result=await api.getContents(uri,{limit:500,offset});
      return {items:result?.items||result?.tracks?.items,
        total:result?.totalLength??result?.total??result?.tracks?.total};
    });
    return {name:meta?.name||document.querySelector('main h1')?.textContent||'Spotify playlist',
            tracks:items.map(toInfo).filter(x=>x.uri&&x.title)};
  }catch(e){console.warn('Soggfy playlist M3U lookup failed',e);throw e;}
}
async function albumTracks(uri){
  try{
    const sp=window.Spicetify;
    const definition=sp?.GraphQL?.Definitions?.getAlbumNameAndTracks||sp?.GraphQL?.Definitions?.getAlbum;
    if(sp?.GraphQL?.Request&&definition){
      let album;
      const items=await pages(async offset=>{
        const result=await sp.GraphQL.Request(definition,{uri,locale:sp.Locale?.getLocale?.()||'en',offset,limit:500});
        album=result?.data?.albumUnion;
        return {items:album?.tracks?.items,total:album?.tracks?.totalCount};
      });
      return {name:album?.name||document.querySelector('main h1')?.textContent||'Spotify album',
              tracks:items.map(x=>toInfo(x?.track||x)).filter(x=>x.uri&&x.title)};
    }
  }catch(e){console.warn('Soggfy album GraphQL M3U lookup failed',e);throw e;}
  return null;
}
function visibleTracks(){
  const tracks=(sgf.trackInfoFromRows?.()||[]).map(x=>({...x,durationMs:0}));
  return {name:document.querySelector('main h1')?.textContent||'Spotify',tracks};
}
sgf.generateM3U=async()=>{
  if(exporting)return;
  exporting=true;
  try{
  const collection=collectionFromLocation();
  let data=null;
  if(collection?.type==='playlist')data=await playlistTracks(collection.uri);
  else if(collection?.type==='album')data=await albumTracks(collection.uri);
  const visibleOnly=!data;
  if(!data)data=visibleTracks();
  if(!data.tracks.length){sgf.notify('No tracks found for M3U export',sgf.Icons.Warning);return;}

  sgf.notify('Checking downloaded tracks…');
  if(data.tracks.length>MAX_TRACKS)throw Error('M3U export supports up to 10,000 tracks');
  const infos=[...new Map(data.tracks.map(track=>[track.uri,track])).values()];
  const statuses=new Map();
  const batch=sgf.statusLimits?.batch||128;
  for(let i=0;i<infos.length;i+=batch){
    const page=infos.slice(i,i+batch),answer=await sgf.requestStatuses(page);
    // Missing replies mean an incomplete check, not an undownloaded track.
    if(page.some(track=>!answer.has(track.uri)))throw Error('Track check incomplete; please retry export');
    for(const [uri,status] of answer)statuses.set(uri,status);
  }
  const downloaded=[];
  for(const track of data.tracks){
    const status=statuses.get(track.uri);
    if(status?.status==='DONE'&&status.path)downloaded.push({track,path:status.path});
  }
  if(!downloaded.length){sgf.notify('No downloaded tracks found',sgf.Icons.Warning);return;}

  const name=clean(data.name||'Spotify').slice(0,200);
  const suggested=name+'.m3u8';
  const header=suggested+sgf.FS+name;
  const records=downloaded.map(({track,path})=>
    Math.round((track.durationMs||0)/1000)+sgf.FS+clean(track.artist)+sgf.FS+
    clean(track.title)+sgf.FS+clean(path)
  );
  const payload=[header,...records].join(sgf.RS);
  if(encodeURIComponent(payload).length>1000000)throw Error('Playlist paths are too large for one export');
  sgf.send('save_m3u',payload);
  sgf.notify('Exporting '+downloaded.length+' downloaded track'+(downloaded.length===1?'':'s')+
    (visibleOnly?' from visible rows only':''));
  }catch(e){sgf.notify(String(e.message||e),sgf.Icons.Warning);}
  finally{exporting=false;}
};
sgf.hasM3UContext=()=>!!collectionFromLocation();
})();
