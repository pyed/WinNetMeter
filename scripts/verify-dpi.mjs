import { readFileSync } from 'fs';
import { resolve } from 'path';

const src = readFileSync(resolve('native/main.cpp'), 'utf8');

if (!src.includes('SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)')) {
    console.error('FAIL: main.cpp must initialize Per-Monitor V2 DPI awareness');
    process.exit(1);
}

if (!src.includes('WM_DPICHANGED')) {
    console.error('FAIL: main.cpp must handle WM_DPICHANGED');
    process.exit(1);
}

if (!src.includes('RefreshFontsAndRelayout') || !src.includes('RelayoutMainControls') || !src.includes('RelayoutSettingsDialog')) {
    console.error('FAIL: main.cpp must relayout and reassign fonts to all child controls on DPI change');
    process.exit(1);
}

if (!src.includes('GetDpiForWindow(hwnd)') && !src.includes('GetDpiForWindow(g_hwndOverlay)')) {
    console.error('FAIL: overlay and dialogs must query their own window DPI');
    process.exit(1);
}

console.log('DPI awareness verification passed');
process.exit(0);
