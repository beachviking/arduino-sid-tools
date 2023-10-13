# esp32-resid
A version of ReSID for ESP32, based off the original ReSID implementation by Dag Lem.

## Installation in Arduino
You can download the library as zip and call include Library -> zip library. Or you can git clone this project into the Arduino libraries folder e.g. with

```
cd  ~/Documents/Arduino/libraries
git clone https://github.com/beachviking/arduino-esp32-resid.git
```

I recommend to use git because you can easily update to the latest version just by executing the ```git pull``` command in the project folder.

## Dependencies
The library and the examples also rely on the following libraries to work properly:
  - https://github.com/pschatzmann/arduino-audio-tools
  - https://github.com/pschatzmann/arduino-audiokit

As hardware, an AI ESP32 Audio Kit V2.2 from AliExpress was used.
