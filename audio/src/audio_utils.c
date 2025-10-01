#include "audio_utils.h"
#include <limits.h>
#include <math.h>
#include <stdint.h>

size_t get_max_sample(ma_format format) {
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

/**
 * determine endianness.
 */
bool is_little_endian() {
  uint16_t tmp = 1;
  const uint8_t *raw = (const uint8_t *)&tmp;
  return raw[0] == 1;
}

/**
 * Calculate Root Mean Squared (RMS) value for the given input audio.
 * Data Type: uint8_t.
 */
static inline double rms_uint8(const void *input, const size_t len) {
  double volume = 0;
  const uint8_t *raw_data = (const uint8_t *)input;
  for (size_t i = 0; i < len; i++) {
    volume += (double)raw_data[i] * (double)raw_data[i];
  }
  volume = volume / (double)len;
  volume = sqrt(volume);
  return volume;
}
/**
 * Calculate Root Mean Squared (RMS) value for the given input audio.
 * Data Type: int16_t.
 */
static inline double rms_int16(const void *input, const size_t len) {
  double volume = 0;
  const int16_t *raw_data = (const int16_t *)input;
  for (size_t i = 0; i < len; i++) {
    volume += (double)raw_data[i] * (double)raw_data[i];
  }
  volume = volume / (double)len;
  volume = sqrt(volume);
  return volume;
}
/**
 * Calculate Root Mean Squared (RMS) value for the given input audio.
 * Data Type: int24_t.
 */
static inline double rms_int24(const void *input, const size_t len) {
  double volume = 0;
  const uint8_t *raw_data = (const uint8_t *)input;
  for (size_t i = 0; i <= (len - 2); i += 3) {
    int32_t value = 0;
    if (is_little_endian()) {
      value = (((int32_t)raw_data[i])) | (((int32_t)raw_data[i + 1]) << 8) |
              (((int32_t)raw_data[i + 2]) << 16);
    } else {
      value = (((int32_t)raw_data[i]) << 16) | (((int32_t)raw_data[i + 1]) << 8) |
              (((int32_t)raw_data[i + 2]));
    }
    volume += (double)value * (double)value;
  }
  volume = volume / (double)len;
  volume = sqrt(volume);
  return volume;
}
/**
 * Calculate Root Mean Squared (RMS) value for the given input audio.
 * Data Type: int32_t.
 */
static inline double rms_int32(const void *input, const size_t len) {
  double volume = 0;
  const int32_t *raw_data = (const int32_t *)input;
  for (size_t i = 0; i < len; i++) {
    volume += (double)raw_data[i] * (double)raw_data[i];
  }
  volume = volume / (double)len;
  volume = sqrt(volume);
  return volume;
}

double calculate_rms(const void *input, const size_t len, const ma_format format) {
  switch (format) {
  case ma_format_u8: {
    return rms_uint8(input, len);
  }
  case ma_format_s16: {
    return rms_int16(input, len);
  }
  case ma_format_s24: {
    return rms_int24(input, len);
  }
  case ma_format_s32: {
    return rms_int32(input, len);
  }
  default: {
    return 0.0;
  }
  }
}

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
                          ma_format format, ma_uint32 channels) {
  const size_t data_len = frameCount * ma_get_bytes_per_frame(format, channels);
  if (data_len == 0) {
    return 0.0;
  }
  double volume = calculate_rms(input, data_len, format);
  // convert to decimals
  // https://en.wikipedia.org/wiki/DBFS
  const double max_sample = get_max_sample(format);
  if (max_sample == 0) {
    return 0.0;
  }
  return 20.0 * log10(volume / (double)get_max_sample(format));
}
