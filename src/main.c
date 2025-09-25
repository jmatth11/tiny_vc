#include <stdio.h>
#include <signal.h>

#include "audio_capture.h"
#include "audio_playback.h"
#include "audio_types.h"

static int running = 1;

void sigint_handler(int signo) {
  if (signo == SIGINT) {
    running = 0;
  }
}

int main(void) {
  struct capture_t *cap = capture_create(4);
  struct playback_t *play = playback_create(4);
  if (cap == NULL || play == NULL) {
    fprintf(stderr, "creation failed.\n");
    return -1;
  }
  capture_start(cap);
  playback_start(play);

  while (running) {
    struct capture_data_t *data = NULL;
    ma_result result = capture_next_available(cap, &data);
    if (result != MA_SUCCESS) {
      fprintf(stderr, "next_available failed: %d\n", result);
      running = 0;
      break;
    }
    result = playback_queue(play, data);
    if (result != MA_SUCCESS) {
      fprintf(stderr, "playback_queue failed: %d\n", result);
      running = 0;
    }
    capture_data_destroy(&data);
  }

  capture_destroy(&cap);
  playback_destroy(&play);
  return 0;
}
