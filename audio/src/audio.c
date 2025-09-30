#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <string.h>
#define MINIAUDIO_IMPLEMENTATION 1
#include "audio_capture.h"
#include "audio_playback.h"
#include "audio_types.h"
#include "miniaudio.h"

#include <float.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

struct capture_t {
  ma_uint32 periodSize;
  ma_uint32 sizeInFrames;
  ma_device_config d_config;
  ma_device device;
  ma_pcm_rb ring_buffer;
};

struct playback_t {
  ma_uint32 periodSize;
  ma_uint32 sizeInFrames;
  ma_device_config d_config;
  ma_device device;
  ma_pcm_rb ring_buffer;
};

static size_t get_max_sample(ma_format format) {
  switch (format) {
  case ma_format_s32: {
    return INT_MAX;
  }
  case ma_format_s24: {
    return 8388607;
  }
  case ma_format_s16: {
    return 32767;
  }
  case ma_format_u8: {
    return 255;
  }
  default: {
    return 0;
  }
  }
}

#define FORMAT_TYPE int16_t
const ma_format STD_FORMAT = ma_format_s16;
static double CAP_THRESHOLD = -13.0;
static double PLAY_THRESHOLD = -7.0;
static double cap_sample_counter = 0;
static double play_sample_counter = 0;

static void data_callback(ma_device *pDevice, void *pOutput, const void *pInput,
                          ma_uint32 frameCount) {
  (void)pOutput;
  struct capture_t *s = (struct capture_t *)pDevice->pUserData;
  FORMAT_TYPE *raw_data = (FORMAT_TYPE *)pInput;
  const size_t data_len =
      frameCount * ma_get_bytes_per_frame(pDevice->capture.format,
                                          pDevice->capture.channels);
  if (data_len == 0) {
    return;
  }
  double volume = 0;
  for (size_t i = 0; i < data_len; i++) {
    volume += (double)raw_data[i] * (double)raw_data[i];
  }
  volume = volume / (double)data_len;
  volume = sqrt(volume);
  // convert to decimals
  // https://en.wikipedia.org/wiki/DBFS
  const double dBFS =
      20 * log10(volume / (double)get_max_sample(pDevice->capture.format));
  // decibels must be certain level before we process it
  printf("dBFS = %f\n", dBFS);
  if (cap_sample_counter < 10) {
    cap_sample_counter++;
    CAP_THRESHOLD += dBFS;
    if (cap_sample_counter == 10) {
      CAP_THRESHOLD = CAP_THRESHOLD / 10.0;
    }
    return;
  } else if (dBFS < CAP_THRESHOLD) {
    return;
  }
  void *buffer = NULL;
  ma_uint32 local_frame_count = frameCount;
  ma_result result =
      ma_pcm_rb_acquire_write(&s->ring_buffer, &local_frame_count, &buffer);
  if (result != MA_SUCCESS) {
    fprintf(stderr,
            "failed to acquire write for ring buffer -- error code(%d).\n",
            result);
    return;
  }
  if (local_frame_count != frameCount) {
    fprintf(stderr,
            "frameCount(%d) was higher than available rb_frameCount(%d) -- "
            "dropping data.\n",
            frameCount, local_frame_count);
    (void)ma_pcm_rb_commit_write(&s->ring_buffer, local_frame_count);
    return;
  }

  ma_copy_pcm_frames(buffer, pInput, local_frame_count, pDevice->capture.format,
                     pDevice->capture.channels);
  result = ma_pcm_rb_commit_write(&s->ring_buffer, local_frame_count);
  if (result != MA_SUCCESS) {
    fprintf(stderr,
            "failed to commit write to ring buffer -- error code(%d).\n",
            result);
    return;
  }
}

