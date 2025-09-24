#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "audio_types.h"
#include "audio_playback.h"

struct playback_t {
  ma_uint32 sizeInFrames;
  ma_device_config d_config;
  ma_device device;
  ma_pcm_rb ring_buffer;
};

static void data_callback(ma_device *pDevice, void *pOutput, const void *pInput,
                          ma_uint32 frameCount) {
}

/**
 * Create Audio Playback structure.
 *
 * @param sizeInFrames Allocate how many frames to be buffered.
 * @return ma_result enum.
 */
struct playback_t* playback_create(ma_uint32 sizeInFrames) {
  struct playback_t* p = malloc(sizeof(struct playback_t));
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

  return MA_SUCCESS;
}

/**
 * Queue up the next capture data to play.
 *
 * @param s Audio Playback structure.
 * @param cd The structure to use for playback data.
 *  This function moves the data to take ownership.
 *  If the function fails the parameter is not nulled out and the User is
 *  responsible for freeing the data.
 * @return ma_result enum.
 */
ma_result playback_queue(struct playback_t *s, struct capture_data_t **cd) {

  return MA_SUCCESS;
}
