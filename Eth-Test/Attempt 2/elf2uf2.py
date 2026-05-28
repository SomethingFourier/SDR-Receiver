#!/usr/bin/env python3
"""Simple ELF to UF2 converter"""
import struct
import sys

# RP2350 Family ID
FAMILY_ID = 0x540ddf62

def read_elf(filename):
    """Read ELF file and extract loadable segments"""
    with open(filename, 'rb') as f:
        data = f.read()
    
    # Parse ELF header (simplified)
    if data[0:4] != b'\x7fELF':
        raise ValueError("Not an ELF file")
    
    # For now, just return the file as-is for conversion
    # Real conversion would need proper ELF parsing
    return data

def elf_to_uf2(elf_data, output_file, family_id=FAMILY_ID):
    """Convert ELF data to UF2 format (simplified)"""
    # This is a placeholder - actual conversion needs proper ELF parsing
    # For now, just report that conversion is needed
    print(f"[INFO] ELF size: {len(elf_data)} bytes")
    print(f"[INFO] Target family ID: 0x{family_id:08x}")
    print(f"[INFO] For proper conversion, use: arm-none-eabi-objcopy -O binary input.elf input.bin")
    return False

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: elf2uf2.py <elf_file> [output_uf2]")
        sys.exit(1)
    
    elf_file = sys.argv[1]
    out_file = sys.argv[2] if len(sys.argv) > 2 else elf_file.replace('.elf', '.uf2')
    
    print(f"[*] Converting {elf_file} to UF2...")
    elf_to_uf2(read_elf(elf_file), out_file)