struct capture_t *capture_create(ma_uint32 periodSize) {
  struct capture_t *s = malloc(sizeof(struct capture_t));
  s->periodSize = periodSize;
  s->d_config = ma_device_config_init(ma_device_type_capture);
  s->d_config.capture.pDeviceID = NULL;
  s->d_config.capture.format = STD_FORMAT;
  s->d_config.capture.channels = 1;
  s->d_config.sampleRate = 44100;
  s->d_config.dataCallback = data_callback;
  s->d_config.pUserData = s;
  ma_result result = ma_device_init(NULL, &s->d_config, &s->device);
  if (result != MA_SUCCESS) {
    fprintf(stderr, "capture: miniaudio device init error code(%d)\n", result);
    free(s);
    return NULL;
  }
  s->sizeInFrames = s->device.capture.internalPeriodSizeInFrames;
  printf("sizeInFrames = %d\n", s->sizeInFrames);
  result = ma_pcm_rb_init(STD_FORMAT,                   // format
                          1,                            // channels
                          s->sizeInFrames * periodSize, // size in Frames
                          NULL,                         // data to prepopulate
                          NULL,                         // allocation callback
                          &s->ring_buffer               // the ring buffer
  );
  if (result != MA_SUCCESS) {
    fprintf(stderr, "capture: miniaudio ring buffer init error code(%d)\n",
            result);
    ma_device_uninit(&s->device);
    free(s);
    return NULL;
  }
  ma_pcm_rb_set_sample_rate(&s->ring_buffer, s->d_config.sampleRate);
  return s;
}

void capture_destroy(struct capture_t **s) {
  if (s == NULL) {
    return;
  }
  if ((*s) == NULL) {
    return;
  }
  ma_device_uninit(&(*s)->device);
  ma_pcm_rb_uninit(&(*s)->ring_buffer);
  free(*s);
  *s = NULL;
}

ma_result capture_start(struct capture_t *s) {
  if (s == NULL) {
    return MA_NO_DATA_AVAILABLE;
  }
  return ma_device_start(&s->device);
}

ma_result capture_next_available(struct capture_t *s,
                                 struct capture_data_t **cd) {
  ma_uint32 sizeInFrames = s->sizeInFrames;
  void *out_buffer = NULL;
  ma_result result =
      ma_pcm_rb_acquire_read(&s->ring_buffer, &sizeInFrames, &out_buffer);
  if (result != MA_SUCCESS) {
    return result;
  }
  if (sizeInFrames == 0) {
    (void)ma_pcm_rb_commit_read(&s->ring_buffer, 0);
    return MA_NO_DATA_AVAILABLE;
  }
  size_t len =
      (sizeInFrames * ma_get_bytes_per_frame(s->device.capture.format,
                                             s->device.capture.channels));
  struct capture_data_t *local_cd = capture_data_create();
  local_cd->sizeInFrames = sizeInFrames;
  local_cd->buffer = malloc(len);
  if (local_cd->buffer == NULL) {
    capture_data_destroy(&local_cd);
    (void)ma_pcm_rb_commit_read(&s->ring_buffer, local_cd->sizeInFrames);
    return MA_NO_ADDRESS;
  }
  ma_copy_pcm_frames(local_cd->buffer, out_buffer, sizeInFrames,
                     s->device.capture.format, s->device.capture.channels);
  local_cd->channels = s->device.capture.channels;
  local_cd->format = s->device.capture.format;
  local_cd->buffer_len = len;
  *cd = local_cd;
  return ma_pcm_rb_commit_read(&s->ring_buffer, local_cd->sizeInFrames);
}

/***********************************************************************************
 *
 *
 *
 * Playback functionality.
 *
 *
 *
 * *********************************************************************************
 */

static void playback_data_callback(ma_device *pDevice, void *pOutput,
                                   const void *pInput, ma_uint32 frameCount) {
  (void)pInput;
  (void)frameCount;
  struct playback_t *p = (struct playback_t *)pDevice->pUserData;
  ma_uint32 frames = p->sizeInFrames * p->periodSize;
  void *buffer = NULL;
  ma_result result = ma_pcm_rb_acquire_read(&p->ring_buffer, &frames, &buffer);
  if (result != MA_SUCCESS || buffer == NULL) {
    fprintf(stderr,
            "failed to acquire read for ring buffer -- error code(%d).\n",
            result);
    return;
  }
  const size_t data_len =
      frames * ma_get_bytes_per_frame(pDevice->playback.format,
                                          pDevice->playback.channels);
  if (data_len == 0) {
    (void)ma_pcm_rb_commit_read(&p->ring_buffer, frames);
    return;
  }
  void *raw_data = malloc(data_len);
  if (raw_data == NULL) {
    fprintf(stderr, "ran out of memory.\n");
    return;
  }
  memcpy(raw_data, buffer, data_len);
  result = ma_pcm_rb_commit_read(&p->ring_buffer, frames);
  if (result != MA_SUCCESS) {
    fprintf(stderr,
            "failed to commit read for ring buffer -- error code(%d).\n",
            result);
    free(raw_data);
    return;
  }
  FORMAT_TYPE *cast_data = (FORMAT_TYPE*)raw_data;
  double volume = 0;
  for (size_t i = 0; i < data_len; i++) {
    volume += (double)cast_data[i] * (double)cast_data[i];
  }
  volume = volume / (double)data_len;
  volume = sqrt(volume);
  // convert to decimals
  // https://en.wikipedia.org/wiki/DBFS
  const double dBFS =
      20 * log10(volume / (double)get_max_sample(pDevice->playback.format));
  // decibels must be certain level before we process it
  printf("dBFS = %f\n", dBFS);
  if (play_sample_counter < 10) {
    play_sample_counter++;
    PLAY_THRESHOLD += dBFS;
    if (play_sample_counter == 10) {
      PLAY_THRESHOLD = PLAY_THRESHOLD / 10.0;
    }
    free(raw_data);
    return;
  } else if (dBFS == INFINITY || dBFS < PLAY_THRESHOLD) {
    free(raw_data);
    return;
  }
  ma_copy_pcm_frames(pOutput, raw_data, frames, pDevice->playback.format,
                     pDevice->playback.channels);
  free(raw_data);
}

