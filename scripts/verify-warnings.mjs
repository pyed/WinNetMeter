import { execSync } from 'child_process';
import { resolve } from 'path';

try {
    const cwd = resolve('native');
    const out = execSync('cmd.exe /c build.bat', { cwd, encoding: 'utf8' });
    if (out.includes('warning C') || out.includes('error C')) {
        console.error('FAIL: Found warnings or errors in compiler output:\n' + out);
        process.exit(1);
    }
    console.log('warning verification passed: zero compiler warnings under W4 WX');
    process.exit(0);
} catch (e) {
    console.error('FAIL: Build with W4 WX failed:', e.message);
    process.exit(1);
}
