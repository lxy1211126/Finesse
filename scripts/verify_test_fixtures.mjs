import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import assert from 'node:assert/strict';
import {fileURLToPath} from 'node:url';
const hash=b=>crypto.createHash('sha256').update(b).digest('hex');
const root=path.resolve(process.argv[2]??path.join(path.dirname(fileURLToPath(import.meta.url)),'../data'));
const m=JSON.parse(fs.readFileSync(path.join(root,'manifest-test-fixtures.json'),'utf8'));
const report={files:0,payload_bytes:0,exact_pairs:0,edited_pairs:0,indel_pairs:0,unrelated_pairs:0};
for(const f of m.files) {
  const b=fs.readFileSync(path.join(root,f.path));
  assert.equal(b.length,f.bytes,f.path+' size'); assert.equal(hash(b),f.sha256,f.path+' SHA256');
  report.files++; report.payload_bytes+=b.length;
  if(!f.base) continue;
  const base=fs.readFileSync(path.join(root,f.base));
  if(f.kind==='exact') {assert.deepEqual(b,base);report.exact_pairs++;}
  if(f.kind==='local'||f.kind==='heavy') {
    const expected=Buffer.from(base); let changed=0,previousEnd=0;
    for(const e of f.edits) {
      assert(e.offset>=previousEnd && e.offset+e.length<=base.length);assert(e.mask>0&&e.mask<=255);
      for(let i=e.offset;i<e.offset+e.length;i++)expected[i]^=e.mask;
      previousEnd=e.offset+e.length;changed+=e.length;
    }
    assert.deepEqual(b,expected);
    assert.equal(b.reduce((n,x,i)=>n+(x!==base[i]),0),changed);
    report.edited_pairs++;
  }
  if(f.kind==='indel') {
    let src=0,dst=0;
    for(const e of f.edits) {
      assert(e.offset>=src && e.offset<=base.length);
      const copied=e.offset-src;
      assert.deepEqual(b.subarray(dst,dst+copied),base.subarray(src,e.offset));dst+=copied;src=e.offset;
      if(e.op==='delete')src+=e.length;
      else {assert.equal(hash(b.subarray(dst,dst+e.length)),e.sha256);dst+=e.length;}
    }
    assert.deepEqual(b.subarray(dst),base.subarray(src));report.indel_pairs++;
  }
  if(f.kind==='unrelated') {
    assert.equal(b.length,base.length);assert.notEqual(hash(b),hash(base));
    const known=new Set();for(let i=0;i+8192<=base.length;i+=8192)known.add(hash(base.subarray(i,i+8192)));
    for(let i=0;i+8192<=b.length;i+=8192)assert(!known.has(hash(b.subarray(i,i+8192))));
    report.unrelated_pairs++;
  }
}
assert.equal(report.payload_bytes,m.total_payload_bytes);
for (const [list,predicate] of [['files_main.txt',f=>!['smoke','edge_cases'].includes(f.family)],['files_smoke.txt',f=>f.family==='smoke'],['files_edges.txt',f=>f.kind==='edge']])
  assert.deepEqual(fs.readFileSync(path.join(root,list),'utf8').trim().split('\n'),m.files.filter(predicate).map(f=>f.path));
console.log(JSON.stringify({status:'PASS',...report},null,2));
