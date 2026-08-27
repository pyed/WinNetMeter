import { readFileSync } from 'fs';
import { resolve } from 'path';

const src = readFileSync(resolve('native/main.cpp'), 'utf8');

const requiredTokens = [
    'MonitorFromWindow',
    'MONITOR_DEFAULTTONEAREST',
    'IsWindowVisible(g_hwndOverlay)',
    'GetMonitorInfoW',
    'rcWork',
    'rcMon',
    'WM_DISPLAYCHANGE',
    'WM_SETTINGCHANGE',
    'PositionTaskbarOverlay',
    'g_overlayDragging'
];

for (const token of requiredTokens) {
    if (!src.includes(token)) {
        console.error(`FAIL: main.cpp is missing taskbar positioning token: "${token}"`);
        process.exit(1);
    }
}

console.log('taskbar widget verification passed');
process.exit(0);
