import { readFileSync } from 'fs';
import { resolve } from 'path';

const src = readFileSync(resolve('native/main.cpp'), 'utf8');

const checks = [
    { name: 'DeleteObject for brushes/pens/fonts', pattern: /DeleteObject/g, minCount: 5 },
    { name: 'DestroyIcon for tray icons', pattern: /DestroyIcon/g, minCount: 2 },
    { name: 'DestroyMenu for popups', pattern: /DestroyMenu/g, minCount: 1 },
    { name: 'DeleteDC / ReleaseDC', pattern: /DeleteDC|ReleaseDC/g, minCount: 2 }
];

for (const check of checks) {
    const matches = src.match(check.pattern) || [];
    if (matches.length < check.minCount) {
        console.error(`FAIL: ${check.name} count too low (${matches.length} < ${check.minCount})`);
        process.exit(1);
    }
}

console.log('resource lifetime verification passed');
process.exit(0);
