(() => {
'use strict';
const sgf=window.__SoggfyClassic;
if(!sgf||sgf.__recoveryLoaded)return;
sgf.__recoveryLoaded=true;

// A playing Spotify session can stop producing decoder callbacks without
// emitting playback_stuck. RC49 silently disabled its watchdog whenever
// getState() omitted the exact isPaused/paused/isPlaying flags. This build
// honours those flags AND Spotify's visible Play/Pause controls, while never
// resuming an explicitly paused session.
sgf.createPlaybackRecoveryMonitor=(opts={})=>{
  let uri='',since=0,attempts=0,busy=false,finished=false,lastDiagnostic=-Infinity;
  const clock=opts.now||(()=>Date.now());
  const state=opts.getState||(()=>sgf.currentState?.());
  const reset=opts.reset||(()=>sgf.resetCurrentTrack?.(false));
  const skip=opts.skip||(()=>sgf.player?.skipToNext?.());
  const report=opts.report||((action,name,age)=>{
    sgf.send('playback_recovery',action+' title='+name+' elapsed_ms='+Math.round(age));
  });
  const allowed=opts.allowed||(()=>sgf.state.downloads&&sgf.state.speedSupported&&
    sgf.state.speedImmediate&&Number(sgf.state.playbackSpeed)>=5);
  const speed=opts.speed||(()=>Number(sgf.state.playbackSpeed)||1);
  const clear=()=>{uri='';since=0;attempts=0;busy=false;finished=false;};
  const durationMs=st=>{
    const value=Number(st?.duration||st?.durationMs||st?.item?.duration||
      st?.item?.metadata?.duration||st?.item?.metadata?.duration_ms||0);
    return Number.isFinite(value)&&value>0 ? (value>1000?value:value*1000):0;
  };
  const defaultPlayState=st=>{
    // Explicit pause always wins, including Spicetify's alternate state keys.
    if(st?.isPaused===true||st?.paused===true||st?.is_paused===true||
       st?.isPlaying===false||st?.is_playing===false||st?.playing===false||
       st?.playbackState==='paused'||st?.playbackState==='PAUSED')return 'paused';
    const dom=typeof document!=='undefined'?document:null;
    const hasPlay=!!dom?.querySelector?.('[data-testid="control-button-play"],button[aria-label="Play"],button[aria-label="Resume"]');
    const hasPause=!!dom?.querySelector?.('[data-testid="control-button-pause"],button[aria-label="Pause"],button[aria-label="Pause playback"]');
    if(hasPlay&&!hasPause)return 'paused';
    if(st?.isPaused===false||st?.paused===false||st?.is_paused===false||
       st?.isPlaying===true||st?.is_playing===true||st?.playing===true||
       st?.playbackState==='playing'||st?.playbackState==='PLAYING')return 'playing';
    if(hasPause)return 'playing';
    return 'unknown';
  };
  const playState=opts.getPlayState||defaultPlayState;
  const diagnostic=(now,reason,track,age)=>{
    if(!opts.diagnostic && !sgf.state?.debug)return;
    if(now-lastDiagnostic<20000)return;
    lastDiagnostic=now;
    report('diag_'+reason,track||'none',Math.max(0,age));
  };
  return {
    clear,
    async tick(){
      if(busy)return;
      const now=clock();
      if(!Number.isFinite(now))return;
      if(!allowed()){
        diagnostic(now,'disabled',uri,0);
        clear();
        return;
      }
      const st=state();
      const current=st?.item?.uri||st?.item?.contextTrack?.uri||'';
      if(!current){
        diagnostic(now,'missing_uri',uri,0);
        clear();
        return;
      }
      if(current!==uri){
        uri=current;since=now;attempts=0;finished=false;
        diagnostic(now,'armed',uri,0);
        return;
      }
      if(finished)return;
      const status=playState(st);
      if(status!=='playing'){
        // A manual pause must not consume the retry budget or trigger Next.
        diagnostic(now,status==='paused'?'paused':'unknown_state',uri,now-since);
        since=now;
        return;
      }
      const rate=Math.max(1,Math.min(50,Number(speed())||1));
      const length=durationMs(st);
      const deadline=Math.max(18000,(length>0?length/rate:0)+12000);
      const age=now-since;
      if(age<deadline){
        diagnostic(now,'playing',uri,age);
        return;
      }
      busy=true;
      try{
        if((state()?.item?.uri||'')!==current)return;
        if(playState(state())!=='playing')return;
        if(attempts<2){
          attempts++;
          report('retry_'+attempts,current,age);
          let ok=false;
          try{ok=(await reset())===true;}catch(e){console.warn('Soggfy recovery retry failed',e);}
          if(ok){since=clock();return;}
          attempts=2;
        }
        if((state()?.item?.uri||'')!==current)return;
        if(playState(state())!=='playing')return;
        report('skip_stalled',current,age);
        finished=true; // Never repeatedly skip a track that cannot recover.
        try{await skip();}catch(e){console.warn('Soggfy recovery skip failed',e);}
      }finally{busy=false;}
    },
    snapshot:()=>({uri,since,attempts,busy,finished})
  };
};

sgf.installPlaybackStallRecovery=()=>{
  if(sgf.__recoveryMonitor)return;
  sgf.__recoveryMonitor=sgf.createPlaybackRecoveryMonitor();
  if(sgf.state?.debug)sgf.send('playback_recovery','installed player='+
    Boolean(sgf.player)+' supported='+Boolean(sgf.state.speedSupported)+
    ' immediate='+Boolean(sgf.state.speedImmediate));
  setInterval(()=>{void sgf.__recoveryMonitor.tick();},1500);
};
})();
