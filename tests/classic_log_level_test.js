'use strict';
const assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const window={};const document={getElementById(){return true;},querySelector(){return null;}};
vm.runInNewContext(fs.readFileSync('native/ui/classic_core.js','utf8'),{window,document,URLSearchParams,encodeURIComponent,console,setTimeout(){}});
const sgf=window.__SoggfyClassic;
for(const level of [0,1,2,4]){
 sgf.applyConfig('logLevel='+level);
 assert.equal(sgf.state.logLevel,level);assert.equal(sgf.state.log,level!==0);assert.equal(sgf.state.debug,level===4);
}
for(const invalid of ['3','04','4junk','-1','']){sgf.applyConfig('logLevel='+invalid);assert.equal(sgf.state.logLevel,4);}
console.log('PASS: Classic log levels synchronize with legacy flags and reject invalid values');
