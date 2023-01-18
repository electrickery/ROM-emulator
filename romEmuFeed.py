#!/usr/bin/python3
#
#version 0.2

import sys
import serial
import time
import argparse

port = '/dev/ttyACM0'
baud = 9600
parser = argparse.ArgumentParser(description='Send hex-intel file to ROM Emulator.')
parser.add_argument('-p', '--port', type=str, nargs='?',
                    help='Serial port to use (9600 BAUD)')
parser.add_argument('-x', '--hexFile', type=str, nargs='?',
                    help='hex-Intel file')
parser.add_argument('-o', '--offset', type=str, nargs='?',
                    help='offset to target address')               

args = parser.parse_args()

if (args.port):
    port = args.port
    
if (args.offset):
    offsetStr = "F" + args.offset
else:
    offsetStr = ""

if (args.hexFile):
    hexFile = args.hexFile
else:
    print("Usage: python3 romEmuFeed.py  -x HEXFILE [-p PORT] [-o OFFSET]")
    print("       default port is " + port + ", default offset is " + hex(offset))
    quit(1)


ser = serial.Serial(port, baud, timeout=2)  # open serial port
LF = "\r\n"
sendDelay = 0.05
time.sleep(sendDelay)
print(ser.readline())

file = open(hexFile, 'r')
lines = file.readlines()
file.close()

print(ser.readline().strip())
time.sleep(sendDelay)

if offsetStr:
    ser.write(str.encode(offsetStr + LF))
    print(ser.readline().strip())
    time.sleep(sendDelay)

for line in lines:
    lineStrip = line.strip()
    if (lineStrip):
        print(lineStrip)
        ser.write(str.encode(lineStrip + LF))
        time.sleep(sendDelay)
        print(ser.readline().strip())
        time.sleep(sendDelay)
