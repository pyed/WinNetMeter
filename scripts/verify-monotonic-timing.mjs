import { execSync } from 'child_process';
import { readFileSync, existsSync } from 'fs';
import { resolve } from 'path';

const src = readFileSync(resolve('native/network.cpp'), 'utf8');
if (!src.includes('QueryPerformanceCounter') || !src.includes('QueryPerformanceFrequency')) {
    console.error('FAIL: network.cpp must use QueryPerformanceCounter and QueryPerformanceFrequency');
    process.exit(1);
}

try {
    const cwd = resolve('native/tests');
    const exePath = resolve('native/tests/unit_tests.exe');
    const cmd = existsSync(exePath) ? 'unit_tests.exe' : 'cmd.exe /c run_tests.bat';
    const out = execSync(cmd, { cwd, encoding: 'utf8' });
    if (!out.includes('PASS: TestNetSamplerMock')) {
        console.error('FAIL: Monotonic timing test failed');
        process.exit(1);
    }
    console.log('monotonic timing verification passed');
    process.exit(0);
} catch (e) {
    console.error('FAIL: Monotonic timing test execution failed:', e.message);
    process.exit(1);
}
