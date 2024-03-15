#pragma once

#include "SidTools.h"
#include "SDFat.h"
#include "mos6502.h"

#define MAX_INSTR 0x100000

struct SidPlayerConfig{
    uint16_t samplerate;
    int sid_model;
    int clockfreq;
		float framerate;
};

struct SIDMetadata {
  char magicID[4];
  uint version;
  uint dataoffset;
  uint loadaddress;
  uint initaddress;
  uint playaddress;
  uint songs;
  uint startsong;
  uint currentsong;
  byte timermode[32];
  char name[32];
  char author[32];
  char released[32];
  uint loadsize;
};

class SidPlayer
{
public:
	SidPlayer(SID *sid);

  int load(StreamFile<FatFile, uint32_t> *currFile);
	void play();
	void playNext();
  void playTune(int subtune);
	void reset(void);
	void stop(void);
  int tick(void);

	inline bool isPlaying(void) { return playing; }	
	size_t read(uint8_t *buffer);

  void setSampleRate(uint32_t rate) { cfg.samplerate = rate; }
  uint32_t getSampleRate() { return(cfg.samplerate); }

	// Provides the current number of samples per frame
	long getSamplesPerFrame() { return(samples_per_frame); }

  SIDMetadata	meta;

private:
	const int SAMPLERATE = 22050;
	const int SID_MODEL = 6581;
	const float PAL_FRAMERATE = 50.0;
	const int CLOCKFREQ = 985248;

	long frame_period_us = 20000;	// raster line time in ms(PAL = 1000/50Hz = 20000 us)
	int samples_per_frame = 441; 	// samplerate / framerate

	SidPlayerConfig cfg;

	cycle_count delta_t;					// ratio between system clk and samplerate, ie CLOCKFREQ / SAMPLERATE

	volatile bool playing;

	SID *sid;
  StreamFile<FatFile, uint32_t> *currfile;

	// Provides/sets the current frame period in us
	long getFramePeriod() { return(frame_period_us); }
	void setFramePeriod(long period_us) {
		frame_period_us = period_us;
    samples_per_frame = frame_period_us / delta_t;
	}
};

SidPlayer::SidPlayer(SID *sid) { 
    this->sid = sid;
    cfg.samplerate = SAMPLERATE;
    cfg.sid_model = SID_MODEL;
    cfg.clockfreq = CLOCKFREQ;
    cfg.framerate = PAL_FRAMERATE;
    // cfg.subtune = 1;      
}

int SidPlayer::load(StreamFile<FatFile, uint32_t> *currFile) {
  const int SidHeaderSize = 126;
  unsigned int header[SidHeaderSize];
  uint32_t loadpos;

  memset(&meta, 0, sizeof(meta));
  memset(&mem, 0, 0x10000);

  // fetch sid header
  currFile->seekSet(0);
  for(int idx = 0; idx < SidHeaderSize; idx++) {
    header[idx] = currFile->read();
  }

  // Read interesting bits of the SID header
  // Big endian format!
  meta.version = (header[4] << 8) | header[5];  
  meta.dataoffset = (header[6] << 8) | header[7];
  meta.loadaddress = (header[8] << 8) | header[9];
  meta.initaddress = (header[10] << 8) | header[11];
  meta.playaddress = (header[12] << 8) | header[13];
  meta.songs = (header[14] << 8) | header[15];
  meta.startsong = (header[16] << 8) | header[17];

  for (int i = 0; i < 32; i++) {
    meta.timermode[31 - i] = (header[0x12 + (i >> 3)] & (byte)pow(2, 7 - i % 8)) ? 1 : 0;
  }

  for (int cc = 0; cc < 4; cc++)
    meta.magicID[cc] = header[cc];

  for (int cc = 0; cc < 31; cc++)
  {
    meta.name[cc] = header[0x16 + cc];
    meta.author[cc] = header[0x36 + cc];
    meta.released[cc] = header[0x56 + cc];
  }

  currFile->seekSet(meta.dataoffset);

  if (meta.loadaddress == 0)
  {
    // Ok, loadaddress is given by the first 2 bytes in the actual sid data
    // Little endian format!
    meta.loadaddress = currFile->read() | (currFile->read() << 8);
  }

  // Load the C64 data
  loadpos = currFile->curPosition();
  meta.loadsize = currFile->size() - loadpos;

  // Print info & run initroutine
  printf("Load address: $%04X Init address: $%04X Play address: $%04X\n", meta.loadaddress, meta.initaddress, meta.playaddress);
  printf("Songs: %02d Start song: %02d \n", meta.songs, meta.startsong);
  printf("Name: %s\n", meta.name);
  printf("Author: %s\n", meta.author);
  printf("Released: %s\n", meta.released);

  printf("Timermodes: ");
  for (int i = 0; i < 32; i++) { printf(" %1d", meta.timermode[31 - i]); }

  printf("\n");

  if (meta.loadsize + meta.loadaddress >= 0x10000)
  {
    printf("Error: SID data continues past end of C64 memory.\n");
    currFile->close();
    return 0;
  }

  // load sid song into memory!
  for (uint32_t i=0; i < meta.loadsize; i++) {
    mem[meta.loadaddress+i] = currFile->read();
  }

  // set default song
  // cfg.subtune = meta.startsong;
  meta.currentsong = meta.startsong;

  return 1;  
}

