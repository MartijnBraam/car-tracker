# GPS Car tracker

Create gps track files for car rides and upload data to a server through WiFi.

## Hardware

* esp8266 mode (esp 12-e) on a nodemcu board.
* ublox neo-6m gps module
* SD card breakout

## Features

* Use 12v line voltage detection to detect if the engine is running without extra wires.
* Write gps log files to the SD card for each trip (seperated by engine power off)
* After shutting of the car engine the module tries to connect to nearby WiFi to upload the new tracks.
