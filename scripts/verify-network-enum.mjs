import { execSync } from 'child_process';
import { resolve } from 'path';

try {
    const cwd = resolve('native/tests');
    const out = execSync('cmd.exe /c run_tests.bat', { cwd, encoding: 'utf8' });
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
