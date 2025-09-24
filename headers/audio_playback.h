#ifndef TINY_VC_AUDIO_PLAYBACK_H
#define TINY_VC_AUDIO_PLAYBACK_H

#include "miniaudio.h"
#include "audio_types.h"

/**
 * Opaque audio playback type.
 */
struct playback_t;

/**
 * Create Audio Playback structure.
 *
 * @param sizeInFrames Allocate how many frames to be buffered.
 * @return ma_result enum.
 */
struct playback_t* playback_create(ma_uint32 sizeInFrames);

/**
 * Destroy Audio playback structure and free internals.
 *
 * @param s Audio Playback structure.
 *  This function nulls out the parameter on success.
 */
void playback_destroy(struct playback_t **s);

/**
 * Trigger the start of the audio playback.
 *
 * @param s Audio Playback structure.
 * @return ma_result enum.
 */
ma_result playback_start(struct playback_t *s);

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
ma_result playback_queue(struct playback_t *s, struct capture_data_t **cd);

#endif
