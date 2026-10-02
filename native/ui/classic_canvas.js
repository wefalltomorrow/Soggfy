(() => {
'use strict';
const sgf=window.__SoggfyClassic;
if(!sgf||sgf.__canvasLoaded)return;
sgf.__canvasLoaded=true;
const seen=new Map();
const clean=value=>String(value??'').replace(/[\x1f\r\n]/g,' ').trim();
sgf.checkCanvas=()=>{
  if(!sgf.state.saveCanvas)return;
  const state=sgf.currentState?.(),item=state?.item;
  if(!item?.uri)return;
  const m=item.metadata||{};
  const url=m['canvas.url']||m.canvas_url||m.canvasUrl||item.canvas?.url||'';
  if(!/^https?:\/\//i.test(url))return;
  const key=item.uri+'\x1f'+url;
  const now=Date.now();
  if(seen.has(key)&&now-seen.get(key)<3600000)return;
  seen.set(key,now);
  if(seen.size>128){
    const oldest=[...seen.entries()].sort((a,b)=>a[1]-b[1]).slice(0,32);
    for(const [k] of oldest)seen.delete(k);
  }
  const title=clean(m.title||item.name||'Untitled');
  const artist=clean(m.album_artist_name||m.artist_name||item.artists?.[0]?.name||'Unknown Artist');
  const album=clean(m.album_title||item.album?.name||'Unknown Album');
  const track=String(Math.max(0,Number(m.album_track_number||item.trackNumber||0)||0));
  sgf.send('canvas',[url,title,artist,album,track].join(sgf.FS));
};
setInterval(()=>sgf.checkCanvas(),1500);
})();