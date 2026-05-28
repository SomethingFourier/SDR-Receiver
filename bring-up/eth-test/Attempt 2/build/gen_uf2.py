#!/usr/bin/env python3
import struct
import sys

def generate_uf2(bin_file, uf2_file, family_id, load_addr):
    with open(bin_file, 'rb') as f:
        data = f.read()
    
    num_blocks = (len(data) + 475) // 476
    
    blocks = []
    for block_idx in range(num_blocks):
        start = block_idx * 476
        end = min(start + 476, len(data))
        chunk = data[start:end]
        
        block = bytearray(512)
        # UF2 magic
        struct.pack_into('<I', block, 0, 0x0A324655)   # 'UF2\\n'
        struct.pack_into('<I', block, 4, 0x9E5D4951)   # Magic 2
        
        # Flags: set NOT_FINAL (bit 0) for all non-last blocks
        flags = 0x1 if block_idx < num_blocks - 1 else 0x0
        struct.pack_into('<I', block, 8, flags)
        
        struct.pack_into('<I', block, 12, load_addr + start)   # Target address
        struct.pack_into('<I', block, 16, len(chunk))           # Data length
        struct.pack_into('<I', block, 20, block_idx)            # Block number
        struct.pack_into('<I', block, 24, num_blocks)           # Total blocks
        struct.pack_into('<I', block, 28, family_id)            # Family ID (RP2350)
        
        # Data payload
        block[32:32+len(chunk)] = chunk
        
        # Final magic
        struct.pack_into('<I', block, 508, 0x0AB16F30)
        
        blocks.append(bytes(block))
    
    with open(uf2_file, 'wb') as f:
        for b in blocks:
            f.write(b)
    
    print(f'✓ Generated {num_blocks} UF2 blocks ({len(data)} bytes)')
    print(f'  Family: 0x{family_id:08x} (RP2350)')
    print(f'  Load:   0x{load_addr:08x}')

if __name__ == '__main__':
    bin_f = sys.argv[1]
    uf2_f = sys.argv[2]
    fam = int(sys.argv[3], 16) if len(sys.argv) > 3 else 0x540ddf62
    addr = int(sys.argv[4], 16) if len(sys.argv) > 4 else 0x10000000
    generate_uf2(bin_f, uf2_f, fam, addr)