void SidPlayer::play()
{
  int instr = 0;

	reset();

	sid->set_sampling_parameters(cfg.clockfreq, SAMPLE_FAST, cfg.samplerate);

	delta_t = (int)((uint32_t)cfg.clockfreq / (uint32_t) cfg.samplerate);

	setFramePeriod(cfg.clockfreq / cfg.framerate);
  
  mem[0x01] = 0x37;

  printf("Playing subtune %d\n", meta.currentsong);

  initcpu(meta.initaddress, meta.currentsong-1, 0, 0);

  while (runcpu())
  {
    // Allow SID model detection (including $d011 wait) to eventually terminate
    ++mem[0xd012];
    if (!mem[0xd012] || ((mem[0xd011] & 0x80) && mem[0xd012] >= 0x38))
    {
        mem[0xd011] ^= 0x80;
        mem[0xd012] = 0x00;
    }
    instr++;
    if (instr > MAX_INSTR)
    {
      printf("Warning: CPU executed a high number of instructions in init, breaking\n");
      break;
    }
  }

  if (meta.playaddress == 0)
  {
    printf("Warning: SID has play address 0, reading from interrupt vector instead\n");
    if ((mem[0x01] & 0x07) == 0x5)
      meta.playaddress = mem[0xfffe] | (mem[0xffff] << 8);
    else
      meta.playaddress = mem[0x314] | (mem[0x315] << 8);
    printf("New play address is $%04X\n", meta.playaddress);
  } else {
    if (meta.playaddress >= 0xe000 && mem[1] == 0x37)
      mem[1]=0x35;
  }

  if (meta.timermode[meta.currentsong-1] || mem[0xdc05]) { //CIA timing
    if (!mem[0xdc05]) { mem[0xdc04] = 0x24; mem[0xdc05] = 0x40; } //C64 startup-default
    setFramePeriod(mem[0xdc04] + mem[0xdc05] * 256);
  }
  else setFramePeriod(cfg.clockfreq / cfg.framerate);  //Vsync timing

	printf("cpu_clk: %ld samplerate: %ld samples/frame: %ld frame period: %ld delta_t: %ld timing: %d\n", \
          cfg.clockfreq,        \
          cfg.samplerate,       \
          samples_per_frame, \
          frame_period_us,     \
          delta_t,              \
          meta.timermode[meta.currentsong-1]);

	playing = true;
}

void SidPlayer::playNext() {
  meta.currentsong = (meta.currentsong == meta.songs) ? 1 : (meta.currentsong+1);
  play();
}

void SidPlayer::playTune(int subtune) {
  if (subtune < 1 || subtune > meta.songs)
    return;

  meta.currentsong = subtune;
  play();
}

void SidPlayer::reset(void)
{
	sid->reset();

  // reset sid's memory mapped registers too
  for (int reg = 0; reg < 25; reg++)
    mem[0xd400 + reg] = 0;

}

void SidPlayer::stop(void)
{
	playing = false;	
}

int SidPlayer::tick(void)
{
  // Run the playroutine
  int instr = 0;
  initcpu(meta.playaddress, 0, 0, 0);
  while (runcpu())
  {
    instr++;
    if (instr > MAX_INSTR)
    {
      printf("Error: CPU executed abnormally high amount of instructions in playroutine, exiting\n");
      playing = false;
      return 1;
    }
    // Test for jump into Kernal interrupt handler exit
    if ((mem[0x01] & 0x07) != 0x5 && (pc == 0xea31 || pc == 0xea81))
      break;
  }

  // update sid with the latest values
  for (int reg = 0; reg < 25; reg++)
    sid->write(reg, mem[0xd400 + reg]);

  // // check timing, update samples_per_frame as needed.
  if ((mem[1]&3) && meta.timermode[meta.currentsong-1])
    setFramePeriod((mem[0xdc05] << 8) | mem[0xdc04]); // use dynamic CIA settings

  return 0;
}

/// fill the data with 2 channels
size_t SidPlayer::read(uint8_t *buffer)
{
  size_t result = 0;

  if (!playing)
    return result;

  int16_t *ptr = (int16_t *)buffer;
  for (int j = 0; j < samples_per_frame; j++)	
  {
		sid->clock(delta_t);
    int16_t sample = sid->output();
    *ptr++ = sample;
    *ptr++ = sample;
    result += 4;
  }
  return result;
}
