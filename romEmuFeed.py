#!/usr/bin/python3
#

import sys
import serial    # python 2.x
#import pySerial # python 3.x??

ser = serial.Serial('/dev/ttyACM0', 9600, timeout=1)  # open serial port
print(ser.readline())

if len(sys.argv) > 1:
    hexFile = sys.argv[1]

file = open(hexFile, 'r')
lines = file.readlines()
file.close()
for line in lines:
    if (line.strip()):
        print(line.strip())
        ser.write(line)
        print(ser.readline().strip())
