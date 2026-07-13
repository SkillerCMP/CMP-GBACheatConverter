# Technical references

GBA Cheat Converter is an independent converter implementation.

The GBA cheat-format behavior and cryptographic algorithms were cross-checked
against:

- mGBA's open-source GBA cheat implementations (MPL-2.0), including its
  CodeBreaker, GameShark Advance, and Pro Action Replay v3 handlers.
- `gameshark-gba-tooling` by kirschju (MIT), including its Action Replay v3
  encryption/decryption reference.
- Public AR MAX / Pro Action Replay v3 code-type documentation.
- Physical-device dumps used only as reverse-engineering references and
  fingerprints.
- Omega DE Kernel 1.06 Original and Enhanced 1.06E3 source used to verify the
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
