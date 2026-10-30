# LamaSync AudIO Windows bridge

LamaSync AudIO is a C++17 Windows console application that connects the standalone IGNITE
AudIO engine to a small text command protocol. It can generate a 220 Hz stereo test tone at
48 kHz and can receive the same commands from the terminal or from an ESP32-S3 USB serial
port.

This is a prototype bridge, not a full sequencer, MIDI player, Reason Remote integration or
ESP32 audio implementation.

## Supported Windows build

- Windows 10/11 x64
- Visual Studio 2022 or newer Build Tools
- Desktop development with C++, a Windows SDK and CMake

From PowerShell:

```powershell
cd LamaSyncBridge
.\build-windows.ps1
```

The script discovers CMake from `PATH` or from the newest installed Visual Studio instance,
builds Release x64, runs both automated tests and writes:

- `build-win\bin\Release\LamaSyncAudIO.exe`
- `build-win\audio-test\lamasync-220hz-48k.wav`

Use `-Configuration Debug` for a debug build and `-Clean` only when you intentionally want
to remove the bridge's existing `build-win` directory before rebuilding.

## Reproducible tests

`LamaSyncProtocolTest` validates parsing and transport state for `START`, `STOP`, `LOOP`,
`STATUS`, `QUIT` and invalid commands.

`LamaSyncAudioRenderTest` exercises the AudIO `SineWave` and `Design` processing path without
requiring a sound card. It renders one second of stereo PCM, checks the 48,000 sample count,
measures approximately 220 Hz, checks RMS level and channel equality, and writes a standard
48 kHz/16-bit stereo WAV file. This proves deterministic DSP/rendering, not audible playback
through a particular Windows driver or loudspeaker.

## Commands

| Command | Reply | Effect |
|---|---|---|
| `START` | `ACK START` | Resume the test tone |
| `STOP` | `ACK STOP` | Pause the test tone |
| `LOOP ON` / `LOOP OFF` | matching ACK | Change the prototype loop flag |
| `STATUS` | `STATUS playing=0 loop=0` | Report transport flags |
| `QUIT` | `ACK QUIT` | Close the program |
| anything else | `ERR UNKNOWN_COMMAND` | No state change |

The `LOOP` flag does not yet repeat MIDI or audio material.

## ESP32-S3 USB serial

List ports without opening audio or changing a port:

```powershell
.\build-win\bin\Release\LamaSyncAudIO.exe --list-ports
```

Test only the newline-delimited protocol at 115200 8N1:

```powershell
.\build-win\bin\Release\LamaSyncAudIO.exe --protocol-only --com COM3
```

Run protocol and Windows audio together:

```powershell
.\build-win\bin\Release\LamaSyncAudIO.exe --com COM3
```

Replace `COM3` with the currently enumerated port. The bridge disables DTR and RTS so opening
the port does not intentionally reset or force an ESP32 boot mode. The ESP32 firmware must send
ASCII/UTF-8 commands terminated by `\n` and read one newline-delimited reply. Port discovery does
not prove that a listed device is an ESP32-S3; verify the device identity in Windows before a
physical test.

## License boundary

Bridge sources and AudIO are covered by `AGPL-3.0-or-later`; see the repository root
[`LICENSE`](../LICENSE) and [`LICENSES.md`](LICENSES.md). AudIO contains third-party components
under their own licenses, documented in [`AudIO/README.md`](../AudIO/README.md).

This bridge uses AudIO's standalone CMake build. It does not fetch, bundle, include or link JUCE.
The separate IGNITE application in `Source/Application` is JUCE-based and is outside this build.
Any future JUCE GUI must undergo a separate license review before distribution.
