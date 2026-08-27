import { execSync } from 'child_process';
import { readFileSync, existsSync } from 'fs';
import { resolve } from 'path';

// 1. Check source implementation for NET_LUID
const header = readFileSync(resolve('native/network.h'), 'utf8');
const cpp = readFileSync(resolve('native/network.cpp'), 'utf8');
const main = readFileSync(resolve('native/main.cpp'), 'utf8');

if (!header.includes('NET_LUID trackedLuid') || !header.includes('NET_LUID luid')) {
    console.error('FAIL: network.h must define NET_LUID based adapter identity');
    process.exit(1);
}

if (!cpp.includes('FetchRowByLuid') || !cpp.includes('GetIfEntry2')) {
    console.error('FAIL: network.cpp must lookup live rows via GetIfEntry2 using NET_LUID');
    process.exit(1);
}

if (!main.includes('g_selectedLuid') || !main.includes('g_comboLuids')) {
    console.error('FAIL: main.cpp must track selected interface by stable NET_LUID');
    process.exit(1);
}

// 2. Execute unit tests which verify LUID stability and mock interface recovery
try {
    const cwd = resolve('native/tests');
    const exePath = resolve('native/tests/unit_tests.exe');
    const cmd = existsSync(exePath) ? 'unit_tests.exe' : 'cmd.exe /c run_tests.bat';
    const out = execSync(cmd, { cwd, encoding: 'utf8' });
    if (!out.includes('PASS: TestNetSamplerMock') || !out.includes('PASS: TestLiveAdapters')) {
        console.error('FAIL: LUID adapter tests failed');
        process.exit(1);
    }
    console.log('stable identity verification passed');
    process.exit(0);
} catch (e) {
    console.error('FAIL: LUID test execution failed:', e.message);
    process.exit(1);
}
