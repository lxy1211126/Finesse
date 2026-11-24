import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath} from 'node:url';

const MiB = 1024 * 1024;
const DEFAULT_SEED = 20261123;
const DEFAULT_SIZE_MIB = 1;

function parseArgs(args) {
  const result = {};
  for (let i = 0; i < args.length; i += 2) {
    if (!['--out', '--seed', '--size-mib'].includes(args[i]) ||
        args[i + 1] === undefined) {
      throw new Error(
        'Usage: node scripts/generate_github_samples.mjs ' +
        '--out NEW_EMPTY_DIRECTORY [--seed 20261123] [--size-mib 1]');
    }
    result[args[i]] = args[i + 1];
  }
  if (!result['--out']) throw new Error('--out is required');
  return result;
}

function shake(seed, label, length) {
  return crypto.createHash('shake256', {outputLength: length})
    .update(JSON.stringify(['finesse-github-samples-v1', seed, label]))
    .digest();
}

function categoryBase(seed, category, length) {
  const output = Buffer.alloc(length);
  for (let offset = 0, page = 0; offset < length;
       offset += 4096, page++) {
    const size = Math.min(4096, length - offset);
    let pageBytes;
    if (category === 'web') {
      const token = shake(seed, `${category}/${page}`, 128).toString('hex');
      pageBytes = Buffer.from(
        (`<!doctype html><article data-snapshot="${page}">` +
         `<h1>Generated page ${page}</h1>` +
         `<p>archive record ${token}</p>`).repeat(64), 'ascii');
    } else if (category === 'rdb') {
      pageBytes = Buffer.alloc(size);
      for (let row = 0; row < size; row += 256) {
        const record = Buffer.alloc(256);
        record.write(`key:${page}:${row / 256};value:`, 0, 'ascii');
        shake(seed, `${category}/${page}/${row}`, 192).copy(record, 64);
        record.copy(pageBytes, row, 0, Math.min(256, size - row));
      }
    } else if (category === 'syn') {
      pageBytes = shake(seed, `${category}/${page}`, size);
    } else if (category === 'vma' || category === 'vmb') {
      pageBytes = page % 10 < (category === 'vma' ? 3 : 1)
        ? Buffer.alloc(size)
        : shake(seed, `${category}/${page}`, size);
      if (page % 10 === 3) {
        Buffer.from(`SAMPLE_${category.toUpperCase()}_PAGE_${page}\n`)
          .copy(pageBytes);
      }
    } else {
      throw new Error(`Unknown category: ${category}`);
    }
    pageBytes.copy(output, offset, 0, size);
  }
  return output;
}

function makeVariants(base, seed, category) {
  const result = [
    {name: '00_base', data: base, kind: 'base'},
    {name: '01_exact', data: Buffer.from(base), kind: 'exact'},
  ];
  for (const [name, width, kind] of [
    ['02_local', 32, 'local'],
    ['04_heavy', 3276, 'heavy'],
  ]) {
    const data = Buffer.from(base);
    for (let start = 0, index = 0; start < base.length;
         start += 8192, index++) {
      const block = Math.min(8192, base.length - start);
      const editLength = Math.min(width, block);
      const offset = start + ((index * 977 + 211) % (block - editLength + 1));
      for (let i = offset; i < offset + editLength; i++) {
        data[i] ^= 1 + (index % 255);
      }
    }
    result.push({name, data, kind});
  }
  const pieces = [];
  for (let start = 0, index = 0; start < base.length;
       start += 131072, index++) {
    const end = Math.min(start + 131072, base.length);
    const removeAt = start + Math.floor((end - start) / 4);
    const insertAt = start + Math.floor((end - start) / 2);
    const deleted = Math.min(1024, insertAt - removeAt);
    pieces.push(
      base.subarray(start, removeAt),
      base.subarray(removeAt + deleted, insertAt),
      shake(seed, `${category}/insert/${index}`, 1536),
      base.subarray(insertAt, end));
  }
  result.push({
    name: '03_indel',
    data: Buffer.concat(pieces),
    kind: 'indel',
  });
  result.push({
    name: '05_unrelated',
    data: shake(seed, `${category}/unrelated`, base.length),
    kind: 'unrelated',
  });
  return result.sort((a, b) => a.name.localeCompare(b.name, 'en'));
}

function saveCategory(root, manifest, category, seed, sizeMiB) {
  const relative = category;
  const directory = path.join(root, relative);
  fs.mkdirSync(directory, {recursive: true});
  const base = categoryBase(seed, category, sizeMiB * MiB);
  for (const item of makeVariants(base, seed, category)) {
    const filename = `${item.name}.bin`;
    const destination = path.join(directory, filename);
    fs.writeFileSync(destination, item.data);
    manifest.files.push({
      path: `${relative}/${filename}`,
      bytes: item.data.length,
      sha256: crypto.createHash('sha256').update(item.data).digest('hex'),
      provenance: 'generated synthetic proxy; not original paper data',
      variant: item.kind,
      base: item.kind === 'base' ? null : `${relative}/00_base.bin`,
    });
  }
}

function main() {
  const args = parseArgs(process.argv.slice(2));
  const output = path.resolve(args['--out']);
  const seed = Number(args['--seed'] ?? DEFAULT_SEED);
  const sizeMiB = Number(args['--size-mib'] ?? DEFAULT_SIZE_MIB);
  if (!Number.isSafeInteger(seed)) throw new Error('seed must be an integer');
  if (!Number.isInteger(sizeMiB) || sizeMiB < 1 || sizeMiB > 16) {
    throw new Error('--size-mib must be an integer from 1 to 16');
  }
  fs.mkdirSync(output, {recursive: true});
  const existing = fs.readdirSync(output);
  if (existing.length !== 0) {
    throw new Error('Output directory must be empty.');
  }
  const manifest = {
    schema: 1,
    seed,
    base_size_mib: sizeMiB,
    paper_original_datasets_included: false,
    warning: 'Only selected files under tar/ are real Linux source samples. ' +
      'WEB, RDB, SYN, VMA, and VMB are synthetic proxies, not original ' +
      'paper data.',
    files: [],
  };
  for (const category of ['web', 'rdb', 'syn', 'vma', 'vmb']) {
    saveCategory(output, manifest, category, seed, sizeMiB);
  }
  manifest.total_bytes = manifest.files.reduce((sum, item) =>
    sum + item.bytes, 0);
  fs.writeFileSync(
    path.join(output, 'manifest-paper-categories.json'),
    `${JSON.stringify(manifest, null, 2)}\n`);
  console.log(JSON.stringify({
    output,
    files: manifest.files.length,
    payload_bytes: manifest.total_bytes,
    seed,
  }, null, 2));
}

if (process.argv[1] &&
    path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  main();
}
