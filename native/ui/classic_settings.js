(() => {
'use strict';
const sgf=window.__SoggfyClassic;
if(!sgf||sgf.__settingsLoaded)return;
sgf.__settingsLoaded=true;

const presets={
  'Original OGG / FLAC':{ext:'',args:'-c copy'},
  'MP3 320K':{ext:'mp3',args:'-c:a libmp3lame -b:a 320k -id3v2_version 3 -c:v copy'},
  'MP3 256K':{ext:'mp3',args:'-c:a libmp3lame -b:a 256k -id3v2_version 3 -c:v copy'},
  'MP3 192K':{ext:'mp3',args:'-c:a libmp3lame -b:a 192k -id3v2_version 3 -c:v copy'},
  'M4A 256K (FDK AAC)':{ext:'m4a',args:'-c:a libfdk_aac -b:a 256k -cutoff 20k -disposition:v attached_pic -c:v copy'},
  'M4A 224K VBR (FDK AAC)':{ext:'m4a',args:'-c:a libfdk_aac -vbr 5 -disposition:v attached_pic -c:v copy'},
  'M4A 160K (FDK AAC)':{ext:'m4a',args:'-c:a libfdk_aac -b:a 160k -cutoff 18k -disposition:v attached_pic -c:v copy'},
  'Opus 160K':{ext:'opus',args:'-c:a libopus -b:a 160k'},
  'Custom':null
};
const variableDescriptions={
  track_name:'Track name / episode name',
  artist_name:'Album artist / primary artist',
  all_artist_names:'All contributing artists',
  album_name:'Album / podcast name',
  track_num:'Track number',
  track_num_2:'Two-digit track number',
  disc_num:'Disc number',
  release_year:'Release year',
  release_date:'Release date',
  multi_disc_path:'\\CD N when an album has multiple discs',
  multi_disc_paren:' (CD N) when an album has multiple discs',
  ext:'Current output extension'
};

function register(key,node){sgf.controls.set(key,node);return node;}
function sendState(key,value){
  sgf.state[key]=value;
  sgf.send(key,value);
  sgf.refreshControls?.();
  return value;
}
function toggle(key,nativeKey=key){
  const n=document.createElement('input');
  n.type='checkbox';n.className='sgf-toggle-switch';n.checked=!!sgf.state[key];
  n.onchange=()=>{
    sgf.state[key]=n.checked;
    sgf.send(nativeKey,n.checked?'1':'0');
    sgf.refreshControls?.();
  };
  register(key,n);return n;
}
function select(key,options,onchange){
  const n=document.createElement('select');n.className='sgf-select';
  for(const [label,value] of Object.entries(options)){
    const o=document.createElement('option');o.textContent=label;o.value=String(value);n.appendChild(o);
  }
  n.value=String(sgf.state[key]??'');
  n.onchange=()=>onchange?onchange(n.value):sendState(key,n.value);
  register(key,n);return n;
}
function textInput(key,nativeKey=key){
  const n=document.createElement('input');n.type='text';n.className='sgf-text-input';
  n.value=sgf.state[key]??'';n.spellcheck=false;
  n.onchange=()=>sendState(nativeKey,n.value);
  register(key,n);return n;
}
function slider(key,nativeKey=key,min=1,max=sgf.MAX_PLAYBACK_SPEED,step=1){
  const wrap=document.createElement('div');wrap.className='sgf-slider-wrapper';
  const label=document.createElement('input');label.className='sgf-slider-label';
  const range=document.createElement('input');range.className='sgf-slider';range.type='range';
  range.min=String(min);range.max=String(max);range.step=String(step);
  const set=v=>{v=Math.max(min,Math.min(max,Number(v)||min));range.value=String(v);label.value=v+'x';};
  set(sgf.state[key]);
  range.oninput=()=>label.value=range.value+'x';
  range.onchange=async()=>{
    const v=Number(range.value);sgf.state[key]=v;
    await sgf.setPlaybackSpeed?.(v);
  };
  label.onfocus=()=>label.value=String(sgf.state[key]);
  label.onblur=()=>label.value=(sgf.state[key]||1)+'x';
  label.onchange=async()=>{
    const v=Math.max(min,Math.min(max,Number.parseFloat(label.value)||1));set(v);
    sgf.state[key]=v;await sgf.setPlaybackSpeed?.(v);
  };
  register(key,range);wrap.append(label,range);return wrap;
}
function row(label,action){
  const n=document.createElement('div');n.className='sgf-setting-row';
  const d=document.createElement('label');d.className='col description';d.textContent=label;
  const a=document.createElement('div');a.className='col action';a.appendChild(action);
  n.append(d,a);return n;
}
function readOnly(key){
  const n=document.createElement('span');n.className='sgf-readonly-value';
  n.textContent=sgf.state[key]||'Unavailable';
  register(key,n);return n;
}
function rows(label,action){
  const n=document.createElement('div');n.className='sgf-setting-rows';
  if(label){const l=document.createElement('label');l.textContent=label;n.appendChild(l);}
  if(action)n.appendChild(action);return n;
}
function section(title,...children){
  const n=document.createElement('div');n.className='sgf-setting-section';
  const h=document.createElement('h2');h.textContent=title;n.appendChild(h);n.append(...children);return n;
}
function colSection(...children){const n=document.createElement('div');n.className='sgf-setting-cols';n.append(...children);return n;}
function button(icon,text,cb){
  const n=document.createElement('button');n.className='sgf-button';
  if(icon)n.appendChild(sgf.parseHtml(icon));if(text)n.appendChild(document.createTextNode(text));n.onclick=cb;return n;
}
function variableTags(){
  const details=document.createElement('details');details.className='sgf-collapsible';
  const summary=document.createElement('summary');summary.textContent='Variables';details.appendChild(summary);
  for(const [name,desc] of Object.entries(variableDescriptions)){
    const b=document.createElement('button');b.className='sgf-tag-button';b.textContent='{'+name+'}';b.title=desc;
    b.onclick=async()=>{
      try{
        if(sgf.platform?.getClipboardAPI)sgf.platform.getClipboardAPI().copy('{'+name+'}');
        else await navigator.clipboard.writeText('{'+name+'}');
        sgf.notify('Copied');
      }catch{}
    };
    details.appendChild(b);
  }
  return details;
}
function makeOverlay(){
  const overlay=document.createElement('div');
  overlay.className='sgf-settings-overlay';
  overlay.innerHTML='<div class="sgf-settings-modal" tabindex="-1" role="dialog" aria-modal="true" aria-label="Soggfy settings"><div class="sgf-settings-container"><div class="sgf-settings-header"><h1 class="sgf-header-title">Soggfy settings</h1><button type="button" aria-label="Close" class="sgf-settings-closeBtn"><svg width="18" height="18" viewBox="0 0 32 32"><path d="M31.098 29.794 16.955 15.65 31.097 1.51 29.683.093 15.54 14.237 1.4.094-.016 1.508 14.126 15.65-.016 29.795l1.414 1.414L15.54 17.065l14.144 14.143" fill="currentColor"/></svg></button></div><div class="sgf-settings-elements"></div></div></div>';
  const modal=overlay.querySelector('.sgf-settings-modal');
  const container=overlay.querySelector('.sgf-settings-container');
  const close=()=>overlay.remove();
  overlay.querySelector('.sgf-settings-closeBtn').addEventListener('click',e=>{
    e.preventDefault();
    e.stopPropagation();
    close();
  },true);
  let outside=false;
  overlay.addEventListener('mousedown',e=>{outside=!container.contains(e.target);},true);
  overlay.addEventListener('mouseup',e=>{
    if(outside&&!container.contains(e.target))close();
    outside=false;
  },true);
  overlay.addEventListener('keydown',e=>{if(e.key==='Escape')close();},true);
  return {overlay,modal,body:overlay.querySelector('.sgf-settings-elements')};
}

sgf.openSettings=()=>{
  const existing=document.querySelector('.sgf-settings-overlay');
  if(existing){
    existing.querySelector('.sgf-settings-modal')?.focus();
    return;
  }
  const {overlay,modal,body}=makeOverlay();

  const formatOptions=Object.fromEntries(Object.keys(presets).map(x=>[x,x]));
  if(sgf.state.outputPreset==='Native'||sgf.state.outputPreset==='Native Spotify format')sgf.state.outputPreset='Original OGG / FLAC';
  if(!formatOptions[sgf.state.outputPreset])sgf.state.outputPreset='Custom';
  const format=select('outputPreset',formatOptions,value=>{
    sgf.state.outputPreset=value;sgf.send('outputPreset',value);
    const preset=presets[value];
    if(preset){
      sgf.state.outputExt=preset.ext;sgf.state.outputArgs=preset.args;
      sgf.send('outputExt',preset.ext);sgf.send('outputArgs',preset.args);
    }
    if(custom)custom.style.display=value==='Custom'?'block':'none';
  });
  const custom=document.createElement('div');custom.className='sgf-subsection';
  custom.append(
    rows('FFmpeg arguments',textInput('outputArgs')),
    row('Extension',select('outputExt',{MP3:'mp3',M4A:'m4a',MP4:'mp4',OGG:'ogg',Opus:'opus',FLAC:'flac'}))
  );
  custom.style.display=sgf.state.outputPreset==='Custom'?'block':'none';

  const canvasRow=rows('Canvas template',textInput('canvasTemplate'));
  canvasRow.style.display=sgf.state.saveCanvas?'flex':'none';
  const saveCanvas=toggle('saveCanvas');
  saveCanvas.onchange=()=>{
    sgf.state.saveCanvas=saveCanvas.checked;
    sgf.send('saveCanvas',saveCanvas.checked?'1':'0');
    canvasRow.style.display=saveCanvas.checked?'flex':'none';
  };

  const general=section('General',
    row('Playback speed',(()=>{const n=slider('playbackSpeed');const supported=!!sgf.state.speedSupported;n.title=supported?'Native accelerated playback':'Disabled on this Spotify build because the native player ABI is not validated';if(!supported)n.querySelectorAll('input').forEach(input=>{input.disabled=true;});return n;})()),
    row('Output format',format),
    custom,
    row('Skip downloaded tracks',toggle('skipDownloaded')),
    row('Skip ignored tracks',toggle('skipIgnored')),
    row('Embed cover art',toggle('embedCover')),
    row('Save cover art in album folder',toggle('saveCover')),
    row('Embed lyrics',toggle('embedLyrics')),
    row('Save lyrics as .lrc/.txt',toggle('saveLyrics')),
    row('Save canvas',saveCanvas)
  );

  const base=textInput('root');
  base.onchange=()=>sgf.send('root',base.value);
  const browse=button(sgf.Icons.Folder,null,()=>{
    sgf.send('browse','1');
    setTimeout(()=>sgf.send('sync','1'),1000);
    setTimeout(()=>sgf.send('sync','1'),3000);
  });
  const invalid=select('invalidChars',{'Unicodes':'unicode','Dashes (-)':'-','Underlines (_)':'_','None (remove)':''});
  const paths=section('Paths',
    rows('Base path',colSection(base,browse)),
    rows('Track template',textInput('template')),
    rows('Podcast template',textInput('podcastTemplate')),
    canvasRow,
    row('Replace invalid characters with',invalid),
    rows('',variableTags())
  );

  const modern=section('Modern capture',
    row('Current song',readOnly('qualitySong')),
    row('Spotify quality',readOnly('qualityLevel')),
    row('Current format',readOnly('qualityFormat')),
    row('Sample rate / depth',readOnly('qualitySample')),
    row('Capture native FLAC',toggle('flac')),
    row('Capture Ogg/Vorbis',toggle('ogg')),
    row('Cached metadata enrichment',toggle('metadata')),
    row('Keep native original after conversion',toggle('keepNative')),
    rows('',(()=>{const n=document.createElement('div');n.className='sgf-modern-note';n.textContent='Native capture stays memory-only until a complete listen is validated. Conversion runs only after the validated native file is published.';return n;})())
  );

  const misc=section('Misc',
    row('Block telemetry',toggle('blockTelemetry')),
    row("Move 'Add to Queue' to top",toggle('liftQueue')),
    row('Activity log',toggle('log')),
    row('Debug log',toggle('debug')),
    rows('FFmpeg path',textInput('ffmpegPath'))
  );

  body.append(general,paths,modern,misc);
  document.body.appendChild(overlay);
  modal?.focus();
  sgf.refreshControls?.();
  sgf.send('sync','1');
  const qualityTimer=setInterval(()=>{
    if(!overlay.isConnected){clearInterval(qualityTimer);return;}
    sgf.send('sync','1');
  },1000);
};

})();