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
sgf.createPlaybackRecoveryMonitor=(opts={})=>{
  let uri='',since=0,attempts=0,busy=false,finished=false;
  const clock=opts.now||(()=>Date.now());
  const state=opts.getState||(()=>sgf.currentState?.());
  const reset=opts.reset||(()=>sgf.resetCurrentTrack?.(false));
  const skip=opts.skip||(()=>sgf.player?.skipToNext?.());
  const report=opts.report||((action,name,age)=>{
    sgf.send('playback_recovery',action+' title='+name+' elapsed_ms='+Math.round(age));
  });
  const allowed=opts.allowed||(()=>sgf.state.downloads&&sgf.state.speedSupported&&
       sgf.state.speedImmediate&&sgf.state.playbackSpeed>=5);
  const speed=opts.speed||(()=>Number(sgf.state.playbackSpeed)||1);
  const clear=()=>{uri='';since=0;attempts=0;busy=false;finished=false;};
  const durationMs=st=>{
    const value=Number(st?.duration||st?.durationMs||st?.item?.duration||
      st?.item?.metadata?.duration||st?.item?.metadata?.duration_ms||0);
    if(!Number.isFinite(value)||value<=0)return 0;
    // Spotify state/metadata normally expose duration in milliseconds.
    return value>1000?value:value*1000;
  };
  return {
    clear,
    async tick(){
      if(busy)return;
      if(!allowed()){clear();return;}
      const st=state();
      const current=st?.item?.uri||st?.item?.contextTrack?.uri||'';
      const now=clock();
      if(!current||!Number.isFinite(now)){clear();return;}
      if(current!==uri){uri=current;since=now;attempts=0;finished=false;return;}
      if(finished)return;

      // Never override Play/Pause, headset controls, remote playback or an
      // intentionally paused song. No explicit "playing" flag = no action.
      if(st.isPaused===true||st.paused===true||st.isPlaying===false){
        since=now;
        return;
      }
      if(!(st.isPaused===false||st.paused===false||st.isPlaying===true)){
        since=now;
        return;
      }
      const rate=Math.max(1,Math.min(50,speed()));
      const length=durationMs(st);
      const expected=length>0?length/rate:0;
      const deadline=Math.max(18000,expected+12000);
      const age=now-since;
      if(age<deadline)return;

      busy=true;
      try {
        // The active track may have changed while we waited for an async
        // Spotify player operation; do not reset the wrong track.
        if((state()?.item?.uri||'')!==current)return;
        if(attempts<2){
          attempts++;
          report('retry_'+attempts,current,age);
          let ok=false;
          try{ok=(await reset())===true;}catch(e){console.warn('Soggfy recovery retry failed',e);}
          if(ok) {
            since=clock();
            return;
          }
          // If the player cannot requeue its current track, do not pretend a
          // retry occurred. Let the next tick attempt a cautious skip.
          attempts=2;
        }

        if((state()?.item?.uri||'')!==current)return;
        report('skip_stalled',current,age);
        // Only one skip per stalled identity. If skip fails, leave the player
        // alone rather than endlessly rebuilding the queue.
        finished=true;
        try{await skip();}catch(e){console.warn('Soggfy recovery skip failed',e);}
      } finally {busy=false;}
    },
    snapshot:()=>({uri,since,attempts,busy,finished})
  };
};

sgf.installPlaybackStallRecovery=()=>{
  if(sgf.__recoveryMonitor)return;
  sgf.__recoveryMonitor=sgf.createPlaybackRecoveryMonitor();
  setInterval(()=>{void sgf.__recoveryMonitor.tick();},1500);
};
})();
