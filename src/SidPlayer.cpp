#include "SidPlayer.h"

void SidPlayer::setDefaultConfig(SidPlayerConfig *cfg) {
  cfg->samplerate = SAMPLERATE;
  cfg->sid_model = SID_MODEL;
  cfg->clockfreq = CLOCKFREQ;
	cfg->framerate = PAL_FRAMERATE;
}

// void SidPlayer::clock()
// {
// 	static long song_idx = 0;

//   // update sid registers
//   for(int i=0;i<25;i++) {
//     regbuffer[i] = config->song_data[song_idx];
//     if(regbuffer[i] != prev_regbuffer[i]) {
//       setreg(i, regbuffer[i]);
//       prev_regbuffer[i] = regbuffer[i];                  
//     }
//     ++song_idx;
//   }

//   if (song_idx >= config->song_length)
//     song_idx = 0;
// }

void SidPlayer::begin(SidPlayerConfig *cfg)
{
	config = cfg;
	this->reset();
	sid->set_sampling_parameters(config->clockfreq, SAMPLE_FAST, config->samplerate); 
	delta_t = round((float)config->clockfreq / ((float)config->samplerate));

	samples_per_frame = round((float)config->samplerate / ((float)cfg->framerate));
	frame_period_ms = 1000 / cfg->framerate;

  // memset(regbuffer,0,sizeof(regbuffer));
  // memset(prev_regbuffer,0,sizeof(prev_regbuffer));

	printf("clockfreq: %d\n", this->config->clockfreq);
	printf("samplerate: %d\n", this->config->samplerate);
	printf("samples per frame: %d\n", samplesPerFrame());
	printf("frame period: %d ms\n", framePeriod());
	printf("delta_t: %d\n", delta_t);

	playing = true;
}

void SidPlayer::reset(void)
{
	sid->reset();
}

void SidPlayer::stop(void)
{
	playing = false;	
}

/// fill the data with 2 channels
size_t SidPlayer::read(uint8_t *buffer, size_t bytes)
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
