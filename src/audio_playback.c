#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "audio_playback.h"
#include "audio_types.h"

struct playback_t {
  ma_uint32 sizeInFrames;
  ma_device_config d_config;
  ma_device device;
  ma_pcm_rb ring_buffer;
};

static void data_callback(ma_device *pDevice, void *pOutput, const void *pInput,
                          ma_uint32 frameCount) {
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
  p->d_config.dataCallback = data_callback;
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
  MA_COPY_MEMORY(buffer, cd->buffer,
                 frames * ma_get_bytes_per_frame(cd->format, cd->channels));
  return ma_pcm_rb_commit_write(&s->ring_buffer, frames);
}
