(() => {
'use strict';
const sgf=window.__SoggfyClassic;
if(!sgf||sgf.__m3uLoaded)return;
sgf.__m3uLoaded=true;

const clean=value=>String(value??'').replace(/[\x1e\x1f\r\n]/g,' ').trim();
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
    const meta=await api.getMetadata?.(uri);
    const result=await api.getContents(uri,{limit:100000,offset:0});
    const items=result?.items||result?.tracks?.items||[];
    return {name:meta?.name||document.querySelector('main h1')?.textContent||'Spotify playlist',
            tracks:items.map(toInfo).filter(x=>x.uri&&x.title)};
  }catch(e){console.warn('Soggfy playlist M3U lookup failed',e);return null;}
}
async function albumTracks(uri){
  try{
    const sp=window.Spicetify;
    const definition=sp?.GraphQL?.Definitions?.getAlbumNameAndTracks||sp?.GraphQL?.Definitions?.getAlbum;
    if(sp?.GraphQL?.Request&&definition){
      const result=await sp.GraphQL.Request(definition,{uri,locale:sp.Locale?.getLocale?.()||'en',offset:0,limit:5000});
      const album=result?.data?.albumUnion;
      const items=album?.tracks?.items||[];
      return {name:album?.name||document.querySelector('main h1')?.textContent||'Spotify album',
              tracks:items.map(x=>toInfo(x?.track||x)).filter(x=>x.uri&&x.title)};
    }
  }catch(e){console.warn('Soggfy album GraphQL M3U lookup failed',e);}
  return null;
}
function visibleTracks(){
  const tracks=(sgf.trackInfoFromRows?.()||[]).map(x=>({...x,durationMs:0}));
  return {name:document.querySelector('main h1')?.textContent||'Spotify',tracks};
}
sgf.generateM3U=async()=>{
  const collection=collectionFromLocation();
  let data=null;
  if(collection?.type==='playlist')data=await playlistTracks(collection.uri);
  else if(collection?.type==='album')data=await albumTracks(collection.uri);
  if(!data||!data.tracks.length)data=visibleTracks();
  if(!data.tracks.length){sgf.notify('No tracks found for M3U export',sgf.Icons.Warning);return;}

  sgf.notify('Checking downloaded tracks…');
  const statuses=await sgf.requestStatuses(data.tracks);
  const downloaded=[];
  for(const track of data.tracks){
    const status=statuses.get(track.uri);
    if(status?.status==='DONE'&&status.path)downloaded.push({track,path:status.path});
  }
  if(!downloaded.length){sgf.notify('No downloaded tracks found',sgf.Icons.Warning);return;}

  const suggested=clean(data.name||'Spotify')+'.m3u8';
  const header=clean(suggested)+sgf.FS+clean(data.name||'Spotify');
  const records=downloaded.map(({track,path})=>
    Math.round((track.durationMs||0)/1000)+sgf.FS+clean(track.artist)+sgf.FS+
    clean(track.title)+sgf.FS+clean(path)
  );
  sgf.send('save_m3u',[header,...records].join(sgf.RS));
  sgf.notify('Exporting '+downloaded.length+' downloaded track'+(downloaded.length===1?'':'s'));
};
sgf.hasM3UContext=()=>!!collectionFromLocation();
})();