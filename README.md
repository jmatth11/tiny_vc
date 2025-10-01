# Tiny VC

A simple tiny voice chat program written in Zig and C.

This is an experimentation into audio processing and voip style programs.

## Structure

- `audio/` - The C portion of the code. Uses miniaudio to handle device
  capture/playback.
- `src/` - The Zig portion of the code. The main application and handling of
  the message bus.

## CLI Flags

- `--ip` - The IP of the message bus.
- `-p`|`--port` - The port of the message bus.
- `-t`|`--topic` - The topic on the message bus to subscribe to.
- `--capture_only` - Flag to run the application in capture only mode.
- `--playback_only` - Flag to run the application in playback only mode.

## Demo


