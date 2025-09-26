#include <stdio.h>
#include <signal.h>

#include "audio_capture.h"
#include "audio_playback.h"
#include "audio_types.h"

const ma_uint32 frame_count = (1102 * 4);
static int running = 1;

void sigint_handler(int signo) {
  if (signo == SIGINT) {
    running = 0;
  }
}

int main(void) {
  struct capture_t *cap = capture_create(frame_count);
  struct playback_t *play = playback_create(frame_count);
  if (cap == NULL || play == NULL) {
    fprintf(stderr, "creation failed.\n");
    return -1;
  }
  capture_start(cap);
  playback_start(play);

  while (running) {
    struct capture_data_t *data = NULL;
    ma_result result = capture_next_available(cap, &data);
    if (result != MA_SUCCESS && result != MA_NO_DATA_AVAILABLE) {
      fprintf(stderr, "next_available failed: %d\n", result);
      running = 0;
      break;
    }
    if (data != NULL) {
      result = playback_queue(play, data);
      if (result != MA_SUCCESS && result != MA_NO_DATA_AVAILABLE) {
        fprintf(stderr, "playback_queue failed: %d\n", result);
        running = 0;
      }
      capture_data_destroy(&data);
    } else {
        //fprintf(stderr, "capture data was null\n");
    }
  }

  capture_destroy(&cap);
  playback_destroy(&play);
  return 0;
}
