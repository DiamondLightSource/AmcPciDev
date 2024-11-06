#!/usr/bin/env python
import logging
from prom_data_creator import check_checksum, dump_coe, dump_header, \
    dump_device_description, dump_memory_description

from prom_data_creator import DMA_TAG, READ_PERM, WRITE_PERM
log = logging.getLogger(__name__)


def test_dump_coe_1byte_group():
    expected = \
        "memory_initialization_radix=16;\n" \
        "memory_initialization_vector=\n" \
        "01, 12, 23, 34, 45, 56, 67, de, ad, be, ef;\n"
    assert dump_coe(b"\x01\x12\x23\x34\x45\x56\x67\xde\xad\xbe\xef", 1) \
        == expected


def test_dump_coe_4bytes_group():
    expected = \
        "memory_initialization_radix=16;\n" \
        "memory_initialization_vector=\n" \
        "34231201, de675645, 00efbead;\n"
    assert dump_coe(b"\x01\x12\x23\x34\x45\x56\x67\xde\xad\xbe\xef", 4) \
        == expected


def test_dump_header():
    assert dump_header() == b"DIAG\x01"


def test_dump_device_description():
    assert dump_device_description("test_dev") == b"\x01\x09test_dev\x00"


def test_dump_memory_description():
    log.info("Testing raw data for read-only memory region")
    result = dump_memory_description(
        "test_name", 0x801011121314, 0x91929394, READ_PERM)
    assert result == \
        b"\x02\x15\x14\x13\x12\x11\x10\x80\x94\x93\x92\x91\x04test_name\x00"
    log.info("Testing raw data for write-only memory region")
    result = dump_memory_description(
        "test_name", 0x801011121314, 0x91929394, WRITE_PERM)
    assert result == \
        b"\x02\x15\x14\x13\x12\x11\x10\x80\x94\x93\x92\x91\x02test_name\x00"
    log.info("Testing raw data for readable and writable memory region")
    result = dump_memory_description(
        "test_name", 0x801011121314, 0x91929394,
        READ_PERM | WRITE_PERM)
    assert result == \
        b"\x02\x15\x14\x13\x12\x11\x10\x80\x94\x93\x92\x91\x06test_name\x00"
    result = dump_memory_description(
        "test_name", 0x0706050403020100, 0x08090a0b0c0d0e0f,
        READ_PERM | WRITE_PERM)
    assert result == \
        b"\x03\x1b\x00\x01\x02\x03\x04\x05\x06\x07" \
        b"\x0f\x0e\x0d\x0c\x0b\x0a\x09\x08\x06" \
        b"test_name\x00"


def test_check_checksum():
    assert check_checksum(
        b"DIAG\x01\x01\x0bamc525_mbf\x00\x02\x10\x00\x00\x00\x00\x00\x80\x00"
        b"\x00\x00\x80\x04ddr0\x00\x02\x10\x00\x00\x00\x80\x00\x80\x00\x00\x00"
        b"\x08\x04ddr1\x00\x00\x02\x89\xde"
    )
