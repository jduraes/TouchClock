#!/usr/bin/env python3
import serial
import sys
import time

port = serial.Serial('COM4', 115200, timeout=1)
time.sleep(2)  # Wait for device to be ready

for i in range(50):
    if port.in_waiting > 0:
        line = port.readline().decode(errors='ignore').strip()
        if line:
            print(line)
    else:
        time.sleep(0.1)

port.close()
