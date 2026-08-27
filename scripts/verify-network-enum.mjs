import { execSync } from 'child_process';
import { readFileSync, existsSync } from 'fs';
import { resolve } from 'path';

const header = readFileSync(resolve('native/network.h'), 'utf8');
const cpp = readFileSync(resolve('native/network.cpp'), 'utf8');

if (!header.includes('ADAPTER_TYPE_GIGABIT = 117')) {
    console.error('FAIL: GigabitEthernet must be defined as 117 per NDIS/Windows specification');
    process.exit(1);
}

if (!cpp.includes('FilterInterface')) {
    console.error('FAIL: FilterInterface must be filtered out in GetAdapters');
    process.exit(1);
}

try {
    const cwd = resolve('native/tests');
    const exePath = resolve('native/tests/unit_tests.exe');
    const cmd = existsSync(exePath) ? 'unit_tests.exe' : 'cmd.exe /c run_tests.bat';
    const out = execSync(cmd, { cwd, encoding: 'utf8' });
    if (!out.includes('PASS: TestLiveAdapters')) {
        console.error('FAIL: TestLiveAdapters failed');
        process.exit(1);
    }
    console.log('network enumeration verification passed');
    process.exit(0);
} catch (e) {
    console.error('FAIL: Network enum verification failed:', e.message);
    process.exit(1);
}
