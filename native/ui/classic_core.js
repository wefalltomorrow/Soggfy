(() => {
'use strict';
if (window.__SoggfyClassicCoreLoaded) return;
window.__SoggfyClassicCoreLoaded = true;

const sgf = window.__SoggfyClassic = window.__SoggfyClassic || {};
sgf.PREFIX = 'SOGGFY_UI_V1:';
sgf.RS = String.fromCharCode(0x1e);
sgf.FS = String.fromCharCode(0x1f);
sgf.bootFns = sgf.bootFns || [];
sgf.controls = new Map();
sgf.statusMap = new Map();
sgf.state = Object.assign({
  downloads:false, ogg:true, flac:true, metadata:true, log:true, debug:false, normalize:true,
  skipDownloaded:false, skipIgnored:false, embedCover:true, saveCover:true, embedLyrics:true,
  saveLyrics:true, saveCanvas:false, blockTelemetry:true, liftQueue:false, keepNative:true,
  playbackSpeed:1, speedSupported:false,
  qualitySong:'Unavailable', qualityLevel:'Unavailable', qualityFormat:'Unavailable', qualitySample:'Unavailable',
  root:'', template:'', podcastTemplate:'', canvasTemplate:'',
  invalidChars:'unicode', outputPreset:'Native', outputExt:'', outputArgs:'', ffmpegPath:''
}, sgf.state || {});

sgf.Icons = {
  Folder:'<svg width="24px" height="24px" viewBox="0 0 24 24" fill="currentColor"><path d="M10 4H4c-1.1 0-1.99.9-1.99 2L2 18c0 1.1.9 2 2 2h16c1.1 0 2-.9 2-2V8c0-1.1-.9-2-2-2h-8l-2-2z"></path></svg>',
  Sliders:'<svg width="24" height="24" viewBox="0 0 24 24" fill="currentColor"><path d="M3 17v2h6v-2H3zM3 5v2h10V5H3zm10 16v-2h8v-2h-8v-2h-2v6h2zM7 9v2H3v2h4v2h2V9H7zm14 4v-2H11v2h10zm-6-4h2V7h4V5h-4V3h-2v6z"></path></svg>',
  Done:'<svg width="16" height="16" viewBox="0 0 16 16" fill="#3f3"><path d="M13.985 2.383L5.127 12.754 1.388 8.375l-.658.77 4.397 5.149 9.618-11.262z"/></svg>',
  Error:'<svg width="16" height="16" viewBox="0 0 16 16" fill="#f22"><path d="M14.354 2.353l-.708-.707L8 7.293 2.353 1.646l-.707.707L7.293 8l-5.647 5.646.707.708L8 8.707l5.646 5.647.708-.708L8.707 8z"/></svg>',
  InProgress:'<svg width="18" height="18" viewBox="0 0 24 24" fill="#29f"><path d="M18.32,4.26C16.84,3.05,15.01,2.25,13,2.05v2.02c1.46,0.18,2.79,0.76,3.9,1.62L18.32,4.26z M19.93,11h2.02c-.2-2.01-1-3.84-2.21-5.32L18.31,7.1C19.17,8.21,19.75,9.54,19.93,11z M18.31,16.9l1.43,1.43c1.21-1.48,2.01-3.32,2.21-5.32h-2.02c-.18,1.45-.76,2.78-1.62,3.89z M13,19.93v2.02c2.01-.2,3.84-1,5.32-2.21l-1.43-1.43c-1.1,.86-2.43,1.44-3.89,1.62z M15.59,10.59L13,13.17V7h-2v6.17l-2.59-2.59L7,12l5,5l5-5-1.41-1.41z M11,19.93v2.02C5.95,21.45,2,17.19,2,12S5.95,2.55,11,2.05v2.02C7.05,4.56,4,7.92,4,12s3.05,7.44,7,7.93z"/></svg>',
  Processing:'<svg width="18" height="18" viewBox="0 0 24 24" fill="#ddd"><path d="M12 4V1L8 5l4 4V6c3.31 0 6 2.69 6 6 0 1.01-.25 1.97-.7 2.8l1.46 1.46C19.54 15.03 20 13.57 20 12c0-4.42-3.58-8-8-8zm0 14c-3.31 0-6-2.69-6-6 0-1.01.25-1.97.7-2.8L5.24 7.74C4.46 8.97 4 10.43 4 12c0 4.42 3.58 8 8 8v3l4-4-4-4v3z"><animateTransform attributeName="transform" attributeType="XML" type="rotate" from="360 12 12" to="0 12 12" dur="3s" repeatCount="indefinite"/></path></svg>',
  Warning:'<svg width="18" height="18" viewBox="0 0 24 24" fill="#fbd935"><path d="M1 21h22L12 2 1 21zm12-3h-2v-2h2v2zm0-4h-2v-4h2v4z"/></svg>',
  SyncDisabled:'<svg height="20" width="20" viewBox="0 0 24 25" fill="#bbb"><path d="m19.8 22.6-3.725-3.725q-.475.275-.987.5-.513.225-1.088.375v-2.1q.15-.05.3-.112.15-.063.3-.138l-8-8q-.275.625-.438 1.288Q6 11.35 6 12.05q0 1.125.425 2.187Q6.85 15.3 7.75 16.2l.25.25V14h2v6H4v-2h2.75l-.4-.35q-1.225-1.225-1.788-2.662Q4 13.55 4 12.05q0-1.125.287-2.163.288-1.037.838-1.962L1.4 4.2l1.425-1.425 18.4 18.4Z"/></svg>',
  FileDownload:'<svg viewBox="0 0 24 24" width="24" height="24" fill="currentColor"><path d="M18,15v3H6v-3H4v3c0,1.1,0.9,2,2,2h12c1.1,0,2-0.9,2-2v-3H18z M17,11l-1.41-1.41L13,12.17V4h-2v8.17L8.41,9.59L7,11l5,5 L17,11z"></path></svg>',
  FileDownloadOff:'<svg viewBox="0 0 24 24" width="24" height="24" fill="currentColor"><path d="M16 18 17.15 20H6Q5.175 20 4.588 19.413 4 18.825 4 18V15H6V18M12.575 15.425 12 16 7 11 7.575 10.425ZM15.6 9.55 17 11 15.425 12.575 14 11.15ZM13 4V10.15L11 8.15V4Z"/><path d="M2.8 2.8 21.2 21.2 19.775 22.625 1.375 4.225Z" fill="#e91429"/></svg>',
  SaveAs:'<svg viewBox="0 0 24 24"><path d="M5 3h11l3 3v15H5V3zm2 2v14h10V7.5L14.5 5H7zm1 1h6v5H8V6zm1 1v3h4V7H9zm-1 6h8v5H8v-5z"></path></svg>',
  Block:'<svg viewBox="0 0 24 24"><path d="M12 2a10 10 0 1 0 0 20 10 10 0 0 0 0-20zm0 2c1.85 0 3.55.63 4.9 1.69L5.69 16.9A8 8 0 0 1 12 4zm0 16a7.96 7.96 0 0 1-4.9-1.69L18.31 7.1A8 8 0 0 1 12 20z"></path></svg>'
};

const css = [
':root{--sgf-green:#1db954;--sgf-active:#444;--sgf-inactive:#555;--sgf-hover:#666;--sgf-container-background:#333;--sgf-text:#ddd;--sgf-text-active:#fff}',
'.sgf-toggle-switch{appearance:none;display:inline-block;position:relative;width:42px;height:24px;border-radius:24px;background-color:var(--sgf-inactive);transition:all .1s ease;cursor:pointer}',
'.sgf-toggle-switch:after{content:"";position:absolute;top:2px;left:2px;width:20px;height:20px;border-radius:50%;background:#fff;box-shadow:0 0 4px rgb(0 0 0 / 20%);transition:all .1s ease}.sgf-toggle-switch:checked{background-color:var(--sgf-green)}.sgf-toggle-switch:checked:after{transform:translatex(18px)}',
'.sgf-select{width:100%;border-radius:4px;border:0;background-color:var(--sgf-inactive);color:var(--sgf-text);font-family:arial;font-size:14px;font-weight:400;height:32px;line-height:20px;padding:0 32px 0 12px;cursor:pointer}',
'.sgf-text-input{width:100%;height:32px;padding:0 5px;border-radius:3px;border:0;background-color:var(--sgf-inactive);color:var(--sgf-text);font-family:arial;font-size:14px;font-weight:400;box-sizing:border-box}',
'.sgf-slider-wrapper{display:flex;flex-direction:row;align-items:center}.sgf-slider{appearance:none;position:relative;width:100%;height:8px;border-radius:4px;background:var(--sgf-inactive);outline:none;cursor:e-resize}.sgf-slider::-webkit-slider-thumb{appearance:none;width:16px;height:16px;border-radius:50%;background:var(--sgf-green)}',
'.sgf-slider-label{width:64px;margin-right:4px;text-align:right;font-size:11px;background:var(--sgf-container-background);border:none;color:var(--sgf-text)}',
'.sgf-collapsible{padding:4px 4px 0;background:var(--sgf-active);border-radius:4px;overflow:hidden;max-height:33px;transition:max-height .5s ease}.sgf-collapsible[open]{padding:4px;border-bottom:0;max-height:30rem}.sgf-collapsible>summary{border-radius:4px;margin:-4px -4px 0;padding:4px;list-style:none;background-color:var(--sgf-inactive);color:var(--sgf-text);cursor:pointer}.sgf-collapsible[open]>summary{margin-bottom:4px}',
'.sgf-tag-button{border-radius:4px;border:0;background-color:var(--sgf-inactive);color:var(--sgf-text);font-family:monospace;font-size:11px;cursor:pointer;padding:4px;margin:2px}.sgf-tag-button:hover{background-color:var(--sgf-hover)}',
'.sgf-button{display:flex;justify-content:center;align-items:center;height:32px;border:none;border-radius:4px;background-color:var(--sgf-inactive);color:var(--sgf-text);gap:4px;padding:2px 8px;font-size:14px;cursor:pointer}',
'.sgf-topbar-retractor{display:block;width:32px;height:32px;margin-right:8px;border-radius:32px;overflow:hidden;transition:all .15s ease}.sgf-topbar-retractor:hover{display:flex;width:68px;background:rgba(0,0,0,.25);gap:4px}.sgf-topbar-retractor button{margin-right:0!important;flex-shrink:0}',
'.sgf-select,.sgf-text-input,.sgf-button{transition:background-color .1s ease-in-out}.sgf-select:hover,.sgf-text-input:hover,.sgf-button:hover{background-color:var(--sgf-hover)}',
'.sgf-settings-overlay{display:flex;align-items:center;justify-content:center;background-color:rgba(0,0,0,.7);position:fixed;width:100%;height:100%;inset:0;overflow:hidden;z-index:99999;user-select:none}.sgf-settings-modal{width:40rem;height:90%;display:block}.sgf-settings-container{background-color:#333;border-radius:10px;box-shadow:0 0 8px 4px rgb(0 0 0 / 15%);height:inherit;max-height:45rem;display:flex;flex-direction:column;position:relative;top:50%;transform:translateY(-50%)}',
'.sgf-settings-header{display:flex;align-items:baseline;border-bottom:1px solid rgba(255,255,255,.1);justify-content:space-between;padding:32px 32px 12px}.sgf-header-title{font-size:32px;font-weight:700;letter-spacing:-.04em;line-height:36px;text-transform:none}.sgf-settings-closeBtn{background-color:transparent;border:0;padding:8px;color:var(--sgf-text);cursor:pointer}.sgf-settings-closeBtn:hover{transform:scale(1.1)}.sgf-settings-elements{overflow:auto;padding:16px 32px}',
'.sgf-setting-row{display:flex;align-items:center;flex-direction:row;margin:4px 0;min-height:37px}.sgf-setting-row .col.description{float:left;padding-right:15px;cursor:default;flex:1}.sgf-setting-row .col.action{float:right;text-align:right;min-width:180px}.sgf-setting-rows{display:flex;flex-direction:column;margin:4px 0}.sgf-setting-cols{display:flex;flex-direction:row;align-items:center;gap:4px}.sgf-setting-section{margin:12px 0 22px}.sgf-setting-section h2{margin:0 0 8px}.sgf-subsection{margin-left:20px}',
'.sgf-status-indicator{background:transparent;border:0;display:flex;position:relative}.sgf-status-indicator-card{display:flex;flex-direction:column;position:absolute;background:#222;border-radius:4px;top:-18px;padding:4px;transform:translateX(calc(-50% + 8px));box-shadow:2px 2px 6px 4px rgb(0 0 0 / 25%);opacity:0;transition:opacity .1s ease-out .5s;z-index:999;max-width:260px;width:max-content}.sgf-status-indicator:hover .sgf-status-indicator-card{opacity:1}.sgf-status-browse-button{background:transparent;border:0;height:24px;display:flex;cursor:pointer;align-items:center}',
'.sgf-notification-bubble{display:flex;position:fixed;z-index:100000;background:#222;padding:6px 10px;border-radius:4px;pointer-events:none;box-shadow:1px 1px 4px rgb(0 0 0 / 30%);left:50%;bottom:105px;transform:translateX(-50%);animation:sgf-fade-out .2s ease var(--delay,2.5s) forwards}.sgf-notification-wrapper{display:flex;align-items:center;gap:8px;font-size:16px;font-weight:500}@keyframes sgf-fade-out{from{opacity:1}to{opacity:0}}',
'.sgf-modern-note{font-size:11px;color:#bbb;line-height:16px;margin:4px 0 10px}',`.sgf-readonly-value{font-size:12px;color:var(--sgf-text);white-space:nowrap;overflow:hidden;text-overflow:ellipsis;max-width:280px;display:inline-block;vertical-align:middle}`
].join('');

if (!document.getElementById('soggfy-classic-style')) {
  const style=document.createElement('style');
  style.id='soggfy-classic-style';
  style.textContent=css;
  document.head.appendChild(style);
}

sgf.send = (key,value='') => {
  try { console.info(sgf.PREFIX + key + '=' + encodeURIComponent(String(value))); } catch {}
};

sgf.parseHtml = html => {
  const t=document.createElement('template');
  t.innerHTML=String(html||'').trim();
  return t.content.firstElementChild;
};

sgf.escapeHtml = value => {
  const d=document.createElement('div');
  d.textContent=String(value??'');
  return d.innerHTML;
};

sgf.notify = (text,icon='') => {
  const n=document.createElement('span');
  n.className='sgf-notification-bubble';
  n.style.setProperty('--delay','2.5s');
  const w=document.createElement('div');
  w.className='sgf-notification-wrapper';
  if(icon)w.appendChild(sgf.parseHtml(icon));
  const t=document.createElement('span');t.textContent=text;w.appendChild(t);
  n.appendChild(w);document.body.appendChild(n);
  setTimeout(()=>n.remove(),2900);
};

sgf.flag = (p,key,fallback) => {
  const v=p.get(key);
  return v===null?fallback:v==='1';
};

sgf.applyConfig = payload => {
  try {
    const p=new URLSearchParams(payload||'');
    const bools=['downloads','ogg','flac','metadata','log','debug','normalize','skipDownloaded','skipIgnored','embedCover','saveCover','embedLyrics','saveLyrics','saveCanvas','blockTelemetry','liftQueue','keepNative'];
    for(const key of bools)sgf.state[key]=sgf.flag(p,key,sgf.state[key]);
    const strings=['root','template','podcastTemplate','canvasTemplate','invalidChars','outputPreset','outputExt','outputArgs','ffmpegPath',
      'qualitySong','qualityLevel','qualityFormat','qualitySample'];
    for(const key of strings)if(p.has(key))sgf.state[key]=p.get(key)||'';
    if(p.has('playbackSpeed'))sgf.state.playbackSpeed=Math.max(1,Math.min(50,Number(p.get('playbackSpeed'))||1));
    if(p.has('speedSupported'))sgf.state.speedSupported=sgf.flag(p,'speedSupported',sgf.state.speedSupported);
    if(sgf.refreshControls)sgf.refreshControls();
  } catch {}
};
window.__soggfyApplyConfig=sgf.applyConfig;
if(window.__soggfyNativeConfig)sgf.applyConfig(window.__soggfyNativeConfig);

sgf.getPlatform = async () => {
  if(window.Spicetify?.Platform)return window.Spicetify.Platform;
  for(let attempt=0;attempt<200;attempt++){
    const main=document.querySelector('#main');
    if(main){
      const key=Object.keys(main).find(k=>k.startsWith('__reactContainer$'));
      let node=key?main[key]:null;
      const seen=new Set();
      for(let depth=0;node&&depth<60;depth++){
        if(seen.has(node))break;seen.add(node);
        const candidates=[
          node?.stateNode?.props?.platform,
          node?.memoizedProps?.platform,
          node?.pendingProps?.platform,
          node?.stateNode?.props?.children?.props?.children?.props?.platform
        ];
        for(const candidate of candidates)if(candidate?.getPlayerAPI)return candidate;
        node=node.child||node.sibling||node.return;
      }
    }
    await new Promise(resolve=>setTimeout(resolve,50));
  }
  return null;
};

sgf.currentState = () => {
  try{return sgf.player?.getState?.()||window.Spicetify?.Player?.data||null;}catch{return null;}
};

sgf.resetCurrentTrack = async preserve => {
  try{
    const st=sgf.currentState();
    if(!st?.item?.uri||!sgf.player)return false;
    const position=Math.max(0,(Date.now()-(st.timestamp||Date.now()))*(st.speed||1)+(st.positionAsOfTimestamp||st.position||0));
    const queue=sgf.player._queue;
    const queued=queue?.getQueue?.()?.queued||[];
    if(queue?.insertIntoQueue&&sgf.player.skipToNext){
      const tracks=[{uri:st.item.uri}];
      if(queued.length)await queue.insertIntoQueue(tracks,{before:queued[0]});else await queue.addToQueue(tracks);
      await sgf.player.skipToNext();
      if(preserve&&sgf.player.seekTo)await sgf.player.seekTo(position);
      return true;
    }
  }catch(e){console.warn('Soggfy resetCurrentTrack failed',e);}
  return false;
};

sgf.setPlaybackSpeed = async speed => {
  speed=Math.max(1,Math.min(50,Number(speed)||1));
  sgf.state.playbackSpeed=speed;
  sgf.send('playbackSpeed',String(speed));
  if(speed===1||sgf.state.speedSupported){
    // The native x64 hook reads Playback Speed when Spotify constructs the
    // track player. Re-create the current track so a change applies now.
    const reset=await sgf.resetCurrentTrack(!sgf.state.downloads);
    if(!reset&&speed!==1)sgf.notify('Playback speed will apply on the next local track',sgf.Icons.Warning);
    return true;
  }
  sgf.notify('Accelerated playback is unavailable on this Spotify build',sgf.Icons.Warning);
  return false;
};

sgf.findTopbarHost = () => {
  const fwd=document.querySelector("[data-testid='top-bar-forward-button'],.main-topBar-forward,.main-topBar-responsiveForward");
  return fwd?.parentElement||document.querySelector('[data-testid="topbar-content-wrapper"],.main-topBar-topbarContent,header');
};

sgf.mountTopbar = existing => {
  const host=sgf.findTopbarHost();
  if(!host)return null;
  if(existing){host.appendChild(existing);return existing;}
  const old=document.getElementById('soggfy-classic-topbar');
  if(old){old.remove();}
  const fwd=document.querySelector("[data-testid='top-bar-forward-button'],.main-topBar-forward,.main-topBar-responsiveForward");
  const buttonClass=fwd?.classList?.[0]||'sgf-native-button';
  const div=document.createElement('div');
  div.id='soggfy-classic-topbar';
  div.className='sgf-topbar-retractor';
  const download=document.createElement('button');
  download.className=buttonClass;
  download.innerHTML=sgf.state.downloads?sgf.Icons.FileDownload:sgf.Icons.FileDownloadOff;
  download.onclick=async()=>{
    sgf.state.downloads=!sgf.state.downloads;
    sgf.send('downloads',sgf.state.downloads?'1':'0');
    download.innerHTML=sgf.state.downloads?sgf.Icons.FileDownload:sgf.Icons.FileDownloadOff;
    if(sgf.state.downloads||sgf.state.playbackSpeed!==1)await sgf.resetCurrentTrack(!sgf.state.downloads);
    sgf.notify(sgf.state.downloads?'Soggfy downloads enabled':'Soggfy downloads disabled');
  };
  const settings=document.createElement('button');
  settings.className=buttonClass;
  settings.innerHTML=sgf.Icons.Sliders;
  settings.onclick=()=>{if(sgf.openSettings)sgf.openSettings();};
  div.append(download,settings);
  host.appendChild(div);
  sgf.topbar=div;
  return div;
};

sgf.refreshControls = () => {
  for(const [key,node] of sgf.controls){
    if(!node?.isConnected)continue;
    if(node.type==='checkbox')node.checked=!!sgf.state[key];
    else if(node.tagName==='SELECT')node.value=sgf.state[key]??node.value;
    else if(node.type==='range'&&key in sgf.state)node.value=String(sgf.state[key]??node.value);
    else if(node.type==='text'&&key in sgf.state)node.value=sgf.state[key]??'';
    else if(node.classList?.contains('sgf-readonly-value')&&key in sgf.state)node.textContent=sgf.state[key]||'Unavailable';
  }
  if(sgf.topbar?.isConnected&&sgf.topbar.children[0])
    sgf.topbar.children[0].innerHTML=sgf.state.downloads?sgf.Icons.FileDownload:sgf.Icons.FileDownloadOff;
};

sgf.initPlayer = async () => {
  sgf.platform=await sgf.getPlatform();
  if(!sgf.platform)return;
  try{sgf.player=sgf.platform.getPlayerAPI();}catch{}
  try{
    const settings=sgf.platform.getSettingsAPI?.();
    settings?.quality?.streamingQuality?.setValue?.(4);
    settings?.quality?.autoAdjustQuality?.setValue?.(false);
  }catch{}
  if(sgf.installPlayerListeners)sgf.installPlayerListeners();
};

})();