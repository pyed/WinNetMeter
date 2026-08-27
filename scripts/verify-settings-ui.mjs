import { readFileSync } from 'fs';
import { resolve } from 'path';

const src = readFileSync(resolve('native/main.cpp'), 'utf8');

const requiredTokens = [
    'OpenSettingsDialog',
    'ChooseColorW',
    'ChooseFontW',
    'SettingsPreviewWndProc',
    'ID_SET_BG_BTN',
    'ID_SET_DOWN_BTN',
    'ID_SET_UP_BTN',
    'ID_SET_FONT_BTN',
    'ID_SET_SAVE_BTN',
    'ID_SET_CANCEL_BTN'
];

for (const token of requiredTokens) {
    if (!src.includes(token)) {
        console.error(`FAIL: main.cpp is missing settings UI token: "${token}"`);
        process.exit(1);
    }
}

console.log('settings UI verification passed');
process.exit(0);
