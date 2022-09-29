#!/usr/bin/env python
from __future__ import print_function

import argparse
import os
import struct
import sys
import textwrap

DEFAULT_VERSION = 1
MAGIC_STRING = "DIAG"
DEVICE_TAG = 1
DMA_TAG = 2
READ_PERM = 4
WRITE_PERM = 2


def int_hex(number):
    return int(number.replace("_", ""), 16)


def int_from_byte_array(buffer):
    result = 0
    # assuming little-endian
    for item in reversed(buffer):
        result = (result << 8) + item
    return result


def dump_coe(bin_data, cell_size=4):
    coe_start = "memory_initialization_radix=16;\n" \
                "memory_initialization_vector=\n"
    coe_end = ";\n"
    formatter = "{:0" + str(cell_size * 2) + "x}"
    hex_part = []
    for i in range(0, len(bin_data), cell_size):
        number = int_from_byte_array(bin_data[i:i + cell_size])
        hex_part.append(formatter.format(number))

    return coe_start + ", ".join(hex_part) + coe_end


def dump_c(bin_data):
    hex_lines = textwrap.wrap(
        ", ".join(["0x{:02x}".format(item) for item in bin_data]), 60,
        break_long_words=False)
    return "{\n  " + "\n  ".join(hex_lines) + "\n};\n"


def dump_memory_description(tag, name, base, length, perm):
    base_high = base >> 32
    base_low = base & ((1 << 32) - 1)
    payload_length = len(name) + 12
    return struct.pack(
        "<BBIHIB", tag, payload_length, base_low, base_high, length, perm) \
        + name.encode() + b"\x00"


def dump_header(version=None):
    return MAGIC_STRING.encode() + struct.pack("B", version or DEFAULT_VERSION)


def dump_device_description(name):
    return struct.pack("BB", DEVICE_TAG, len(name) + 1) + name.encode() \
        + b"\x00"


def check_checksum(prom_data):
    return checksum(prom_data) == 0


def perm_flag(arg):
    result = 0
    if "r" in arg or "R" in arg:
        result += 4
    if "w" in arg or "W" in arg:
        result += 2
    return result if result else int(arg)


def checksum(content):
    result = 0
    for i in range(0, len(content) // 2):
        result += struct.unpack("<H", content[i*2:i*2+2])[0]
    if len(content) % 2:
        result += struct.unpack("B", content[-1:])[0] << 8
    result = (result & 0xffff) + (result >> 16)
    result = (result & 0xffff) + (result >> 16)
    return (~result) & 0xffff


def dump_end(content=None):
    # if content is passed it will add a checksum
    if content:
        if len(content) % 2:
            return b"\x00\x03\x00" + \
                struct.pack("<H", checksum(content + b"\x00\x03\x00"))
        else:
            return b"\x00\x02" + \
                struct.pack("<H", checksum(content + b"\x00\x02"))
    return b"\x00\x00"


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--format", choices=["coe", "c", "bin"], default="coe",
        help="Output format")
    parser.add_argument("config_path")
    args = parser.parse_args()
    return args


def process_config_file(path):
    bin_data = bytearray()
    with open(path, "r") as fhandle:
        for line in fhandle:
            if line.startswith("#") or line[0] == '\n':
                continue
            raw_field, raw_value = line.split(":", 1)
            field, value = raw_field.strip().lower(), raw_value.strip()
            if field == "version":
                bin_data.extend(dump_header(int(value)))
            elif field == "name":
                bin_data.extend(dump_device_description(value))
            elif field == "dma":
                name, perm, base, length = value.split()
                bin_data.extend(
                    dump_memory_description(
                        DMA_TAG, name,
                        int_hex(base), int_hex(length), perm_flag(perm)))

    bin_data.extend(dump_end(bytes(bin_data)))
    return bin_data


def main():
    args = parse_args()

    bin_data = process_config_file(args.config_path)

    if args.format == "bin":
        output = bin_data
        os.write(sys.stdout.fileno(), output)
    elif args.format == "coe":
        output = dump_coe(bin_data)
        print(output, end="")
    else:
        output = dump_c(bin_data)
        print(output, end="")

    assert(check_checksum(bin_data))


if __name__ == "__main__":
    main()
