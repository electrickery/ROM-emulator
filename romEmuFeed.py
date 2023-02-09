#!/usr/bin/python3
#

import sys
import serial
import time

port         = '/dev/ttyACM0'
hexOffsetStr = ""

if len(sys.argv) == 1:
   print("Usage: python3 romEmuFeed.py <hexFile> [<ttyPort>] [<hexOffset>]")
   exit()
if len(sys.argv) > 1:
    hexFile = sys.argv[1]
if len(sys.argv) > 2:
    port = sys.argv[2]
if len(sys.argv) > 3:
    hexOffsetStr = "F" + sys.argv[3]

LF = "\r\n"
sendDelay = 0.05
time.sleep(sendDelay)

ser = serial.Serial(port, 9600, timeout=2)  # open serial port

print(ser.readline())

file = open(hexFile, 'r')
lines = file.readlines()
file.close()

print(ser.readline().strip())
time.sleep(sendDelay)

if hexOffsetStr:
    ser.write(str.encode(hexOffsetStr + LF))
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
