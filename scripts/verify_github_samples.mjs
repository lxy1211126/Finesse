import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';

const root = path.resolve(process.argv[2] ?? '');
const manifestName = process.argv[3] ?? 'manifest-paper-categories.json';
if (!process.argv[2]) {
  throw new Error(
    'Usage: node scripts/verify_github_samples.mjs DATASET_DIR [MANIFEST]');
}
const manifest = JSON.parse(
  fs.readFileSync(path.join(root, manifestName), 'utf8'));
let checkedBytes = 0;
for (const item of manifest.files) {
  const filename = path.join(root, item.path);
  const bytes = fs.readFileSync(filename);
  const digest = crypto.createHash('sha256').update(bytes).digest('hex');
  if (bytes.length !== item.bytes || digest !== item.sha256) {
    throw new Error(`Integrity mismatch: ${item.path}`);
  }
  checkedBytes += bytes.length;
}
if (manifest.total_bytes !== checkedBytes) {
  throw new Error('Manifest total_bytes does not match verified payload');
}
console.log(JSON.stringify({
  status: 'PASS',
  files: manifest.files.length,
  bytes: checkedBytes,
  original_paper_datasets_included:
    manifest.paper_original_datasets_included,
}, null, 2));
