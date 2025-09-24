#include "audio_types.h"
#include <stdint.h>
#include <stdlib.h>

struct capture_data_t* capture_data_create() {
  struct capture_data_t* local = malloc(sizeof(struct capture_data_t));
  local->sizeInFrames = 0;
  local->channels = 0;
  local->format = ma_format_unknown;
  local->buffer_len = 0;
  local->buffer = NULL;
  return local;
}

void capture_data_destroy(struct capture_data_t **cd) {
  if (cd == NULL) {
    return;
  }
  if ((*cd) == NULL) {
    return;
  }
  if ((*cd)->buffer != NULL) {
    free((*cd)->buffer);
  }
  free(*cd);
  *cd = NULL;
}
