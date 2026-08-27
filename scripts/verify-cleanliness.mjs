import { existsSync, readdirSync } from 'fs';
import { resolve } from 'path';

// Check essential files
const required = [
    'NATIVE_REWRITE_SPEC.md',
    'GATES.md',
    'native/main.cpp',
    'native/network.cpp',
    'native/network.h',
    'native/settings.cpp',
    'native/settings.h',
    'native/build.bat',
    'native/app.rc',
    'native/icon.ico',
    'networkMonitorLite/MainForm.cs',
    'networkMonitorLite/NetworkStatsTracker.cs'
];

for (const req of required) {
    if (!existsSync(resolve(req))) {
        console.error(`FAIL: Required file missing: ${req}`);
        process.exit(1);
    }
}

// Check native root directory for unexpected files
const nativeFiles = readdirSync(resolve('native'));
const expectedNative = new Set([
    'app.rc',
    'build.bat',
    'icon.ico',
    'main.cpp',
    'network.cpp',
    'network.h',
    'out',
    'settings.cpp',
    'settings.h',
    'tests'
]);

for (const f of nativeFiles) {
    if (!expectedNative.has(f)) {
        console.error(`FAIL: Unexpected untracked file in native/: ${f}`);
        process.exit(1);
    }
}

console.log('repository cleanliness verification passed');
process.exit(0);
