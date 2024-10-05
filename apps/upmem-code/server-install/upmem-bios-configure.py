#!/usr/bin/env python3

import os
import sys
import array
import argparse
import subprocess

MAC_OFFSET=0xCAEA
REGION_MASK=0x7FFF
REGION_10GBE_A=11
FD_SIGNATURE=0x0FF0A55A

def dump_bios(bios_image, flash_device, tty):
    if flash_device == "bus_blaster":
	    command = [ "flashrom", "-p", "ft2232_spi:type=2232H,port=A,divisor=6", "-r", bios_image ]
    elif flash_device == "bus_pirate":
        if tty == "":
            print("Please set --tty option when using bus_pirate flash device")
            sys.exit(-1)

        command = [ "flashrom", "-p", "buspirate_spi:dev={},spispeed=8M".format(tty), "-r", bios_image ]
    else:
        print("Unknown flash device: {}".format(flash_device))
        sys.exit(-1)

    if subprocess.run(command).returncode != 0:
        print("Failed to dump BIOS")
        sys.exit(-1)

def flash_bios(bios_image, flash_device, tty):
    if flash_device == "bus_blaster":
        command = [ "flashrom", "-p", "ft2232_spi:type=2232H,port=A,divisor=6", "-N", "-w", bios_image ]
    elif flash_device == "bus_pirate":
        if tty == "":
            print("Please set --tty option when using bus_pirate flash device")
            sys.exit(-1)

        command = [ "flashrom", "-p", "buspirate_spi:dev={},spispeed=8M".format(tty), "-N", "-w", bios_image ]
    else:
        print("Unknown flash device: {}".format(flash_device))
        sys.exit(-1)

    if subprocess.run(command).returncode != 0:
        print("Failed to flash BIOS")
        sys.exit(-1)

def find_region(img, region_type):
    img.seek(0)
    words = array.array("I",img.read())
    for i in range(len(words)):
        if words[i] == FD_SIGNATURE:
            flmap0 = words[i + 1]
            frba = ((flmap0 >> 16) & 0xff) << 4
            flreg = words[frba // 4 + region_type]
            base = (flreg & REGION_MASK) << 12
            limit = ((flreg & (REGION_MASK << 16)) >> 4) | 0xfff
            return (base, limit - base + 1)
    print("region%d NOT FOUND" % (region_type))

def dump_mac(filename, print_mac = False):
    # return the first MAC address only
    mac_address = ""

    with open(filename, "rb") as img:
        base,length = find_region(img, REGION_10GBE_A)
        img.seek(base)
        cfg = img.read(length)
        cfg_words = array.array("H",cfg)
        cfg_bytes = array.array("B",cfg)
        mac_count = cfg_words[MAC_OFFSET // 2] // 8
        current_offset = MAC_OFFSET + 2
        for i in range(mac_count):
            enabled = (cfg_bytes[current_offset + 7] & 0x80) != 0
            mac = cfg_bytes[current_offset:current_offset + 6]
            current_offset += 8
            if not enabled: continue

            if mac_address == "":
                mac_address = ":".join("%02x" % (a) for a in mac)

            if print_mac:
                print(":".join("%02x" % (a) for a in mac))

        return mac_address

def set_mac(filename, mac0):
    with open(filename, "rb+") as img:
        base,length = find_region(img, REGION_10GBE_A)
        img.seek(base)
        cfg = img.read(length)
        cfg_words = array.array("H",cfg)
        cfg_bytes = array.array("B",cfg)
        mac_count = cfg_words[MAC_OFFSET//2] // 8
        current_offset = MAC_OFFSET + 2
        for i in range(mac_count):
            enabled = (cfg_bytes[current_offset + 7] & 0x80) != 0
            set_offset = current_offset
            current_offset += 8
            if not enabled: continue
            img.seek(base + set_offset)
            img.write(bytearray(mac0))
            mac0[5] += 1

def check_root():
    if os.geteuid() != 0:
        print("Error, this requires root privilege")
        sys.exit(-1)

parser = argparse.ArgumentParser(description = "UPMEM BIOS configuration tool")
parser.add_argument("--original-bios", metavar="<path to original bios>",
        help = "Path to the original BIOS extracted using an external clip",
        default = None)
parser.add_argument("bios_image", metavar="<path to BIOS image>",
        help = "Path to BIOS image",
        default=None)
parser.add_argument("--mac-address", metavar="<mac address>",
        help = "MAC address to set into UPMEM BIOS image (ignored if --original-bios is specified)",
        default = None)

parser.add_argument("--set-mac", action="store_true",
        help = "Set MAC address into the given BIOS image either using the original bios (--original_bios option) or the provided MAC address (--mac-address option)")
parser.add_argument("--flash-bios", action="store_true",
        help = "Flash the given BIOS image")
parser.add_argument("--dump-bios", action="store_true",
        help = "Dump the BIOS into the given BIOS image")
parser.add_argument("--dump-mac", action="store_true",
        help = "Dump the MAC addresses from the given BIOS image")

parser.add_argument("--flash-device", metavar="<flash_device>",
        default = "bus_pirate",
        help = "Flash device to use: bus_blaster or bus_pirate (default) which requires to set --tty option)")
parser.add_argument("--tty", metavar="<tty>",
        default = "",
        help = "TTY device to use (to be set when flash_device is set to bus_pirate)")

args = parser.parse_args()

if __name__ == "__main__":
    if args.set_mac: 
        # Extract the MAC address from the original image
        if (args.original_bios != None):
            new_mac = dump_mac(args.original_bios)
        elif (args.mac_address != None):
            new_mac = args.mac_address
        else:
            print("Error, please provide at least either --original-bios or --mac-address option")
            sys.exit(-1)

        mac_digits = [int(i,16) for i in new_mac.split(":")]
        if len(mac_digits) != 6:
            sys.stderr.write("Invalid MAC address %s\n" % new_mac)
            sys.exit(2)

        set_mac(args.bios_image, mac_digits)
    elif args.dump_mac:
        dump_mac(args.bios_image, True)
    elif args.dump_bios:
        check_root()
        dump_bios(args.bios_image, args.flash_device, args.tty)
    elif args.flash_bios:
        check_root()
        flash_bios(args.bios_image, args.flash_device, args.tty)
    else:
        print("Error, please select an action")
        parser.print_help()
        sys.exit(-1)
