import { readFileSync } from 'fs';
import { resolve } from 'path';

const src = readFileSync(resolve('native/main.cpp'), 'utf8');

if (!src.includes('RegisterWindowMessageW(L"TaskbarCreated")')) {
    console.error('FAIL: main.cpp must register TaskbarCreated message');
    process.exit(1);
}

if (!src.includes('g_uTaskbarCreatedMsg') || !src.includes('SetupTrayIcon()')) {
    console.error('FAIL: main.cpp must handle TaskbarCreated message to re-register tray icon');
    process.exit(1);
}

console.log('explorer restart handling verification passed');
process.exit(0);
