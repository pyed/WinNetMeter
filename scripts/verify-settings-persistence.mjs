import { execSync } from 'child_process';
import { readFileSync, existsSync } from 'fs';
import { resolve } from 'path';

const src = readFileSync(resolve('native/settings.cpp'), 'utf8');

if (!src.includes('GetPrivateProfileStringW') || !src.includes('WritePrivateProfileStringW')) {
    console.error('FAIL: settings.cpp must use standard Win32 profile functions');
    process.exit(1);
}

if (!src.includes('NetworkMonitorLite') || !src.includes('settings.ini')) {
    console.error('FAIL: settings.cpp must target NetworkMonitorLite and settings.ini');
    process.exit(1);
}

try {
    const cwd = resolve('native/tests');
    const exePath = resolve('native/tests/unit_tests.exe');
    const cmd = existsSync(exePath) ? 'unit_tests.exe' : 'cmd.exe /c run_tests.bat';
    const out = execSync(cmd, { cwd, encoding: 'utf8' });
    if (!out.includes('PASS: TestSettings')) {
        console.error('FAIL: TestSettings failed');
        process.exit(1);
    }
    console.log('settings persistence verification passed');
    process.exit(0);
} catch (e) {
    console.error('FAIL: Settings persistence verification failed:', e.message);
    process.exit(1);
}
