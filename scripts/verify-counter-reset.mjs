import { execSync } from 'child_process';
import { existsSync } from 'fs';
import { resolve } from 'path';

try {
    const cwd = resolve('native/tests');
    const exePath = resolve('native/tests/unit_tests.exe');
    const cmd = existsSync(exePath) ? 'unit_tests.exe' : 'cmd.exe /c run_tests.bat';
    const out = execSync(cmd, { cwd, encoding: 'utf8' });
    if (!out.includes('PASS: TestNetSamplerMock')) {
        console.error('FAIL: TestNetSamplerMock failed counter reset checks');
        process.exit(1);
    }
    console.log('counter reset and adapter recovery verification passed');
    process.exit(0);
} catch (e) {
    console.error('FAIL: Counter reset verification failed:', e.message);
    process.exit(1);
}
