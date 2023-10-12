#include "AudioKitHAL.h"
#include "AudioTools.h"
#include "SidPlayer.h"

#include "comic.h"

#define NEXT_TUNE_BUTTON_PIN  5
#define NEXT_SONG_BUTTON_PIN  18

// #define DEBUGMODE

AudioKit kit;
AudioActions actions;

SID sid;
SidPlayer player(&sid);
SidPlayerConfig sid_cfg;

char buffer[25];
char oldbuffer[25];

const int BUFFER_SIZE = 4 * 882;  // needs to be at least (2 CH * 2 BYTES * SAMPLERATE/PAL_CLOCK). Ex. 4 * (44100/50)
uint8_t audiobuffer[BUFFER_SIZE];

// void debugRun(int numframes) {
//   int song_idx = 0;
//   char output[128];
//   printf("| Frame | 00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F 10 11 12 13 14 15 16 17 18 |");
//   printf("\n");
//   printf("+-------+---------------------+--------------------+--------------------+------------+");
//   printf("\n");
//   for(int frame=0; frame < numframes; frame++)
//   {
//     output[0] = 0;
//     sprintf(&output[strlen(output)], "| %5d | ", frame);
//     for(int i=0;i<25;i++) {
//       buffer[i] = Comic_Bakery[song_idx];
//       if(buffer[i] != oldbuffer[i]) {
//         player.setreg(i, buffer[i]);
//         sprintf(&output[strlen(output)], "%02X ", buffer[i]);
//         oldbuffer[i] = buffer[i];                  
//       }
//       else
//       {
//         sprintf(&output[strlen(output)], ".. ");
//       }
//       ++song_idx;
//     }
//     printf("%s|\n", output);
//     printf("+-------+---------------------+--------------------+--------------------+------------+");
//     printf("\n");
//     // print samples
//     size_t l = player.read(audiobuffer, BUFFER_SIZE);
//     for(int k = 0; k < BUFFER_SIZE/4; k+=4 ) {
//       output[0] = 0;
//       sprintf(&output[strlen(output)], "| %5d | ", k);
//       sprintf(&output[strlen(output)], "%02X %02X %02X %02X ", audiobuffer[k], audiobuffer[k+1], audiobuffer[k+2], audiobuffer[k+3]);
//       printf("%s|\n", output);
//     }
//     printf("+-------+---------------------+--------------------+--------------------+------------+");
//     printf("\n");
//   }

//   printf("Simulation done.\n");
//   while(1==1);
// }

void setup() {
  LOGLEVEL_AUDIOKIT = AudioKitInfo; 
  Serial.begin(115200);
  // open in write mode
  auto cfg = kit.defaultConfig(audiokit::AudioOutput);
  //cfg.sample_rate = audio_hal_iface_samples_t::AUDIO_HAL_22K_SAMPLES;
  kit.begin(cfg);

  player.setDefaultConfig(&sid_cfg);
  sid_cfg.samplerate = cfg.sampleRate();
  // sid_cfg.song_data = (unsigned char*)Comic_Bakery;
  // sid_cfg.song_length = Comic_Bakery_len;
  player.begin(&sid_cfg);
  memset(buffer,0,sizeof(buffer));
  memset(oldbuffer,0,sizeof(oldbuffer));  
}

void loop() {
  static int m = millis();
  static long song_idx = 0;

  if (millis()-m < player.framePeriod()) return;
  m = millis();

  // update sid registers, raster line time (50Hz)
  // player.clock();

  // update sid registers, raster line time (50Hz)
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
  size_t l = player.read(audiobuffer, BUFFER_SIZE);
  kit.write(audiobuffer, l);
  // actions.processActions();
}
