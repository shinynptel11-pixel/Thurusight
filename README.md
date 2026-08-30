# Thurusight
ESP32 Survivor Detection Prototype
This repository contains the ESP32 coding part of a small disaster-response project that I am currently learning and developing.

The basic idea is to use more than one sensor to check for possible human movement. In this version, I am experimenting with an RCWL radar motion sensor and a PIR sensor. The ESP32 reads both sensors and gives a simple status based on the combination of their outputs.

This is still a learning-stage prototype, so the detection logic and confidence values are simple for now. I am planning to improve the project as I learn more about sensors, signal processing and embedded systems.

What this code does
The ESP32 program currently:

Reads an RCWL motion sensor

Reads a PIR sensor

Shows the result on a 128×64 OLED display

Gives a buzzer alert for a high-confidence detection

Uses a push button to change between two modes

Provides an automatic demo mode

Sends basic sensor information through the Serial Monitor

Uses millis() instead of long delays for timing

Software
The project uses:

Arduino framework

PlatformIO

C++
