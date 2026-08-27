import { readFileSync } from 'fs';
import { resolve } from 'path';

const src = readFileSync(resolve('native/main.cpp'), 'utf8');

const requiredTokens = [
    'ID_TRAY_EXIT',
    'KillTimer',
    'RemoveTrayIcon',
    'DestroyWindow',
    'PostQuitMessage'
];

for (const token of requiredTokens) {
    if (!src.includes(token)) {
        console.error(`FAIL: main.cpp is missing shutdown token: "${token}"`);
        process.exit(1);
    }
}

console.log('shutdown verification passed');
process.exit(0);
