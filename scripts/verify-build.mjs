import { execSync } from 'child_process';
import { existsSync } from 'fs';
import { resolve } from 'path';

try {
    const cwd = resolve('native');
    execSync('cmd.exe /c build.bat', { cwd, stdio: 'pipe' });
    const exePath = resolve('native/out/NetworkMonitorLite.exe');
    if (!existsSync(exePath)) {
        console.error('FAIL: NetworkMonitorLite.exe does not exist');
        process.exit(1);
    }
    console.log('build verification passed: NetworkMonitorLite.exe created with static CRT');
    process.exit(0);
} catch (e) {
    console.error('FAIL: Build failed:', e.message);
    process.exit(1);
}
