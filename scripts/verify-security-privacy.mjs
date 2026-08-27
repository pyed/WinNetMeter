import { readFileSync } from 'fs';
import { resolve } from 'path';

const files = [
    'native/main.cpp',
    'native/network.cpp',
    'native/network.h',
    'native/settings.cpp',
    'native/settings.h',
    'native/app.rc'
];

let fullCode = '';
for (const f of files) {
    fullCode += readFileSync(resolve(f), 'utf8') + '\n';
}

const forbiddenPatterns = [
    /CreateProcess/i,
    /WinExec/i,
    /system\s*\(/i,
    /powershell/i,
    /cmd\.exe/i,
    /RegCreateKey/i,
    /RegSetValue/i,
    /telemetry/i,
    /analytics/i,
    /update_check/i,
    /requireAdministrator/i,
    /pcap/i,
    /raw_socket/i
];

for (const p of forbiddenPatterns) {
    if (p.test(fullCode)) {
        console.error(`FAIL: Found forbidden pattern matching ${p}`);
        process.exit(1);
    }
}

console.log('security and privacy verification passed');
process.exit(0);