struct playback_t *playback_create(ma_uint32 periodSize) {
  struct playback_t *p = malloc(sizeof(struct playback_t));
  p->periodSize = periodSize;
  p->d_config = ma_device_config_init(ma_device_type_playback);
  p->d_config.playback.pDeviceID = NULL;
  p->d_config.playback.format = STD_FORMAT;
  p->d_config.playback.channels = 1;
  p->d_config.sampleRate = 44100;
  p->d_config.dataCallback = playback_data_callback;
  p->d_config.pUserData = p;
  ma_result result = ma_device_init(NULL, &p->d_config, &p->device);
  if (result != MA_SUCCESS) {
    fprintf(stderr, "playback: miniaudio device init error code(%d)\n", result);
    free(p);
    return NULL;
  }
  p->sizeInFrames = p->device.playback.internalPeriodSizeInFrames;
  printf("sizeInFrames = %d\n", p->sizeInFrames);
  result = ma_pcm_rb_init(STD_FORMAT,                   // format
                          1,                            // channels
                          p->sizeInFrames * periodSize, // size in Frames
                          NULL,                         // data to prepopulate
                          NULL,                         // allocation callback
                          &p->ring_buffer               // the ring buffer
  );
  if (result != MA_SUCCESS) {
    fprintf(stderr, "playback: miniaudio ring buffer init error code(%d)\n",
            result);
    ma_device_uninit(&p->device);
    free(p);
    return NULL;
  }
  ma_pcm_rb_set_sample_rate(&p->ring_buffer, p->d_config.sampleRate);
  return p;
}

/**
 * Destroy Audio playback structure and free internals.
 *
 * @param s Audio Playback structure.
 *  This function nulls out the parameter on success.
 */
void playback_destroy(struct playback_t **s) {
  if (s == NULL) {
    return;
  }
  if ((*s) == NULL) {
    return;
  }
  ma_device_uninit(&(*s)->device);
  ma_pcm_rb_uninit(&(*s)->ring_buffer);
  free(*s);
  *s = NULL;
}

/**
 * Trigger the start of the audio playback.
 *
 * @param s Audio Playback structure.
 * @return ma_result enum.
 */
ma_result playback_start(struct playback_t *s) {
  return ma_device_start(&s->device);
}

/**
 * Queue up the next capture data to play.
 *
 * @param s Audio Playback structure.
 * @param cd The structure to use for playback data.
 * @return ma_result enum.
 */
ma_result playback_queue(struct playback_t *s, struct capture_data_t *cd) {
  ma_uint32 frames = cd->sizeInFrames;
  void *buffer = NULL;
  ma_result result = ma_pcm_rb_acquire_write(&s->ring_buffer, &frames, &buffer);
  if (result != MA_SUCCESS) {
    fprintf(stderr,
            "failed to acquire write for ring buffer -- error code(%d).\n",
            result);
    return result;
  }
  if (frames != cd->sizeInFrames) {
    fprintf(stderr, "playback: incoming frames (%d); ring_buffer frames (%d)\n",
            cd->sizeInFrames, frames);
  }
  ma_copy_pcm_frames(buffer, cd->buffer, frames, cd->format, cd->channels);
  return ma_pcm_rb_commit_write(&s->ring_buffer, frames);
}
