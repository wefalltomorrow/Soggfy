(() => {
  'use strict';
  if (window.__soggfyClassicUiLoaded) {
    if (window.__soggfyApplyConfig && window.__soggfyNativeConfig)
      window.__soggfyApplyConfig(window.__soggfyNativeConfig);
    return;
  }
  window.__soggfyClassicUiLoaded = true;

  const PREFIX = 'SOGGFY_UI_V1:';
  const state = {
    downloads: false,
    ogg: true,
    flac: true,
    metadata: true,
    log: true,
    debug: false,
    normalize: true,
    root: '',
    template: ''
  };
  const controls = new Map();

  const send = (key, value = '') => {
    try {
      console.info(PREFIX + key + '=' + encodeURIComponent(String(value)));
    } catch {}
  };

  const parse = payload => {
    try {
      const p = new URLSearchParams(payload || '');
      const flag = (key, fallback) => {
        const v = p.get(key);
        return v === null ? fallback : v === '1';
      };
      state.downloads = flag('downloads', state.downloads);
      state.ogg = flag('ogg', state.ogg);
      state.flac = flag('flac', state.flac);
      state.metadata = flag('metadata', state.metadata);
      state.log = flag('log', state.log);
      state.debug = flag('debug', state.debug);
      state.normalize = flag('normalize', state.normalize);
      if (p.has('root')) state.root = p.get('root') || '';
      if (p.has('template')) state.template = p.get('template') || '';
      refresh();
    } catch {}
  };

  window.__soggfyApplyConfig = parse;
  if (window.__soggfyNativeConfig) parse(window.__soggfyNativeConfig);

  const svg = {
    download: '<svg viewBox="0 0 24 24" width="20" height="20" aria-hidden="true"><path fill="currentColor" d="M11 3h2v9.17l3.59-3.58L18 10l-6 6-6-6 1.41-1.41L11 12.17V3zM5 19h14v2H5z"/></svg>',
    downloadOff: '<svg viewBox="0 0 24 24" width="20" height="20" aria-hidden="true"><path fill="currentColor" d="M4.27 3 3 4.27l7.99 7.99V3h2v9.17l1.65-1.65L21 16.88 19.73 18.15 4.27 3zM5 19h11.73l2 2H5v-2z"/></svg>',
    sliders: '<svg viewBox="0 0 24 24" width="20" height="20" aria-hidden="true"><path fill="currentColor" d="M4 7h8v2H4V7zm12 0h4v2h-4V7zM4 15h4v2H4v-2zm8 0h8v2h-8v-2zM13 5h2v6h-2V5zm-4 8h2v6H9v-6z"/></svg>',
    folder: '<svg viewBox="0 0 24 24" width="18" height="18" aria-hidden="true"><path fill="currentColor" d="M3 5h7l2 2h9v12H3V5zm2 4v8h14V9H5z"/></svg>',
    close: '<svg viewBox="0 0 24 24" width="20" height="20" aria-hidden="true"><path fill="currentColor" d="m6.4 5 5.6 5.6L17.6 5 19 6.4 13.4 12l5.6 5.6-1.4 1.4-5.6-5.6L6.4 19 5 17.6l5.6-5.6L5 6.4 6.4 5z"/></svg>'
  };

  const style = document.createElement('style');
  style.id = 'soggfy-classic-style';
  style.textContent = [
    '.sgf-topbar-retractor{display:flex;align-items:center;gap:4px;margin-left:6px}',
    '.sgf-topbar-btn{width:32px;height:32px;border:0;border-radius:50%;background:transparent;color:var(--text-subdued,#b3b3b3);display:flex;align-items:center;justify-content:center;cursor:pointer}',
    '.sgf-topbar-btn:hover{color:var(--text-base,#fff);background:rgba(255,255,255,.08);transform:scale(1.04)}',
    '.sgf-topbar-btn:active{transform:scale(.96)}',
    '.sgf-download-on{color:var(--text-base,#fff)}',
    '.sgf-settings-overlay{position:fixed;inset:0;z-index:99999;background:rgba(0,0,0,.72);display:flex;align-items:center;justify-content:center;user-select:none}',
    '.sgf-settings-modal{width:min(640px,calc(100vw - 48px));height:min(760px,calc(100vh - 48px));display:flex;flex-direction:column;background:var(--background-elevated-base,#282828);border-radius:12px;box-shadow:0 12px 48px rgba(0,0,0,.5);overflow:hidden;color:var(--text-base,#fff)}',
    '.sgf-settings-header{display:flex;align-items:center;justify-content:space-between;padding:28px 30px 14px;border-bottom:1px solid rgba(255,255,255,.1)}',
    '.sgf-settings-title{font-size:28px;line-height:34px;font-weight:700;letter-spacing:-.03em;margin:0}',
    '.sgf-settings-close{border:0;background:transparent;color:inherit;padding:8px;border-radius:50%;cursor:pointer;display:flex}',
    '.sgf-settings-close:hover{background:rgba(255,255,255,.08)}',
    '.sgf-settings-elements{overflow:auto;padding:18px 30px 30px}',
    '.sgf-section{margin:0 0 26px}',
    '.sgf-section h2{font-size:16px;margin:0 0 8px;color:var(--text-base,#fff)}',
    '.sgf-section-note{font-size:12px;line-height:18px;color:var(--text-subdued,#b3b3b3);margin:0 0 10px}',
    '.sgf-setting-row{display:flex;align-items:center;min-height:42px;gap:16px}',
    '.sgf-setting-label{flex:1;font-size:14px;line-height:20px}',
    '.sgf-setting-action{display:flex;align-items:center;gap:6px;max-width:62%}',
    '.sgf-toggle{appearance:none;width:34px;height:20px;border-radius:10px;background:#535353;position:relative;cursor:pointer;transition:.15s}',
    '.sgf-toggle:before{content:"";position:absolute;width:14px;height:14px;border-radius:50%;background:white;left:3px;top:3px;transition:.15s}',
    '.sgf-toggle:checked{background:#1ed760}',
    '.sgf-toggle:checked:before{transform:translateX(14px)}',
    '.sgf-input{box-sizing:border-box;width:100%;min-width:250px;background:var(--background-highlight,#333);color:var(--text-base,#fff);border:1px solid #5a5a5a;border-radius:4px;padding:8px 10px;font:inherit;user-select:text}',
    '.sgf-input:focus{outline:1px solid #fff;border-color:#fff}',
    '.sgf-input[readonly]{color:var(--text-subdued,#b3b3b3)}',
    '.sgf-button{border:0;border-radius:500px;background:#fff;color:#000;font-weight:700;padding:8px 14px;cursor:pointer;white-space:nowrap;display:flex;align-items:center;gap:6px}',
    '.sgf-button:hover{transform:scale(1.03)}',
    '.sgf-path-row{align-items:flex-start;padding:6px 0}',
    '.sgf-path-row .sgf-setting-action{flex:1;max-width:none}',
    '.sgf-notification{position:fixed;left:50%;bottom:110px;transform:translateX(-50%);z-index:100000;background:#222;color:#fff;border-radius:6px;padding:10px 14px;box-shadow:0 4px 20px rgba(0,0,0,.45);font-size:14px;pointer-events:none;opacity:0;transition:opacity .15s}',
    '.sgf-notification.show{opacity:1}',
    '.sgf-engine-state{font-size:12px;color:var(--text-subdued,#b3b3b3);padding:8px 0 0}'
  ].join('');
  document.head.appendChild(style);

  let topbar = null;
  let modal = null;
  let noteTimer = null;

  const notify = text => {
    let n = document.querySelector('.sgf-notification');
    if (!n) {
      n = document.createElement('div');
      n.className = 'sgf-notification';
      document.body.appendChild(n);
    }
    n.textContent = text;
    n.classList.add('show');
    clearTimeout(noteTimer);
    noteTimer = setTimeout(() => n.classList.remove('show'), 2600);
  };

  const refresh = () => {
    const button = controls.get('downloadButton');
    if (button) {
      button.innerHTML = state.downloads ? svg.download : svg.downloadOff;
      button.classList.toggle('sgf-download-on', state.downloads);
      button.title = state.downloads ? 'Soggfy downloads enabled' : 'Soggfy downloads disabled';
      button.setAttribute('aria-label', button.title);
    }
    for (const key of ['downloads','ogg','flac','metadata','log','debug','normalize']) {
      const c = controls.get(key);
      if (c && c.checked !== !!state[key]) c.checked = !!state[key];
    }
    const root = controls.get('root');
    if (root && root.value !== state.root) root.value = state.root;
    const template = controls.get('template');
    if (template && template.value !== state.template) template.value = state.template;
  };

  const setFlag = (key, value) => {
    state[key] = !!value;
    refresh();
    send(key, value ? '1' : '0');
  };

  const makeToggle = key => {
    const input = document.createElement('input');
    input.type = 'checkbox';
    input.className = 'sgf-toggle';
    input.checked = !!state[key];
    input.onchange = () => setFlag(key, input.checked);
    controls.set(key, input);
    return input;
  };

  const row = (label, action, extraClass = '') => {
    const r = document.createElement('div');
    r.className = 'sgf-setting-row ' + extraClass;
    const l = document.createElement('div');
    l.className = 'sgf-setting-label';
    l.textContent = label;
    const a = document.createElement('div');
    a.className = 'sgf-setting-action';
    a.appendChild(action);
    r.append(l, a);
    return r;
  };

  const section = (title, note) => {
    const s = document.createElement('section');
    s.className = 'sgf-section';
    const h = document.createElement('h2');
    h.textContent = title;
    s.appendChild(h);
    if (note) {
      const n = document.createElement('p');
      n.className = 'sgf-section-note';
      n.textContent = note;
      s.appendChild(n);
    }
    return s;
  };

  const closeModal = () => {
    if (modal) {
      modal.remove();
      modal = null;
      controls.delete('root');
      controls.delete('template');
      for (const key of ['downloads','ogg','flac','metadata','log','debug','normalize'])
        controls.delete(key);
    }
  };

  const openSettings = () => {
    if (modal) return;
    const overlay = document.createElement('div');
    overlay.className = 'sgf-settings-overlay';
    overlay.innerHTML = '<div class="sgf-settings-modal" role="dialog" aria-modal="true"><div class="sgf-settings-header"><h1 class="sgf-settings-title">Soggfy settings</h1><button class="sgf-settings-close" aria-label="Close">' + svg.close + '</button></div><div class="sgf-settings-elements"></div></div>';
    const body = overlay.querySelector('.sgf-settings-elements');

    const general = section('General', 'Classic Soggfy controls backed by the modern x64 capture engine.');
    general.append(
      row('Downloads', makeToggle('downloads')),
      row('Capture native FLAC', makeToggle('flac')),
      row('Capture Ogg', makeToggle('ogg')),
      row('Cached metadata enrichment', makeToggle('metadata'))
    );

    const paths = section('Paths', 'Leave the track template empty to use the smart library layout.');
    const rootInput = document.createElement('input');
    rootInput.className = 'sgf-input';
    rootInput.value = state.root;
    rootInput.spellcheck = false;
    rootInput.onchange = () => send('root', rootInput.value);
    controls.set('root', rootInput);
    const browse = document.createElement('button');
    browse.className = 'sgf-button';
    browse.innerHTML = svg.folder + '<span>Browse</span>';
    browse.onclick = () => {
      send('browse', '1');
      setTimeout(() => send('sync', '1'), 1200);
      setTimeout(() => send('sync', '1'), 3200);
    };
    const rootWrap = document.createElement('div');
    rootWrap.style.display = 'flex';
    rootWrap.style.gap = '6px';
    rootWrap.style.width = '100%';
    rootWrap.append(rootInput, browse);

    const templateInput = document.createElement('input');
    templateInput.className = 'sgf-input';
    templateInput.value = state.template;
    templateInput.placeholder = '{artist_name}\\{album_name}\\{track_num_2} - {track_name}.{ext}';
    templateInput.spellcheck = false;
    templateInput.onchange = () => send('template', templateInput.value);
    controls.set('template', templateInput);

    paths.append(
      row('Base path', rootWrap, 'sgf-path-row'),
      row('Track template', templateInput, 'sgf-path-row'),
      row('Normalize artist separators', makeToggle('normalize'))
    );

    const advanced = section('Advanced', 'These affect diagnostics and metadata only; capture remains native Ogg/FLAC.');
    advanced.append(
      row('Activity log', makeToggle('log')),
      row('Debug log', makeToggle('debug'))
    );
    const engine = document.createElement('div');
    engine.className = 'sgf-engine-state';
    engine.textContent = 'The old Floggfy “To Disk” menu is disabled while Classic UI is active. Settings save immediately.';
    advanced.appendChild(engine);

    body.append(general, paths, advanced);
    overlay.querySelector('.sgf-settings-close').onclick = closeModal;
    overlay.onmousedown = ev => {
      if (ev.target === overlay) closeModal();
    };
    document.body.appendChild(overlay);
    modal = overlay;
    refresh();
    send('sync', '1');
  };

  const findTopbarHost = () => {
    const nav = document.querySelector(
      '[data-testid="top-bar-forward-button"],[data-testid="top-bar-back-button"],.main-topBar-forward,.main-topBar-back,.main-topBar-responsiveForward'
    );
    if (nav && nav.parentElement) return nav.parentElement;
    const wrapper = document.querySelector(
      '[data-testid="topbar-content-wrapper"],.main-topBar-topbarContent,header [role="banner"],header'
    );
    return wrapper || null;
  };

  const mountTopbar = () => {
    if (topbar && topbar.isConnected) return true;
    const host = findTopbarHost();
    if (!host) return false;

    let existing = document.getElementById('soggfy-classic-topbar');
    if (existing) {
      topbar = existing;
      refresh();
      return true;
    }

    const div = document.createElement('div');
    div.id = 'soggfy-classic-topbar';
    div.className = 'sgf-topbar-retractor';

    const download = document.createElement('button');
    download.className = 'sgf-topbar-btn';
    download.onclick = () => {
      const next = !state.downloads;
      setFlag('downloads', next);
      notify(next ? 'Soggfy downloads enabled' : 'Soggfy downloads disabled');
    };
    controls.set('downloadButton', download);

    const settings = document.createElement('button');
    settings.className = 'sgf-topbar-btn';
    settings.title = 'Soggfy settings';
    settings.setAttribute('aria-label', settings.title);
    settings.innerHTML = svg.sliders;
    settings.onclick = openSettings;

    div.append(download, settings);
    host.appendChild(div);
    topbar = div;
    refresh();
    return true;
  };

  let mountQueued = false;
  const queueMount = () => {
    if (mountQueued) return;
    mountQueued = true;
    requestAnimationFrame(() => {
      mountQueued = false;
      mountTopbar();
    });
  };

  const observer = new MutationObserver(queueMount);
  const start = () => {
    if (!document.body) {
      setTimeout(start, 50);
      return;
    }
    observer.observe(document.body, {childList: true, subtree: true});
    mountTopbar();
    send('sync', '1');
    setTimeout(() => send('sync', '1'), 1000);
  };
  start();
})();