#ifndef TINY_VC_AUDIO_UTILS_H
#define TINY_VC_AUDIO_UTILS_H

#include "miniaudio.h"
#include <stdbool.h>

/**
 * Get the max sample size for the given format.
 */
double get_max_sample(ma_format format);

/**
 * Check if the system is little endian or not.
 */
bool is_little_endian();

/**
 * Calculate Root Mean Squared (RMS) value for the given input audio.
 *
 * @param[in] input The raw audio data.
 * @param[in] len The lenght of the raw audio buffer.
 * @param[in] format The format the raw data is in.
 * @return The RMS value.
 */
double calculate_rms(const void *input, const size_t len, const ma_format format);

/**
 * Get decibel conversion from the given audio data.
 *
 * @param[in] input The raw audio data.
 * @param[in] frameCount The amount of PCM frames within the raw audio data.
 * @param[in] format The format of the data.
 * @param[in] channels The number of channels.
 * @return The decibel value of the period of audio data. 0 is returned
 *  for errors along with no data.
 */
double audio_get_decibels(const void *input, ma_uint32 frameCount,
                          ma_format format, ma_uint32 channels);

#endif
