# Device ROM and EZ-Flash code-type audit — v2.00

This audit was performed against the five locally supplied device ROM images.
The ROM images are not distributed with the converter.

## CodeBreaker / Xploder Advance — FCD 8+4

Both devices use the same semantic 0-F family table:

| Type | ROM behavior | Converter behavior |
|---|---|---|
| 0 | Game identifier / metadata | Preserved as GameId |
| 1 | Hook/master operation | Preserved when representable |
| 2 | 16-bit OR | Exact |
| 3 | 8-bit write | Exact |
| 4 | Incrementing slide/fill | Exact compact or expanded |
| 5 | Packed 16-bit list | Exact compact or expanded |
| 6 | 16-bit AND | Exact |
| 7 | 16-bit equal condition | Exact |
| 8 | 16-bit write | Exact |
| 9 | FCD encryption key/reseed | Consumed as key metadata |
| A | 16-bit not-equal condition | Exact |
| B | Unsigned greater-than condition | Exact; unsigned hint retained |
| C | Unsigned less-than condition | Exact; unsigned hint retained |
| D | Special operation; address 0x20 is device button/KEYINPUT NAND | Exact known form; others unsupported |
| E | 16-bit add | Exact; subtraction represented by two's complement |
| F | 16-bit bit-mask/AND condition | Exact |

No missing FCD dispatcher family was found in the current semantic model.

## GameShark GBA / Action Replay GBX — 8+8

The executable primary families observed in the ROM dispatcher are:

| Family | ROM behavior | Converter behavior |
|---|---|---|
| 0 | 8-bit write | Exact |
| 1 | 16-bit write | Exact |
| 2 | 32-bit write | Exact |
| 3/00 | Assignment-address list | Exact compact round-trip; safely expandable |
| 3/10 | Add 8-bit immediate to 32-bit target | Exact |
| 3/20 | Subtract 8-bit immediate from 32-bit target | Exact |
| 3/30 | Add 16-bit immediate to 32-bit target | Exact |
| 3/40 | Subtract 16-bit immediate from 32-bit target | Exact |
| 3/50 | Add full 32-bit continuation value | Exact, including unused second continuation word |
| 3/60 | Subtract full 32-bit continuation value | Exact, including unused second continuation word |
| 6 | 16-bit cartridge ROM interception patch | Canonical mode exact; unusual flags preserved with warning |
| 8 | Physical-device button write / `80F00000` slowdown | Exact supported forms |
| D/0 | Equal, next operation | Exact |
| D/1 | Not equal, next operation | Exact |
| D/2 | Unsigned less-or-equal, next operation | Exact |
| D/3 | Unsigned greater-or-equal, next operation | Exact |
| E/0 | Equal, following N operations | Exact |
| E/1 | Not equal, following N operations | Exact |
| E/2 | Unsigned less-or-equal, following N operations | Exact |
| E/3 | Unsigned greater-or-equal, following N operations | Exact |
| F | Hook/master operation | Exact when representable |
| DEADFACE | Dynamic v1/v2 TEA-key replacement | Exact rolling-key behavior |
| `xxxxxxxx 001DC0DE` | Game identifier metadata | Preserved |

Families without an executable runtime handler remain Unsupported. They are
never guessed as direct RAM writes.

### Dynamic DEADFACE

The GameShark/AR GBX ROMs use their own two 256-byte lookup tables. Only the
low 16 bits of the DEADFACE value select the derived four-word TEA key. The key
changes after the DEADFACE row and remains active across later rows and cheat
entries until another DEADFACE row changes it.

## Action Replay MAX / PAR v3

Verified areas:

- 8/16/32-bit direct assign and add operations.
- Signed and unsigned strict comparisons remain distinct.
- Next-one, next-two, block IF, ELSE, and ENDIF actions.
- 8/16/32-bit pointer writes.
- 8/16/32-bit physical-device button writes.
- 8/16/32-bit fill/increment operations.
- Four ROM-patch special rows (`18`, `1A`, `1C`, `1E`).
- Slowdown special row (`08`).
- Dynamic `DEADFACE` reseeding.

Special dispatch uses the exact high byte (`FF000000` mask). Values such as
`11xxxxxx` and `19xxxxxx` are reserved aliases, not valid `10` button or `18`
patch rows.

## Cross-format signedness rule

FCD B/C comparisons are unsigned. AR MAX has separate signed and unsigned
strict-comparison opcodes. FCD-to-AR MAX uses the unsigned variants. A signed
AR MAX strict comparison has no exact FCD equivalent and is suppressed together
with its controlled writes.


## EZ-Flash Omega DE Enhanced 1.06E3

The Enhanced kernel source confirms a text-driven runtime and pre-launch ROM
patch model rather than an encrypted cartridge code stream. The converter
implements the following verified commands:

| Command | Verified behavior |
|---|---|
| `ON=` / `ON:` | Arbitrary-length little-endian byte writes to EWRAM/IWRAM. |
| `IF=` | Byte-array equality; multiple discontiguous terms form one logical AND group. |
| `IFNE=` | Byte-array not-equal comparison. |
| `IFLT=` / `IFGT=` | Unsigned ordered byte-array comparisons. |
| `IFLE=` / `IFGE=` | Unsigned inclusive ordered byte-array comparisons. |
| `ELSE` / `ENDIF` | Explicit true/false runtime block structure. |
| `ADD=` / `SUB=` | Arbitrary-width little-endian arithmetic with carry/borrow across all bytes. |
| `PTR=` | One-level pointer write using a 32-bit pointer, signed 32-bit offset, and byte payload. |
| `FILL=` | Transactional repeated pattern expansion. |
| `SLIDE=` | Transactional count/address-step/value-step expansion. |
| `ROM=` | Pre-launch Game Pak ROM image patch bytes. |
| `ROMIF=` | Pre-launch ROM byte-array equality guard for mixed RAM/ROM entries. |

Enhanced 1.06E3 retains the compact address map for EWRAM, IWRAM, and read-only
I/O conditions. ROM commands use full Game Pak addresses. Runtime commands
share one 128-record table; ROM and ROMIF bytes use separate pre-launch tables.
The converter also enforces the verified 49-byte section-name and 298-visible-
character physical-row limits. Malformed or overflowing dependent entries are
rolled back as complete units.

CodeBreaker/Xploder slides, compatible GameShark arithmetic/comparisons, and
compatible AR MAX comparisons, block ELSE, pointer, fill/slide, and ROM forms
map to Enhanced commands only when the behavior is exact. Device-only slowdown,
hook/master, interception, and unsupported nested semantics remain suppressed.
