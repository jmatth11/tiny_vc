#define MINIAUDIO_IMPLEMENTATION 1
#include "audio_capture.h"
#include "audio_playback.h"
#include "audio_types.h"
#include "miniaudio.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

struct capture_t {
  ma_uint32 sizeInFrames;
  ma_device_config d_config;
  ma_device device;
  ma_pcm_rb ring_buffer;
};

struct playback_t {
  ma_uint32 sizeInFrames;
  ma_device_config d_config;
  ma_device device;
  ma_pcm_rb ring_buffer;
};

static void data_callback(ma_device *pDevice, void *pOutput, const void *pInput,
                          ma_uint32 frameCount) {
  (void)pOutput;
  struct capture_t *s = (struct capture_t *)pDevice->pUserData;
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
            "frameCount(%d) was higher than available rb_frameCount(%d)\n",
            frameCount, local_frame_count);
  }

  ma_copy_pcm_frames(buffer,
                     ma_offset_pcm_frames_const_ptr_f32(
                         (const float *)pInput, 0, pDevice->capture.channels),
                     local_frame_count, pDevice->capture.format,
                     pDevice->capture.channels);
  result = ma_pcm_rb_commit_write(&s->ring_buffer, local_frame_count);
  if (result != MA_SUCCESS) {
    fprintf(stderr,
            "failed to commit write to ring buffer -- error code(%d).\n",
            result);
    return;
  }
}

struct capture_t *capture_create(ma_uint32 sizeInFrames) {
  struct capture_t *s = malloc(sizeof(struct capture_t));
  s->sizeInFrames = sizeInFrames;
  s->d_config = ma_device_config_init(ma_device_type_capture);
  s->d_config.capture.pDeviceID = NULL;
  s->d_config.capture.format = ma_format_f32;
  s->d_config.capture.channels = 1;
  s->d_config.sampleRate = 44100;
  s->d_config.dataCallback = data_callback;
  s->d_config.pUserData = s;
  ma_result result = ma_device_init(NULL, &s->d_config, &s->device);
  if (result != MA_SUCCESS) {
    fprintf(stderr, "miniaudio device init error code(%d)\n", result);
    free(s);
    return NULL;
  }
  result = ma_pcm_rb_init(ma_format_f32,   // format
                          1,               // channels
                          s->sizeInFrames, // size in Frames
                          NULL,            // data to prepopulate
                          NULL,            // allocation callback
                          &s->ring_buffer  // the ring buffer
  );
  if (result != MA_SUCCESS) {
    fprintf(stderr, "miniaudio device init error code(%d)\n", result);
    ma_device_uninit(&s->device);
    free(s);
    return NULL;
  }
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
  local_cd->buffer = malloc(sizeof(out_buffer[0]) * len);
  if (local_cd->buffer == NULL) {
    capture_data_destroy(&local_cd);
    return MA_NO_ADDRESS;
  }
  ma_copy_pcm_frames(local_cd->buffer,
                     ma_offset_pcm_frames_const_ptr_f32(
                         (const float *)out_buffer, 0, s->device.capture.channels),
                     sizeInFrames, s->device.capture.format,
                     s->device.capture.channels);
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
  struct playback_t *p = (struct playback_t *)pDevice->pUserData;
  ma_uint32 frames = frameCount;
  void *buffer = NULL;
  ma_result result = ma_pcm_rb_acquire_read(&p->ring_buffer, &frames, &buffer);
  if (result != MA_SUCCESS) {
    fprintf(stderr,
            "failed to acquire read for ring buffer -- error code(%d).\n",
            result);
    return;
  }
  ma_copy_pcm_frames(pOutput,
                     ma_offset_pcm_frames_const_ptr_f32(
                         (const float *)buffer, 0, pDevice->capture.channels),
                     frames, pDevice->capture.format,
                     pDevice->capture.channels);
  MA_COPY_MEMORY(pOutput, buffer,
                 frames * ma_get_bytes_per_frame(pDevice->playback.format,
                                                 pDevice->playback.channels));
  result = ma_pcm_rb_commit_read(&p->ring_buffer, frames);
  if (result != MA_SUCCESS) {
    fprintf(stderr,
            "failed to commit read for ring buffer -- error code(%d).\n",
            result);
    return;
  }
}

/**
 * Create Audio Playback structure.
 *
 * @param sizeInFrames Allocate how many frames to be buffered.
 * @return ma_result enum.
 */
struct playback_t *playback_create(ma_uint32 sizeInFrames) {
  struct playback_t *p = malloc(sizeof(struct playback_t));
  p->sizeInFrames = sizeInFrames;
  p->d_config = ma_device_config_init(ma_device_type_playback);
  p->d_config.playback.pDeviceID = NULL;
  p->d_config.playback.format = ma_format_f32;
  p->d_config.playback.channels = 1;
  p->d_config.sampleRate = 44100;
  p->d_config.dataCallback = playback_data_callback;
  p->d_config.pUserData = p;
  ma_result result = ma_device_init(NULL, &p->d_config, &p->device);
  if (result != MA_SUCCESS) {
    fprintf(stderr, "miniaudio device init error code(%d)\n", result);
    free(p);
    return NULL;
  }
  result = ma_pcm_rb_init(ma_format_f32,   // format
                          1,               // channels
                          p->sizeInFrames, // size in Frames
                          NULL,            // data to prepopulate
                          NULL,            // allocation callback
                          &p->ring_buffer  // the ring buffer
  );
  if (result != MA_SUCCESS) {
    fprintf(stderr, "miniaudio device init error code(%d)\n", result);
    ma_device_uninit(&p->device);
    free(p);
    return NULL;
  }
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
  ma_copy_pcm_frames(buffer,
                     ma_offset_pcm_frames_const_ptr_f32(
                         (const float *)cd->buffer, 0, cd->channels),
                     frames, cd->format,
                     cd->channels);
  return ma_pcm_rb_commit_write(&s->ring_buffer, frames);
}
