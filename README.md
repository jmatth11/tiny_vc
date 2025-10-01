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

The demo audio is a little choppy at the moment.
I'm trying to figure out if it's related to my auto-config dB threshold algo on the capture side or maybe if I need to buffer the data on the playback side.

https://github.com/user-attachments/assets/89ebbc52-6cb9-45a4-a79c-7e4b2a738324
