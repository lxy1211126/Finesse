import fs from 'node:fs';
import path from 'node:path';
// Concatenate in list order. No archive headers or delimiters are inserted.
const [listArg,outArg]=process.argv.slice(2);
if(!listArg||!outArg)throw new Error('Usage: node scripts/make_test_stream.mjs data/files_smoke.txt NEW_OUTPUT.bin');
const list=path.resolve(listArg),out=path.resolve(outArg),root=path.dirname(list);
const names=fs.readFileSync(list,'utf8').trim().split(/\r?\n/).filter(Boolean);
const inputs=names.map(n=>path.resolve(root,n));
if(inputs.includes(out))throw new Error('Output cannot replace an input');
for(const input of inputs)if(!fs.statSync(input).isFile())throw new Error('Not a file: '+input);
const fd=fs.openSync(out,'wx');let bytes=0;
try{for(const input of inputs){const b=fs.readFileSync(input);let off=0;while(off<b.length)off+=fs.writeSync(fd,b,off,b.length-off);bytes+=b.length;}}finally{fs.closeSync(fd);}
console.log(JSON.stringify({files:inputs.length,bytes,output:out}));
