#!/usr/bin/env python3
import serial
import time

# Open port and reset device via DTR
port = serial.Serial('COM4', 115200, timeout=1)
port.dtr = False  # Assert DTR to reset ESP32
time.sleep(0.5)
port.dtr = True
time.sleep(2)    # Wait for boot

print("=== Reading Serial Output ===")
for i in range(100):
    if port.in_waiting > 0:
        try:
            line = port.readline().decode(errors='ignore').strip()
            if line:
                print(line)
        except:
            pass
    time.sleep(0.05)

port.close()
