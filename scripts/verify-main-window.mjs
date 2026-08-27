import { readFileSync, existsSync } from 'fs';
import { execSync } from 'child_process';
import { resolve } from 'path';

// 1. Check main.cpp for expected controls and texts
const src = readFileSync(resolve('native/main.cpp'), 'utf8');

const requiredTokens = [
    'Network Interface:',
    'Download Speed:',
    'Upload Speed:',
    'Total Downloaded:',
    'Total Uploaded:',
    'networkMonitorLite',
    'by mcagriaksoy - 2025',
    'https://github.com/mcagriaksoy/NetworkMonitorLite',
    'CLR_BG',
    'CLR_DOWN',
    'CLR_UP',
    'CLR_LABEL',
    'CLR_WHITE',
    'CLR_GRAY'
];

for (const token of requiredTokens) {
    if (!src.includes(token)) {
        console.error(`FAIL: main.cpp is missing token: "${token}"`);
        process.exit(1);
    }
}

// 2. Run unit tests to check formatting strings exact match
try {
    const cwd = resolve('native/tests');
    const exePath = resolve('native/tests/unit_tests.exe');
    const cmd = existsSync(exePath) ? 'unit_tests.exe' : 'cmd.exe /c run_tests.bat';
    const out = execSync(cmd, { cwd, encoding: 'utf8' });
    if (!out.includes('PASS: TestFormatting')) {
        console.error('FAIL: TestFormatting failed');
        process.exit(1);
    }
    console.log('main window verification passed');
    process.exit(0);
} catch (e) {
    console.error('FAIL: Main window verification failed:', e.message);
    process.exit(1);
}
