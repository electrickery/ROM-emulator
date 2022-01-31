#!/usr/bin/python3
#

import sys
import serial
import time

ser = serial.Serial('/dev/ttyACM0', 9600, timeout=2)  # open serial port
LF = "\r\n"
hexOffsetStr = ""
sendDelay = 0.05
time.sleep(sendDelay)
print(ser.readline())

if len(sys.argv) > 1:
    hexFile = sys.argv[1]
if len(sys.argv) > 2:
    hexOffsetStr = "F" + sys.argv[2]

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
