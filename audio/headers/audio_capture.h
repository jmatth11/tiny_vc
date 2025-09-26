#ifndef TINY_VC_AUDIO_CAPTURE_H
#define TINY_VC_AUDIO_CAPTURE_H

#include "audio_types.h"
#include <stddef.h>

/**
 * Opaque audio capture type.
 */
struct capture_t;

/**
 * Create Audio Capture structure.
 *
 * @param sizeInFrames Allocate how many frames to be buffered.
 * @return Newly created capture structure, null on error.
 */
struct capture_t* capture_create(ma_uint32 sizeInFrames);

/**
 * Destroy Audio capture structure and free internals.
 *
 * @param s Audio Capture structure.
 *  This function nulls the parameter out on success.
 */
void capture_destroy(struct capture_t **s);

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
 * @param cd The capture data pointer to populate.
 *  This function will create this structure. User is responsible for
 *  freeing the buffer. See capture_data_destroy.
 * @return ma_result enum.
 */
ma_result capture_next_available(struct capture_t *s, struct capture_data_t **cd);

#endif
