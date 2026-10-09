'use strict';

const fs = require('fs');
const vm = require('vm');
const assert = require('assert');

const intervals = [];
let refreshes = 0;
class ElementMock {
  constructor(tag = 'div') {
    this.tag = tag;
    this.children = [];
    this.isConnected = true;
    this.style = {};
    this.attributes = {};
    this.lastElementChild = null;
  }
  append(...items) { this.children.push(...items); }
  appendChild(item) { this.children.push(item); return item; }
  prepend(...items) { this.children.unshift(...items); }
  remove() { this.isConnected = false; }
  setAttribute(name, value) { this.attributes[name] = value; }
  querySelector() { return null; }
  querySelectorAll() { return []; }
}
const row = new ElementMock();
const durationCell = new ElementMock();
row.lastElementChild = durationCell;
const icons = Object.fromEntries(
  ['Error','InProgress','Processing','Warning','Done','SyncDisabled','Folder'].map(n => [n,n])
);
const sgf = {
  Icons: icons,
  FS: String.fromCharCode(0x1f),
  RS: String.fromCharCode(0x1e),
  statusMap: new Map(),
  state: { skipDownloaded: false, skipIgnored: false },
  send() {},
  parseHtml(html) { return { html }; },
};
const context = {
  window: { __SoggfyClassic: sgf },
  document: {
    body: new ElementMock(),
    visibilityState: 'visible',
    createElement(name) { return new ElementMock(name); },
    querySelector() { return row; },
    querySelectorAll() { return []; },
  },
  Element: ElementMock,
  localStorage: { getItem: () => '[]' },
  MutationObserver: class { observe() {} },
  setTimeout(callback) { refreshes++; return refreshes; },
  clearTimeout() {},
  setInterval(callback, ms) { intervals.push({ callback, ms }); },
  console,
  location: { pathname: '/playlist/example' },
};
vm.runInNewContext(fs.readFileSync('native/ui/classic_status.js','utf8'), context, {
  filename: 'classic_status.js'
});
sgf.trackInfoFromRows = () => [{
  row, uri: 'spotify:track:example', ignoreUris: ['spotify:track:example']
}];

sgf.renderVisibleStatuses();
let node = row.__sgf_status_ind;
assert(node, 'No downloaded-file record must render a badge');
assert.strictEqual(node.__sgfStatus, 'MISSING');
assert.strictEqual(node.children[1].html, 'Error', 'Missing file displays a red cross');
assert.strictEqual(node.attributes['aria-label'], 'Not downloaded');
const missing = node;

sgf.statusMap.set('spotify:track:example', {
  status: 'DONE', path: 'C:\\Music\\Artist - Song.mp3', message: 'Saved'
});
sgf.renderVisibleStatuses();
node = row.__sgf_status_ind;
assert.strictEqual(node.__sgfStatus, 'DONE');
assert.strictEqual(node.children[1].html, 'Done', 'Saved file displays green check');
assert.strictEqual(missing.isConnected, false, 'Previous red cross removed');

sgf.renderVisibleStatuses();
assert.strictEqual(row.__sgf_status_ind, node, 'Unchanged status does not redraw');

sgf.statusMap.set('spotify:track:example', {status:'ERROR',message:'Tagging error',path:''});
sgf.renderVisibleStatuses();
node = row.__sgf_status_ind;
assert.strictEqual(node.__sgfStatus, 'ERROR');
assert.strictEqual(node.title, 'Tagging error');

sgf.statusMap.set('spotify:track:example', {status:'ERROR',message:'Retry failed',path:''});
sgf.renderVisibleStatuses();
assert.notStrictEqual(row.__sgf_status_ind, node, 'Tooltip updates for same status');

assert(intervals.some(x => x.ms === 3000), 'Visible rows should be rescanned after asynchronous saves');
const before = refreshes;
intervals.find(x => x.ms === 3000).callback();
assert(refreshes > before, 'Periodic refresh must request native statuses');

console.log('PASS: Classic Soggfy crosses, checkmarks, error tooltips and refresh');
