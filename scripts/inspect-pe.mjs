import { readFileSync } from 'fs';
import { resolve } from 'path';

function inspectPE(filePath) {
    const buf = readFileSync(filePath);
    
    // Check DOS header
    if (buf.readUInt16LE(0) !== 0x5A4D) {
        throw new Error('Not a valid PE DOS header');
    }
    
    const e_lfanew = buf.readUInt32LE(0x3C);
    if (buf.readUInt32LE(e_lfanew) !== 0x00004550) {
        throw new Error('Not a valid PE NT header');
    }
    
    const machine = buf.readUInt16LE(e_lfanew + 4);
    const is64Bit = machine === 0x8664;
    console.log(`Architecture: ${is64Bit ? 'x64 (AMD64)' : 'x86 (32-bit)'}`);
    
    const optHeaderOffset = e_lfanew + 24;
    const magic = buf.readUInt16LE(optHeaderOffset);
    console.log(`PE Magic: 0x${magic.toString(16)} (${magic === 0x20b ? 'PE32+' : 'PE32'})`);
    
    // Import table data directory
    const importDirRvaOffset = optHeaderOffset + (is64Bit ? 120 : 104);
    const importDirRva = buf.readUInt32LE(importDirRvaOffset);
    const importDirSize = buf.readUInt32LE(importDirRvaOffset + 4);
    console.log(`Import Directory: RVA=0x${importDirRva.toString(16)}, Size=${importDirSize}`);
    
    // Section headers to map RVA to file offset
    const numSections = buf.readUInt16LE(e_lfanew + 6);
    const sizeOfOptHeader = buf.readUInt16LE(e_lfanew + 20);
    const sectionsOffset = optHeaderOffset + sizeOfOptHeader;
    
    const sections = [];
    for (let i = 0; i < numSections; ++i) {
        const secOffset = sectionsOffset + i * 40;
        const name = buf.toString('ascii', secOffset, secOffset + 8).replace(/\0/g, '');
        const virtSize = buf.readUInt32LE(secOffset + 8);
        const virtAddr = buf.readUInt32LE(secOffset + 12);
        const rawSize = buf.readUInt32LE(secOffset + 16);
        const rawOffset = buf.readUInt32LE(secOffset + 20);
        sections.push({ name, virtSize, virtAddr, rawSize, rawOffset });
    }
    
    function rvaToOffset(rva) {
        for (const s of sections) {
            if (rva >= s.virtAddr && rva < s.virtAddr + s.virtSize) {
                return s.rawOffset + (rva - s.virtAddr);
            }
        }
        return 0;
    }
    
    // Read import descriptors
    const importOffset = rvaToOffset(importDirRva);
    const importedDlls = [];
    let curOffset = importOffset;
    
    while (true) {
        const originalFirstThunk = buf.readUInt32LE(curOffset);
        const timeDateStamp = buf.readUInt32LE(curOffset + 4);
        const forwarderChain = buf.readUInt32LE(curOffset + 8);
        const nameRva = buf.readUInt32LE(curOffset + 12);
        const firstThunk = buf.readUInt32LE(curOffset + 16);
        
        if (nameRva === 0 && firstThunk === 0) break;
        
        const nameOffset = rvaToOffset(nameRva);
        let dllName = '';
        let ptr = nameOffset;
        while (buf[ptr] !== 0) {
            dllName += String.fromCharCode(buf[ptr]);
            ptr++;
        }
        importedDlls.push(dllName);
        curOffset += 20;
    }
    
    console.log('\nImported DLLs:');
    for (const dll of importedDlls) {
        console.log(`  - ${dll}`);
    }
}

inspectPE(resolve('native/out/NetworkMonitorLite.exe'));
