import { execSync } from 'child_process';
import { readFileSync, existsSync } from 'fs';
import { resolve } from 'path';

// 1. Check types in header
const header = readFileSync(resolve('native/network.h'), 'utf8');
if (!header.includes('ULONGLONG lastIn') || !header.includes('ULONGLONG totalIn')) {
    console.error('FAIL: network.h must use 64-bit ULONGLONG for counters');
    process.exit(1);
}

// 2. Run unit test suite which tests values > 4GB (e.g. 10,000,000,000 bytes)
try {
    const cwd = resolve('native/tests');
    const exePath = resolve('native/tests/unit_tests.exe');
    const cmd = existsSync(exePath) ? 'unit_tests.exe' : 'cmd.exe /c run_tests.bat';
    const out = execSync(cmd, { cwd, encoding: 'utf8' });
    if (!out.includes('PASS: TestNetSamplerMock')) {
        console.error('FAIL: TestNetSamplerMock failed');
        process.exit(1);
    }
    console.log('64-bit counters verification passed');
    process.exit(0);
} catch (e) {
    console.error('FAIL: 64-bit counters verification failed:', e.message);
    process.exit(1);
}
