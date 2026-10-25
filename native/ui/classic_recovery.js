(() => {
'use strict';
const sgf=window.__SoggfyClassic;
if(!sgf||sgf.__recoveryLoaded)return;
sgf.__recoveryLoaded=true;

// Spotify sometimes remains in a "playing" state without advancing to the
// next track after accelerated playback. It does not necessarily emit the
// playback_stuck error that original Soggfy listens for. Retry only after a
// full-track wall-clock deadline plus a generous buffering allowance.
//
// This watchdog does NOT depend on synthetic media position (which can reach
// the end even when Spotify's decoder is silent). A deliberate user pause
// suspends its deadline. Neither 1x playback nor disabled downloads is touched.
// Normalise Spotify native PlayerState (track.uri, is_playing, is_paused)
// and the camelCase wrapper state used by some Spotify versions.
sgf.readRecoveryState=st=>{
  const item=st?.item||st?.track||st?.contextTrack||st?.context_track;
  const uri=item?.uri||item?.contextTrack?.uri||st?.track_uri||'';
  const paused=st?.isPaused??st?.is_paused??st?.paused;
  const playing=st?.isPlaying??st?.is_playing;
  let mode='unknown',source='unavailable';
  // Conflicting Spotify flags are not permission to manipulate playback.
  if(paused===true||playing===false){
    mode='paused';
    source=paused===true&&playing===true?'conflicting-flags':'player-flags';
  }else if(paused===false||playing===true){
    mode='playing';source='player-flags';
  }else{
    try{
      if(document.querySelector('button[data-testid="control-button-pause"]')){
        mode='playing';source='transport-button';
      }else if(document.querySelector('button[data-testid="control-button-play"]')){
        mode='paused';source='transport-button';
      }
    }catch{}
  }
  const duration=Number(st?.duration??st?.durationMs??st?.duration_ms??
    item?.duration??item?.duration_ms??item?.metadata?.duration_ms??
    item?.metadata?.duration??0);
  const position=Number(st?.positionAsOfTimestamp??st?.position_as_of_timestamp??
    st?.position??st?.positionMs??st?.position_ms??0);
  return {uri,mode,source,
    positionMs:Number.isFinite(position)&&position>=0?position:0,
    durationMs:Number.isFinite(duration)&&duration>0?
      (duration>1000?duration:duration*1000):0};
};
sgf.createPlaybackRecoveryMonitor=(opts={})=>{
  let uri='',since=0,attempts=0,busy=false,finished=false,lastDiagnostic=-Infinity;
  let lastMode='unknown';
  let generation=0;
  const clock=opts.now||(()=>Date.now());
  const state=opts.getState||(()=>sgf.currentState?.());
  const read=opts.read||sgf.readRecoveryState;
  const reset=opts.reset||(canContinue=>sgf.resetCurrentTrack?.(false,canContinue));
  const skip=opts.skip||(()=>sgf.player?.skipToNext?.()||window.Spicetify?.Player?.next?.());
  const report=opts.report||((action,name,age,details='')=>{
    sgf.send('playback_recovery',action+' title='+name+' elapsed_ms='+Math.round(age)+
      (details?' '+details:''));
  });
  const allowed=opts.allowed||(()=>sgf.state.downloads&&sgf.state.speedSupported&&
       sgf.state.playbackSpeed>=5);
  const speed=opts.speed||(()=>Number(sgf.state.playbackSpeed)||1);
  // Invalidate pending work without releasing its lock before it settles.
  const clear=()=>{generation++;uri='';since=0;attempts=0;finished=false;lastMode='unknown';};
  const diagnostic=(now,reason,name)=>{
    if(now-lastDiagnostic<30000)return;
    lastDiagnostic=now;
    report('watchdog_'+reason,name||'unknown',0);
  };
  const observeMode=(st,now)=>{
    if(st.mode===lastMode)return;
    const previous=lastMode;
    lastMode=st.mode;
    const details='from='+previous+' to='+st.mode+' source='+st.source+
      ' position_ms='+Math.round(st.positionMs)+' duration_ms='+Math.round(st.durationMs);
    if(st.mode==='paused')report('pause_observed',uri,now-since,details);
    else if(st.mode==='playing'){
      report('resume_observed',uri,now-since,details);
      since=now;
    }else diagnostic(now,'unknown_state',uri);
  };
  return {
    clear,
    async tick(){
      if(!allowed()){clear();return;}
      const now=clock();
      if(!Number.isFinite(now))return;
      const st=read(state());
      const current=st?.uri||'';
      if(!current){clear();diagnostic(now,'no_track','');return;}
      if(current!==uri){
        generation++;
        uri=current;since=now;attempts=0;finished=false;lastMode=st.mode;
        if(st.mode==='paused')
          report('track_started_paused',current,0,'source='+st.source+
            ' position_ms='+Math.round(st.positionMs));
        return;
      }
      if(finished)return;

      // Transport state does not reveal why playback paused. Record a
      // transition once, without guessing whether it was user-initiated.
      observeMode(st,now);

      // Do not override Play/Pause, remote playback or unknown transport state.
      if(st.mode!=='playing'){
        generation++;
        since=now;
        // A future resume must not inherit the old retry/skip budget.
        attempts=0;
        if(st.mode==='unknown')diagnostic(now,'unknown_state',current);
        return;
      }
      // Keep observing transitions while queue operations are pending, but
      // never begin another recovery until the first operation settles.
      if(busy)return;
      const rate=Math.max(1,Math.min(sgf.MAX_PLAYBACK_SPEED||30,speed()));
      const length=st.durationMs;
      const expected=length>0?length/rate:0;
      const deadline=Math.max(18000,expected+12000);
      const age=now-since;
      if(age<deadline)return;

      busy=true;
      const startedGeneration=generation,player=sgf.player;
      let aborted=false;
      const canContinue=(phase='during_retry')=>{
        if(aborted)return false;
        const live=read(state());
        if(startedGeneration===generation&&allowed()&&sgf.player===player&&
           live.uri===current&&live.mode==='playing')return true;
        aborted=true;
        report('recovery_aborted_state_changed',current,clock()-since,
          'phase='+phase+' mode='+live.mode+' source='+live.source+
          ' allowed='+Number(!!allowed())+' generation_changed='+Number(startedGeneration!==generation));
        if(startedGeneration===generation){
          if(live.uri===uri)observeMode(live,clock());
          generation++;attempts=0;since=clock();
        }
        return false;
      };
      try {
        // A user can press Pause after our first sample but before recovery.
        // Check both identity and state before retry, and again before skip.
        if(!canContinue('before_retry'))return;
        if(attempts<2){
          attempts++;
          report('retry_'+attempts,current,age,
            'mode='+st.mode+' source='+st.source);
          let ok=false;
          try{ok=(await reset(canContinue))===true;}catch(e){console.warn('Soggfy recovery retry failed',e);}
          if(!canContinue('before_skip'))return;
          if(ok) {
            since=clock();
            return;
          }
          // Failed async retry may have changed Spotify to paused. Even if
          // it failed, the next-track fallback must not undo that pause.
          attempts=2;
        }

        if(!canContinue('before_skip'))return;
        report('skip_stalled',current,age,
          'mode='+st.mode+' source='+st.source);
        // Only one skip per stalled identity. If skip fails, leave the player
        // alone rather than endlessly rebuilding the queue.
        finished=true;
        try{await skip();}catch(e){console.warn('Soggfy recovery skip failed',e);}
      } finally {busy=false;}
    },
    snapshot:()=>({uri,since,attempts,busy,finished,lastMode})
  };
};

sgf.installPlaybackStallRecovery=()=>{
  if(sgf.__recoveryMonitor)return;
  sgf.__recoveryMonitor=sgf.createPlaybackRecoveryMonitor();
  sgf.send('playback_recovery','watchdog_initialized state_fields=camel_or_snake');
  setInterval(()=>{void sgf.__recoveryMonitor.tick();},1500);
};
})();
