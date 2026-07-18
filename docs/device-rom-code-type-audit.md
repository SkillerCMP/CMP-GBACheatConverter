# Device ROM and EZ-Flash code-type audit — v2.06

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
| D | Special operation; address 0x20 is a masked GBA KEYINPUT equality against zero | Exact known form; others unsupported |
| E | 16-bit add | Exact; subtraction represented by two's complement |
| F | 16-bit masked not-equal-zero condition | Exact |

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


## EZ-Flash Omega DE Enhanced 1.06E7

The E7 source implements `.cht` format revision 6. Every visible code is a
`CodeName=commands;` row. Rows before the first heading are standalone independent
toggles. Plain `[Group]` headings are multi-select. `[Group|ONE]` permits zero or
one selected sibling; the suffix must be exact uppercase `|ONE`, counts toward
the 49-byte physical heading limit, and is removed from the visible menu name.

| Command | Verified behavior | Runtime records |
|---|---|---:|
| `W8`, `W16`, `W32` | Exact width-aware EWRAM/IWRAM write | 1 |
| `IF`, `IFNE`, `IFLT`, `IFGT`, `IFLE`, `IFGE` | Width-aware unsigned comparison | 1 |
| masked `IF*M` forms | Masked width-aware comparison | 2 |
| `ELSE`, `ENDIF` | Nested runtime block control | 1 |
| `ADD`, `SUB` | Width-aware arithmetic | 1 |
| `PTR` | One-level pointer plus signed 32-bit offset | 2 |
| `FILL` | Compact repeated constant-width write | 2 |
| `SLIDE` | Compact address/value progression | 4 |
| `ROMIF`, `ROM` | Pre-launch ROM guard/patch bytes | 0 |

The compact address map covers EWRAM, IWRAM, and condition-only I/O. The
converter canonicalizes all `02xxxxxx` EWRAM and `03xxxxxx` IWRAM mirrors before
emitting compact addresses. Width-aware accesses must be aligned and remain
inside one region. ROM guards must precede runtime actions, and ROM patches
cannot be placed inside a runtime IF branch.

The selected rows share a 128-record table and a 4,096-write-per-pass safety
budget. Capacity sums every standalone row and every sibling in plain groups,
because all may be selected together. Each `|ONE` group contributes only its
largest sibling. FILL and SLIDE consume fixed record counts while their
repetition counts contribute to the work budget. Malformed or overflowing rows
roll back transactionally.

CodeBreaker/Xploder slides, compatible GameShark arithmetic/comparisons, and
compatible AR MAX comparisons, block ELSE, pointer, fill/slide, and ROM forms
map to E7 only when exact. Device-only slowdown, hook/master, interception, and
unsupported dependency structures remain suppressed.
