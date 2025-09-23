#ifndef TINY_VC_AUDIO_CAPTURE_H
#define TINY_VC_AUDIO_CAPTURE_H

#include "miniaudio.h"
#include <stddef.h>

/**
 * Opaque audio capture type.
 */
struct capture_t;

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
 * Initialize Audio Capture structure.
 *
 * @param s Audio Capture structure.
 * @param sizeInFrames Allocate how many frames to be buffered.
 * @return ma_result enum.
 */
ma_result capture_init(struct capture_t *s, ma_uint32 sizeInFrames);

/**
 * Free Audio capture structure's internals.
 *
 * @param s Audio Capture structure.
 */
void capture_free(struct capture_t *s);

/**
 * Trigger the start of the audio capture.
 *
 * @param s Audio Capture structure.
 * @return ma_result enum.
 */
ma_result capture_start(struct capture_t *s);

/**
 * Get the next available captured data.
 *
 * @param s Audio Capture structure.
 * @param cd The structure to populate with the capture data.
 *  This function will initialize this structure. User is responsible for
 *  freeing the buffer.
 * @return ma_result enum.
 */
ma_result capture_next_available(struct capture_t *s, struct capture_data_t *cd);

#endif
