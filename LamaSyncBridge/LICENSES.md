# LamaSync AudIO license notes

## Bridge and AudIO

- `LamaSyncBridge/`: AGPL-3.0-or-later, matching the repository license.
- `AudIO/`: AGPL-3.0-or-later. Bundled third-party code retains its own license; the component
  list and locations are maintained in `AudIO/README.md`.

Distributing the executable therefore also requires satisfying the corresponding source and
license obligations. This file is a technical inventory, not legal advice.

## JUCE

JUCE is referenced and used by the separate IGNITE application under `Source/Application`.
The LamaSync bridge target uses AudIO through its standalone CMake interface and has no JUCE
headers, libraries, downloads or link dependencies. Consequently, this bridge artifact makes
no claim about a JUCE redistribution license. Reassess that boundary before adding a JUCE GUI or
linking any JUCE module.

## Generated artifacts

`LamaSyncAudIO.exe` is built from the AGPL-covered bridge and AudIO sources. The generated WAV
test tone contains no bundled sample or SoundFont material.
