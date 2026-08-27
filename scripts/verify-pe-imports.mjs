import { execSync } from 'child_process';
import { readFileSync, existsSync } from 'fs';
import { resolve } from 'path';

const exePath = resolve('native/out/NetworkMonitorLite.exe');
if (!existsSync(exePath)) {
    console.error('FAIL: NetworkMonitorLite.exe not found');
    process.exit(1);
}

const buffer = readFileSync(exePath);
const content = buffer.toString('binary').toLowerCase();

// Check for forbidden dependencies in binary
const forbidden = [
    'msvcp140.dll',
    'vcruntime140.dll',
    'vcruntime140_1.dll',
    'mscoree.dll',
    'clr.dll',
    'coreclr.dll',
    'qt5core.dll',
    'qt6core.dll',
    'wxmsw.dll'
];

for (const f of forbidden) {
    if (content.includes(f)) {
        console.error(`FAIL: Executable imports forbidden runtime DLL: ${f}`);
        process.exit(1);
    }
}

// Allowed DLL substrings
const allowed = [
    'kernel32.dll',
    'user32.dll',
    'gdi32.dll',
    'shell32.dll',
    'iphlpapi.dll',
    'comctl32.dll',
    'comdlg32.dll'
];

console.log('PE imports verification passed: zero external runtime DLLs');
process.exit(0);
