import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath} from 'node:url';

// No dependencies. All data bytes depend only on seed, label and size.
export const VERSION = '1.0.0';
const MiB = 1024 * 1024;
export const sha = b => crypto.createHash('sha256').update(b).digest('hex');
function bytes(seed, label, n) {
  return crypto.createHash('shake256', {outputLength: n})
    .update(JSON.stringify(['finesse-mini-v1', seed, label])).digest();
}
function baseData(seed, family, n) {
  if (family === 'smoke') return bytes(seed, family, n);
  const out = Buffer.alloc(n);
  for (let off = 0, page = 0; off < n; off += 4096, page++) {
    const len = Math.min(4096, n - off);
    let b;
    if (family === 'web_like' || family === 'source_like') {
      const token = bytes(seed, `${family}/${page}`, 384).toString('hex');
      let s = family === 'web_like'
        ? `<!doctype html>\n<article data-page="${page}"><h1>Synthetic page ${page}</h1>\n`
        : `/* Synthetic source page ${page}; not a compilable project. */\n`;
      for (let j = 0; s.length < len; j++) {
        const t = token.slice((j % 12) * 64, (j % 12 + 1) * 64);
        s += family === 'web_like'
          ? `<p id="${page}-${j}">backup snapshot record ${t}</p>\n`
          : `static const char value_${page}_${j}[] = "${t}"; /* record */\n`;
      }
      b = Buffer.from(s, 'ascii').subarray(0, len);
    } else if (family === 'kv_like') {
      b = Buffer.alloc(len);
      for (let r = 0; r < len; r += 256) {
        const row = Buffer.alloc(256);
        row.write(`key:${page.toString().padStart(8, '0')}:${r / 256} value:`, 'ascii');
        bytes(seed, `${family}/${page}/${r}`, 192).copy(row, 64);
        row.copy(b, r, 0, Math.min(256, len - r));
      }
    } else if (family === 'vm_like') {
      // Explicit zero, structured, and high-entropy pages; not a VM disk format.
      b = page % 10 < 2 ? Buffer.alloc(len) : bytes(seed, `${family}/${page}`, len);
      if (page % 10 === 2) Buffer.from(`SYNTHETIC_BLOCK_${page}\n`.repeat(256)).copy(b, 0, 0, len);
    } else throw new Error(`Unknown family ${family}`);
    b.copy(out, off);
  }
  return out;
}
export function variants(base, seed, family) {
  const result = [{name:'00_base', data:base, kind:'base', edits:[]}];
  result.push({name:'01_exact', data:Buffer.from(base), kind:'exact', edits:[]});
  for (const [name, width, kind] of [['02_local',32,'local'],['04_heavy',3276,'heavy']]) {
    const b = Buffer.from(base), edits = [];
    for (let start = 0, i = 0; start < base.length; start += 8192, i++) {
      const block = Math.min(8192, base.length-start);
      const length = Math.min(width, block);
      const offset = start + ((i * 977 + 211) % (block-length+1));
      const mask = 1 + i % 255;
      for (let k = offset; k < offset+length; k++) b[k] ^= mask;
      edits.push({op:'xor', offset, length, mask});
    }
    result.push({name, data:b, kind, edits});
  }
  const pieces = [], edits = [];
  for (let start = 0, i = 0; start < base.length; start += 131072, i++) {
    const end = Math.min(start+131072, base.length);
    const del = start+Math.floor((end-start)/4);
    const ins = start+Math.floor((end-start)/2);
    const deleted = Math.min(1024, ins-del);
    const inserted = bytes(seed, `${family}/insert/${i}`, 1536);
    pieces.push(base.subarray(start,del), base.subarray(del+deleted,ins), inserted, base.subarray(ins,end));
    edits.push({op:'delete', offset:del, length:deleted});
    edits.push({op:'insert', offset:ins, length:inserted.length, sha256:sha(inserted)});
  }
  result.push({name:'03_indel',data:Buffer.concat(pieces),kind:'indel',edits});
  result.push({name:'05_unrelated',data:bytes(seed, `${family}/unrelated`, base.length),kind:'unrelated',edits:[]});
  return result.sort((a,b)=>a.name.localeCompare(b.name,'en'));
}
export function generate(root, seed=20261030, baseMiB=2) {
  if (!Number.isSafeInteger(seed)) throw new Error('seed must be an integer');
  if (!Number.isInteger(baseMiB) || baseMiB < 1 || baseMiB > 64) throw new Error('base-mib must be 1..64');
  fs.mkdirSync(root,{recursive:true});
  if (fs.readdirSync(root).length) throw new Error('Output directory must be empty; existing files are never overwritten.');
  const manifest = {schema:1, generator_version:VERSION, seed, base_mib:baseMiB,
    synthetic:true, paper_original_dataset:false,
    source:'https://www.usenix.org/system/files/fast19-zhang.pdf',
    byte_generator:'SHAKE256(JSON UTF-8 ["finesse-mini-v1", seed, label]), requested output length',
    edit_coordinates:'zero-based offsets in the base file; all variants independently derive from 00_base',
    files:[]};
  function save(rel, data, meta) {
    const dest=path.join(root, rel); fs.mkdirSync(path.dirname(dest),{recursive:true}); fs.writeFileSync(dest,data);
    manifest.files.push({path:rel, bytes:data.length, sha256:sha(data), ...meta});
  }
  const families=[
    ['web_like','web','test-web-like'],
    ['source_like','tar','test-source-like'],
    ['kv_like','rdb','test-kv-like'],
    ['vm_like','vma','test-vm-like'],
    ['smoke','syn','test-smoke'],
  ];
  for (const [family,category,prefix] of families) {
    const b=baseData(seed,family,family==='smoke'?65536:baseMiB*MiB);
    for (const v of variants(b,seed,family)) {
      const filename=`${prefix}-${v.name}.bin`;
      save(`${category}/${filename}`,v.data,{family,kind:v.kind,
        base:v.kind==='base'?null:`${category}/${prefix}-00_base.bin`,edits:v.edits});
    }
    console.log(`Generated ${family}`);
  }
  for (const n of [0,1,47,48,49,8191,8192,8193])
    save(`syn/test-edge-len_${n.toString().padStart(5,'0')}.bin`,n?bytes(seed,`edge/${n}`,n):Buffer.alloc(0),{family:'edge_cases',kind:'edge',base:null,edits:[]});
  manifest.total_payload_bytes=manifest.files.reduce((s,f)=>s+f.bytes,0);
  fs.writeFileSync(path.join(root,'manifest-test-fixtures.json'),JSON.stringify(manifest,null,2)+'\n');
  for(const [name,files] of [
    ['files_main.txt',manifest.files.filter(f=>!['smoke','edge_cases'].includes(f.family))],
    ['files_smoke.txt',manifest.files.filter(f=>f.family==='smoke')],
    ['files_edges.txt',manifest.files.filter(f=>f.kind==='edge')]])
    fs.writeFileSync(path.join(root,name),files.map(f=>f.path).join('\n')+'\n');
  console.log(JSON.stringify({files:manifest.files.length,bytes:manifest.total_payload_bytes,MiB:manifest.total_payload_bytes/MiB}));
  return manifest;
}
if(process.argv[1] && path.resolve(process.argv[1])===fileURLToPath(import.meta.url)) {
  const args=process.argv.slice(2), opts={};
  for(let i=0;i<args.length;i+=2) {
    if(!['--out','--seed','--base-mib'].includes(args[i]) || args[i+1]===undefined) throw new Error('Usage: node generate.mjs --out NEW_EMPTY_DIRECTORY [--seed 20261030] [--base-mib 2]');
    opts[args[i]]=args[i+1];
  }
  if(!opts['--out']) throw new Error('--out is required');
  generate(path.resolve(opts['--out']),Number(opts['--seed']??20261030),Number(opts['--base-mib']??2));
}
