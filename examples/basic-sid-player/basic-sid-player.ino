/*
Basic example using the ReSID library with ESP32, using sid register dump data
A single in-memory array with sid register dumps for a tune is applied every 20ms to the sid emulator.
In between these updates, the number of samples needed in between updates given a sample rate is computed.
6502 emulation is not needed for this scheme to work, however this probably limits the number of tunes that 
can be used with this type of player. No CIA timing based tunes will work with this example.

This example also relies on the following libraries to work properly:
  - https://github.com/pschatzmann/arduino-audio-tools
  - https://github.com/pschatzmann/arduino-audiokit

As hardware, an AI ESP32 Audio Kit V2.2 from AliExpress was used. Make sure you set up the arduino-audiokit lib 
correctly by following the instructions for that library.

10/13/2023 beachviking
*/

#include "AudioKitHAL.h"
#include "AudioTools.h"
#include "SidTools.h"

#include "comic.h"

AudioKit kit;
AudioActions actions;

SID sid;
SidRegPlayer player(&sid);
SidRegPlayerConfig sid_cfg;

char buffer[25];
char oldbuffer[25];

const int BUFFER_SIZE = 4 * 882;  // needs to be at least (2 CH * 2 BYTES * SAMPLERATE/PAL_CLOCK). Ex. 4 * (44100/50)
uint8_t audiobuffer[BUFFER_SIZE];

void setup() {
  LOGLEVEL_AUDIOKIT = AudioKitInfo; 
  Serial.begin(115200);

  // open in write mode
  auto cfg = kit.defaultConfig(audiokit::KitOutput);
  //cfg.sample_rate = audio_hal_iface_samples_t::AUDIO_HAL_22K_SAMPLES;
  kit.begin(cfg);

  player.setDefaultConfig(&sid_cfg);
  sid_cfg.samplerate = cfg.sampleRate();
  player.begin(&sid_cfg);
  memset(buffer,0,sizeof(buffer));
  memset(oldbuffer,0,sizeof(oldbuffer));  
}

void loop() {
  static long m = micros();
  static long song_idx = 0;

  if (micros()-m < player.getFramePeriod()) return;
  m = micros();

  // update the total of 25 sid registers, every raster line time (50Hz for PAL)
  for(int i=0;i<25;i++) {
    buffer[i] = Comic_Bakery[song_idx];
    if(buffer[i] != oldbuffer[i]) {
      player.setreg(i, buffer[i]);
      oldbuffer[i] = buffer[i];                  
    }
    ++song_idx;
  }

  if (song_idx >= Comic_Bakery_len)
    song_idx = 0;

  // read samples for this frame
  size_t l = player.read(audiobuffer, player.getSamplesPerFrame());
  kit.write(audiobuffer, l);
}
