#include <stddef.h>
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

#include "audio_capture.h"

#include <stdio.h>
#include <stdlib.h>

struct capture_t {
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
    fprintf(stderr, "failed to acquire write for ring buffer\n");
    return;
  }
  if (local_frame_count != frameCount) {
    fprintf(stderr,
            "frameCount(%d) was higher than available rb_frameCount(%d)\n",
            frameCount, local_frame_count);
  }
  MA_COPY_MEMORY(buffer, pInput,
                 local_frame_count *
                     ma_get_bytes_per_frame(pDevice->capture.format,
                                            pDevice->capture.channels));

  result = ma_pcm_rb_commit_write(&s->ring_buffer, local_frame_count);
  if (result != MA_SUCCESS) {
    fprintf(stderr, "failed to commit write to ring buffer.\n");
    return;
  }
}

ma_result capture_init(struct capture_t *s, ma_uint32 sizeInFrames) {
  if (s == NULL) {
    return MA_NO_DATA_AVAILABLE;
  }
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
    return result;
  }
  result = ma_pcm_rb_init(ma_format_f32,   // format
                          1,               // channels
                          s->sizeInFrames, // size in Frames
                          NULL,            // data to prepopulate
                          NULL,            // allocation callback
                          &s->ring_buffer  // the ring buffer
  );
  if (result != MA_SUCCESS) {
    ma_device_uninit(&s->device);
    return result;
  }
  return result;
}

void capture_free(struct capture_t *s) {
  if (s == NULL) {
    return;
  }
  ma_device_uninit(&s->device);
}

ma_result capture_start(struct capture_t *s) {
  if (s == NULL) {
    return MA_NO_DATA_AVAILABLE;
  }
  return ma_device_start(&s->device);
}

ma_result capture_next_available(struct capture_t *s,
                                 struct capture_data_t *cd) {
  cd->sizeInFrames = s->sizeInFrames;
  void *out_buffer = NULL;
  ma_result result =
      ma_pcm_rb_acquire_read(&s->ring_buffer, &cd->sizeInFrames, &out_buffer);
  if (result != MA_SUCCESS) {
    return result;
  }
  if (cd->sizeInFrames == 0) {
    (void)ma_pcm_rb_commit_read(&s->ring_buffer, 0);
    return MA_NO_DATA_AVAILABLE;
  }
  size_t len =
      (cd->sizeInFrames * ma_get_bytes_per_frame(s->device.capture.format,
                                                 s->device.capture.channels));
  cd->buffer = malloc(sizeof(out_buffer[0]) * len);
  if (cd->buffer == NULL) {
    return MA_NO_ADDRESS;
  }
  MA_COPY_MEMORY(cd->buffer, out_buffer, len);
  cd->channels = s->device.capture.channels;
  cd->format = s->device.capture.format;
  cd->buffer_len = len;
  return ma_pcm_rb_commit_read(&s->ring_buffer, cd->sizeInFrames);
}
