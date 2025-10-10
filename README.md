# Tiny VC

A simple tiny voice chat program written in Zig and C.

This is an experimentation into audio processing and voip style programs.

## Dependencies

Languages:
- c11
- Zig v0.15.1

External Packages:
- [miniaudio](https://github.com/mackron/miniaudio) - for device capture and playback.
- [chebi](https://github.com/jmatth11/chebi) - simple message bus written in zig.

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

Simple demo of running a playback_only and capture_only programs sending audio over my message bus.
Using a ring buffer to prevent choppy audio.

https://github.com/user-attachments/assets/0bbaebc0-5c5d-4949-b16f-e91eee3ea318


