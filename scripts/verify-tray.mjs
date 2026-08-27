import { readFileSync } from 'fs';
import { resolve } from 'path';

const src = readFileSync(resolve('native/main.cpp'), 'utf8');

const requiredTokens = [
    'CreateSpeedTrayIcon',
    'maskBits',
    'hbmpMask',
    'DestroyIcon(g_hCurrentTrayIcon)',
    'Shell_NotifyIconW',
    'Show Window',
    'Settings...',
    'Show Taskbar Widget',
    'Exit'
];

for (const token of requiredTokens) {
    if (!src.includes(token)) {
        console.error(`FAIL: main.cpp is missing tray token: "${token}"`);
        process.exit(1);
    }
}

console.log('tray icon and menu verification passed');
process.exit(0);
