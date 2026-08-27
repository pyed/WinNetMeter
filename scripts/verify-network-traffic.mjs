import { readFileSync } from 'fs';
import { resolve } from 'path';

const files = [
    'native/main.cpp',
    'native/network.cpp',
    'native/network.h',
    'native/settings.cpp',
    'native/settings.h'
];

let fullCode = '';
for (const f of files) {
    fullCode += readFileSync(resolve(f), 'utf8') + '\n';
}

const networkPatterns = [
    /\bsocket\s*\(/i,
    /\bconnect\s*\(/i,
    /\bWSAConnect\b/i,
    /\bsend\s*\(/i,
    /\brecv\s*\(/i,
    /\bInternetOpen\b/i,
    /\bWinHttpOpen\b/i,
    /\bHttpOpenRequest\b/i
];

for (const p of networkPatterns) {
    if (p.test(fullCode)) {
        console.error(`FAIL: Found outbound network call matching ${p}`);
        process.exit(1);
    }
}

console.log('network traffic audit verification passed: zero unsolicited network connections');
process.exit(0);
