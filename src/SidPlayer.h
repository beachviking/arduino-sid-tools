#pragma once

#include "sid.h"

typedef struct{
    uint16_t samplerate;
    int sid_model;
    int clockfreq;
		float framerate;
    unsigned char *song_data;
    int song_length;		
} SidPlayerConfig;

class SidPlayer
{
public:
	SidPlayer(SID *sid) { this->sid = sid;}
	void setDefaultConfig(SidPlayerConfig *cfg);
	void begin(SidPlayerConfig *cfg);
  // void clock();

	inline void setreg(int ofs, int val) { sid->write(ofs, val); }
	inline uint8_t getreg(int ofs) { return sid->read(ofs); }
	void reset(void);
	void stop(void);
	inline bool isPlaying(void) { return playing; }	
	size_t read(uint8_t *buffer, size_t bytes);

	// Provides the frame period in ms
	int framePeriod() { return(frame_period_ms); }
	// Provides the number of samples per frame
	int samplesPerFrame() { return(samples_per_frame); }

private:
	const int SAMPLERATE = 22050;
	const int SID_MODEL = 6581;
	const float PAL_FRAMERATE = 50.0;
	const int CLOCKFREQ = 985248;

	SidPlayerConfig *config;
	
	cycle_count delta_t;					// ratio between system clk and samplerate, ie CLOCKFREQ / SAMPLERATE
	int frame_period_ms = 20;			// raster line time in ms(PAL = 1000/50Hz = 20 ms)
	int samples_per_frame = 441; 	// samplerate / framerate

	volatile bool playing;
	SID *sid;

	// char regbuffer[25];				// current frame sid registers content
	// char prev_regbuffer[25]; 	// previous frame sid registers content
};