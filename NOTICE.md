# Technical references

GBA Cheat Converter is an independent converter implementation.

The GBA cheat-format behavior and cryptographic algorithms were cross-checked
against:

- mGBA's open-source GBA cheat implementations (MPL-2.0), including `mCheatParseFile`, `mCheatSaveFile`, GBA directive handling, CodeBreaker, GameShark Advance, Pro Action Replay v3, and VBA row parsing.
- RetroArch's `cheat_manager.c` and `cheat_manager.h` (GPL-3.0-or-later)
  from commit `1c837e4619da01399c3f5dca170508f82fd1fbb1`, used to verify the
  indexed `.cht` fields, defaults, handler modes, repeats, and runtime
  condition behavior.
- VisualBoyAdvance-M's GBA cheat-list implementation (GPL-2.0-or-later), used
  to verify current type-1 and legacy type-0 `.clt` record layouts.
- Mednafen 1.32.1's `src/mempatcher.cpp` (GPL-2.0-or-later), used to
  verify `.cht` section headers, record types, extended fields, endian and
  value widths, conditions, and load/save grammar.
- GBA MiSTer and Main MiSTer sources (GPL-2.0-or-later), used to verify the
  GBA `.gg` record layout, condition truth tables, 32-record core limit, and
  ZIP loading behavior.
- `gameshark-gba-tooling` by kirschju (MIT), including its Action Replay v3
  encryption/decryption reference.
- Public AR MAX / Pro Action Replay v3 code-type documentation.
- Physical-device dumps used only as reverse-engineering references and
  fingerprints.
- Omega DE Kernel 1.06 Original and Enhanced 1.06E4.3 source used to verify the
  EZ-Flash text grammar, runtime record model, ROM tables, and safety limits.

Action Replay MAX dynamic `DEADFACE` behavior was also checked against the
translation tables in the supplied AR MAX ROM. The v3 tables were found at ROM
offsets `0xEF34` and `0x2E3E0`.

No proprietary cheat-device ROM, EZ-Flash kernel binary, or third-party source
archive is redistributed by this project.

# Project license

GBA Cheat Converter is distributed under the GNU General Public License
version 3 only (`GPL-3.0-only`). The projects and materials listed above were
used as technical references; their own licenses continue to apply to their
original works.
