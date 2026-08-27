import { readFileSync } from 'fs';
import { resolve } from 'path';

const src = readFileSync(resolve('native/main.cpp'), 'utf8');

// Check that combo box associates items with InterfaceIndex using CB_SETITEMDATA
if (!src.includes('CB_SETITEMDATA') || !src.includes('CB_GETITEMDATA')) {
    console.error('FAIL: main.cpp must use CB_SETITEMDATA and CB_GETITEMDATA for stable identity');
    process.exit(1);
}

if (!src.includes('g_selectedIfIndex')) {
    console.error('FAIL: main.cpp must track selected interface index stably');
    process.exit(1);
}

// Ensure timer uses the stable index rather than combo index
if (src.includes('GetAdapters(list, 64);\n    int sel =') || src.includes('list[sel].index')) {
    console.error('FAIL: OnTimerTick must not index a fresh GetAdapters array by combo position');
    process.exit(1);
}

console.log('stable identity verification passed');
process.exit(0);
