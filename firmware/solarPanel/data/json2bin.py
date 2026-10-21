#!/usr/bin/env python3
#! -*- coding: utf-8 -*-

import struct
import json
import zlib
import argparse
import sys

def compute_checksum(data):
    return sum(data) % 65536  # Simple checksum (modulo 65536)

def compute_crc32(data):
    return zlib.crc32(data) & 0xFFFFFFFF  # CRC32 checksum

def json_to_binary(json_file, binary_file):
    with open(json_file, 'r') as f:
        data = json.load(f)
    
    description = data.get("description", "").encode('utf-8')[:128]
    description = description.ljust(128, b'\0')  # Ensure 128-byte padding
    sampling_duration = data.get("sampling_duration", 0)
    entries = data.get("entries", [])
    
    #magic number - version - sampling (ms) - size description
    header = struct.pack('<HHII', 0xCAFE, 0x01, sampling_duration, len(entries))
    
    binary_data = header + description
    checksum_header = compute_checksum(binary_data)
    binary_data += struct.pack('<H', checksum_header)  # Store header checksum after description
    
    data_section = b''
    num_entries = len(entries)
    # Print a progress dot every 5% of entries processed
    step = max(1, num_entries // 20)
    for idx, entry in enumerate(entries, start=1):
        data_section += struct.pack('<IH', entry["ts"], entry["value"])
        if idx % step == 0:
            print('.', end='', flush=True)
    
    # print('data '+' '.join([hex(b) for b in data_section]))
    checksum_final = compute_crc32(data_section)
    # print('crc '+hex(checksum_final))
    with open(binary_file, 'wb') as f:
        f.write(binary_data + data_section + struct.pack('<I', checksum_final))
    print()  # newline after progress dots

def binary_to_json(binary_file, json_file):
    with open(binary_file, 'rb') as f:
        header = f.read(12)  # Read header: magic number, version, sampling duration, number of entries
        description_padded = f.read(128)
        description = description_padded.rstrip(b'\0').decode('utf-8')
        checksum_header = struct.unpack('<H', f.read(2))[0]  # Read checksum after description

        # Compute checksum over header + padded description (same as when written)
        computed_checksum = compute_checksum(header + description_padded)
        if computed_checksum != checksum_header:
            raise ValueError("Invalid header checksum!")

        sampling_duration, num_entries = struct.unpack('<II', header[4:])
        entries = []
        data_section = f.read(num_entries * 6)
        for i in range(num_entries):
            ts, value = struct.unpack_from('<IH', data_section, i * 6)
            entries.append({"ts": ts, "value": value})
        
        checksum_final_stored = struct.unpack('<I', f.read(4))[0]
        computed_crc32 = compute_crc32(data_section)
        
        if computed_crc32 != checksum_final_stored:
            raise ValueError("Invalid final checksum!")
    
    with open(json_file, 'w') as f:
        json.dump({"description": description, "sampling_duration": sampling_duration, "entries": entries}, f, indent=2)

def _build_parser():
    p = argparse.ArgumentParser(description='Convert between JSON and binary sequence formats')
    sub = p.add_subparsers(dest='command', required=True)

    p_to_bin = sub.add_parser('to-bin', help='Convert JSON -> binary')
    p_to_bin.add_argument('input', help='Input JSON file')
    p_to_bin.add_argument('output', help='Output binary file')

    p_to_json = sub.add_parser('to-json', help='Convert binary -> JSON')
    p_to_json.add_argument('input', help='Input binary file')
    p_to_json.add_argument('output', help='Output JSON file')

    return p


def main(argv=None):
    argv = argv if argv is not None else sys.argv[1:]
    parser = _build_parser()
    args = parser.parse_args(argv)

    try:
        if args.command == 'to-bin':
            json_to_binary(args.input, args.output)
        elif args.command == 'to-json':
            binary_to_json(args.input, args.output)
    except Exception as e:
        print(f'Error: {e}', file=sys.stderr)
        return 2

    return 0


if __name__ == '__main__':
    raise SystemExit(main())

