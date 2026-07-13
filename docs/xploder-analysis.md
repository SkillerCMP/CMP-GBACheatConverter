# Xploder Advance R1 compatibility analysis

Uploaded ROM fingerprint:

- Size: 131,072 bytes
- CRC32: `4F3E7427`
- SHA-256: `f1a99e2874dfc19b7843a64f28e8588e1dcdf3edf3469c3e3fd8ed178a5eab59`

The Xploder Advance R1 and CodeBreaker R1 ROMs are both Future Console Design
builds. Binary comparison found the seeded-cipher constants at matching code
locations with an Xploder offset of `+0x70`:

- `0x41C64E6D` LCG multiplier
- `0x00003039` LCG increment
- `0x4EFAD1C3` reseed state
- `0x00001111` and `0x0000F254` seed mixers

The aligned cipher area is more than 94% byte-identical across a broad region,
with an exact 757-byte run. This version therefore shares the CodeBreaker /
GameShark SP / Xploder 8+4 dispatcher and seeded encryption implementation.

The converter keeps Xploder as a separate format name so a future Xploder ROM
revision can receive its own codec if a real behavior difference is found.
