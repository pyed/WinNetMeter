import { readFileSync } from 'fs';
import { resolve } from 'path';

const src = readFileSync(resolve('native/main.cpp'), 'utf8');

const requiredTokens = [
    'OverlayWndProc',
    'WS_EX_TOPMOST',
    'WS_EX_TOOLWINDOW',
    'WS_EX_LAYERED',
    'SetLayeredWindowAttributes',
    'PositionTaskbarOverlay',
    'g_overlayDragging',
    'WM_LBUTTONDOWN',
    'WM_MOUSEMOVE',
    'WM_LBUTTONUP'
];

for (const token of requiredTokens) {
    if (!src.includes(token)) {
        console.error(`FAIL: main.cpp is missing overlay widget token: "${token}"`);
        process.exit(1);
    }
}

console.log('taskbar widget verification passed');
process.exit(0);
