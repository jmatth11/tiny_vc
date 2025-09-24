#ifndef TINY_VC_AUDIO_TYPES_H
#define TINY_VC_AUDIO_TYPES_H

#include "miniaudio.h"
#include <stddef.h>

/**
 * Captured data in frames.
 */
struct capture_data_t {
  /* Size in Frames. */
  ma_uint32 sizeInFrames;
  /* Format of the data. */
  ma_format format;
  /* Number of channels in the data. */
  ma_uint32 channels;
  /* Size of the buffer. */
  size_t buffer_len;
  /* Buffer of PCM frame data. */
  void* buffer;
};

/**
 * Create a capture data structure.
 *
 * @return Newly created capture data, null on error.
 */
struct capture_data_t* capture_data_create();

/**
 * Destroy the capture data.
 * The passed in capture data pointer is nulled on success.
 *
 * @param cd The capture data.
 */
void capture_data_destroy(struct capture_data_t **cd);

#endif
