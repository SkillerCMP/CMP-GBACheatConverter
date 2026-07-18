# Save Output As formats

The current release provides native emulator/device files under:

```text
File
  Save Output As
    Current Output Text...
    EZ-Flash (.cht)...
    Action Replay MAX (.dsc)...
    VisualBoy Advance-M (.clt)...
    My Boy! (.cht)...
    MiSTer (.zip)...
    Mednafen (.cht)...
    mGBA (.cheats)...
    Libretro / RetroArch (.cht)...
```

The native exporter reparses the current Output editor using the selected
Output format. This means manual edits made in the Output editor are included.
Each destination is exported per cheat entry. If an entry cannot be represented
without changing its behavior, the whole entry is omitted and the GUI reports
a compatibility warning. Controlled writes are never flattened into
unconditional writes.


## Opening or dragging native output files

The GUI can reverse-import every format listed above. `File > Open Input` and
Windows drag-and-drop call the same importer, and the Input format changes
before Auto Convert runs.

| Native file | Input selection after import |
|---|---|
| Action Replay MAX `.dsc` | Encrypted Action Replay MAX |
| VisualBoy Advance-M `.clt` | Stored FCD, GameShark, or PAR v3 family when identifiable; otherwise a safe common format |
| EZ-Flash `.cht` | EZ-Flash |
| My Boy! `.cht` | Contained family, or a lossless common format when mixed |
| mGBA `.cheats` | Contained family, or a lossless common format when mixed |
| Libretro `.cht` | Contained family, or a lossless common format when mixed |
| Mednafen `.cht` | Editable compatible family when exact; otherwise native-only metadata retained for CLI re-export |
| MiSTer `.gg` / `.zip` | Lossless direct-write and one-record-condition text |

Detection is parser-backed and confidence-scored because several unrelated formats share `.cht`. EZ-Flash, My Boy!, RetroArch, and Mednafen parsers are all evaluated; extension is only a confidence signal. Competing matches are reported.
Binary record counts and lengths are bounds-checked. MiSTer input validates the
ZIP central directory, local headers, Stored or Deflate payloads, optional data
descriptors, CRC-32 values, `.gg` byte masks, operation types, aligned 28-bit
addresses, and the GBA core's 32-record limit. Raw `.gg` files are accepted
directly. A normal ZIP without a structurally valid `.gg` entry is not claimed
as MiSTer input.

VBA-M `.clt` import accepts the current type-1 84-byte record layout and the
legacy type-0 80-byte layout. Stored CodeBreaker/FCD, GameShark v1/v2, and PAR
v3 code strings are preserved when valid. Generic internal direct-write records
are reconstructed from their decoded width, address, and value fields.


## EZ-Flash `.cht`

The writer follows the selected `Options > EZ-Flash` mode. It reparses the
current Output editor, so manual edits are included and target limits are
rechecked.

### Original

- emits stock byte-list values such as `ON=25BC4,3F,42;`
- expands safe 16- and 32-bit writes into sequential bytes
- omits conditions and other dependent operations as complete units
- remains compatible with the stock Omega DE `.cht` engine

### Enhanced 1.06E7 format revision 6

- emits ungrouped entries as standalone `CodeName=commands;` rows
- emits plain `[Group]` sections as multi-select groups whose siblings may all be active
- emits `[Group|ONE]` for zero-or-one groups; the exact uppercase suffix is case-sensitive
- reserves `=` for code names and uses `:` for every Enhanced command
- emits one-record `W8`, `W16`, and `W32` writes
- emits width-aware IF-family comparisons with nested `ELSE` / `ENDIF`
- emits one-record `ADD` / `SUB`, two-record `PTR` / `FILL`, and four-record `SLIDE`
- keeps FILL/SLIDE record costs fixed regardless of repetition count
- emits pre-launch `ROMIF` guards and `ROM` patches in separate tables
- rejects ROM patches inside runtime IF branches
- enforces 49-byte group/code names, including the physical `|ONE` suffix, 298-character rows, the shared 128-record table, and the 4,096-write-per-pass budget
- sums standalone rows and plain-group siblings for capacity because they may be active together
- counts only the largest sibling in each `|ONE` group

Standalone rows must precede the first group; the exporter reorders them with a warning when necessary. The earlier command-as-key grammar is not accepted. Stock Original byte-list values remain supported as a separate mode.

Both modes use `.cht`. If no entry can be represented safely, no file is created.

## Action Replay MAX `.dsc`

The writer reproduces the attached `ARDS000000000001` container:

- three fixed UTF-16LE game-title fields
- one short game-title field
- encrypted PAR v3/Action Replay MAX code rows
- fixed 20-byte cheat-name records
- trailing device padding

The output filename becomes the game title. Master/hook/Game-ID entries are
omitted because the supplied `.dsc` sample stores normal selectable cheats,
not the game master record. Names are truncated to the container limits.

## VisualBoy Advance-M `.clt`

The writer produces the current VBA-M type-1 container:

- version `1`
- list type `1`
- variable record count with no fixed empty-record padding
- 84-byte records matching the current `CheatsData` field layout
- correct `code`, `size`, `status`, `enabled`, `rawaddress`, `address`,
  `value`, `oldValue`, code-string, and description offsets
- maximum 16,384 records

Each entry is first encoded as exact CodeBreaker/FCD internal records when that
family can preserve it. Otherwise, the exporter tries exact encrypted PAR v3
records and stores both the original encrypted row and its decoded VBA-M
operation fields. This permits conditions, slides, fills, pointers, arithmetic,
ROM operations, master records, and other supported types when an exact family
representation exists. Entries that cannot be represented exactly are omitted
as complete units.

The importer also accepts VBA-M's legacy type-0 80-byte records. Exported
records are disabled by default so opening a generated list does not
automatically activate every cheat.

## My Boy! `.cht`

This implementation is provisional: it is based on existing converter fixtures, not an inspected upstream My Boy! parser source. The XML writer can mix formats per entry:

- `type="cb"` for exact raw FCD/CodeBreaker-compatible entries
- `type="gs3"` for exact raw PAR v3/Action Replay MAX entries

Names are XML escaped. Entries that cannot be represented exactly by either
family are omitted.

## MiSTer `.gg` / `.zip`

The `mister-zip` writer creates a standard ZIP archive containing one Stored `.gg` file per cheat. The `mister-gg` writer emits one raw `.gg` payload and therefore requires exactly one compatible cheat entry. The importer accepts both raw `.gg` files and ZIP entries using either
Stored or Deflate compression, including ordinary central-directory data
descriptors.

Each GBA `.gg` record is 16 bytes and contains four little-endian dwords:

```text
flags | address | compare/reserved | replacement
```

The GBA core uses the low flags nibble as the operation type, the next nibble
as a four-byte lane mask, a canonical aligned 28-bit address, and the
replacement/comparison value. The third dword is present in the container but
is ignored by the GBA core; nonzero values are reported and normalized away on
re-export.

Supported operations:

- independent aligned 8-, 16-, and 32-bit writes
- equality, inequality, greater-than, greater-or-equal, less-than, and
  less-or-equal conditions
- arbitrary imported byte-lane write masks, reconstructed as exact byte/word
  writes

A MiSTer condition only decides whether the immediately following record is
executed. When one semantic condition controls several writes, the exporter
repeats the condition before every write. Those repeated records count toward
the hardware limit. Each `.gg` file is therefore limited to **32 records**.

Nested condition skip-chains, ELSE blocks, pointers, cartridge patches, hooks,
and device-only operations are omitted as complete entries when their behavior
cannot be represented exactly.

## Mednafen `.cht`

The codec follows Mednafen 1.32.1's `mempatcher.cpp` load/save grammar. A file
may contain multiple game sections:

```text
[0123456789abcdef0123456789abcdef] Game Name
R A 1 L 0 02000000 63 Infinite Health

```

Supported native records include:

- `R` replace/write
- `A` add
- `T` transfer/copy
- `S` read substitution
- `C` compare-on-read substitution
- active `A` and inactive `I` state
- lengths from 1 through 8 bytes
- little-endian `L` and big-endian `B` values
- 64-bit values and compare values
- instance counts
- extended `!` records with repeat count, destination increment, value
  increment, transfer source address, and source increment
- multiple comma-separated conditions using `>=`, `<=`, `>`, `<`, `==`,
  `!=`, `&`, `!&`, `^`, `!^`, `|`, and `!|`
- multiple ROM MD5/game-name sections in one file

Imported native records retain every field for exact Mednafen-to-Mednafen CLI
re-export. Compatible records are also exposed through the shared semantic
model. Records that are valid only in Mednafen remain native-only instead of
being guessed into a device-code family.

For newly generated files, the GUI asks for the matching `.gba` ROM and
calculates the section MD5. The CLI accepts:

```text
GbaCheatConverterCLI --from cb-raw --to mednafen-cht \
  --rom-md5 0123456789abcdef0123456789abcdef \
  --game-name "Game Name" codes.txt > gba.cht
```

A regular sequence of writes with constant address and value steps can be
collapsed into one extended record. Irregular multi-operation entries are
omitted as a whole rather than split into independently toggleable Mednafen
cheats.

## mGBA `.cheats`

The codec follows mGBA's `mCheatParseFile`, `mCheatSaveFile`, and GBA cheat
directive handlers. It supports:

- `!GSAv1` and `!GSAv1 raw`
- `!PARv3` and `!PARv3 raw`
- `!disabled` for the following set
- `!reset` directive-list behavior
- named `# Cheat` sets
- CodeBreaker 8+4 rows
- GameShark/PAR 8+8 rows
- VBA `ADDRESS:VALUE` rows with 8-, 16-, or 32-bit values
- enabled-only files that contain no `!disabled` marker

Imported sets retain their resolved family, original rows, and toggle state for
lossless mGBA-to-mGBA CLI output. Mixed-family sets that cannot be represented
safely in one GUI device-code format remain available as native-only records.

New semantic entries keep their `enabled` state. The writer prefers exact raw
CodeBreaker rows, retains the historical encrypted PAR v3 fallback, and now
also tries exact GSAv1 and raw PAR/GS families before omitting an incompatible
entry.

## Libretro / RetroArch `.cht`

The codec follows RetroArch's indexed `cheat_manager` format. It reads and
writes the complete saved field set for every record:

```text
cheats = N
cheat0_desc = "Name"
cheat0_code = "CODE+CODE"
cheat0_enable = false
cheat0_big_endian = false
cheat0_handler = 0
cheat0_memory_search_size = 3
cheat0_cheat_type = 1
cheat0_value = 0
cheat0_address = 0
cheat0_address_bit_position = 0
cheat0_rumble_type = 0
cheat0_rumble_value = 0
cheat0_rumble_port = 0
cheat0_rumble_primary_strength = 0
cheat0_rumble_primary_duration = 0
cheat0_rumble_secondary_strength = 0
cheat0_rumble_secondary_duration = 0
cheat0_repeat_count = 1
cheat0_repeat_add_to_value = 0
cheat0_repeat_add_to_address = 1
```

Two handler modes are preserved:

- `handler = 0` passes `code` to the active Libretro core. New semantic
  entries are exported in this mode using exact raw FCD rows when possible,
  otherwise exact raw PAR v3 rows.
- `handler = 1` is RetroArch's native memory engine. Imported records retain
  the operation type, width, endian flag, address mask, repeats, value/address
  increments, enabled state, and rumble fields for exact native re-export.

Native handler-1 addresses are offsets in RetroArch's flattened core-memory
space, not guaranteed GBA hardware addresses. Direct writes/add/subtract may be
shown semantically when safe, but cross-record conditions, sub-byte masks,
and core-specific records remain native-only instead of being guessed. The CLI
can perform lossless native-to-native conversion with:

```text
GbaCheatConverterCLI --from retroarch-cht --to retroarch-cht game.cht
```

## Safety and warnings

The save operation reports how many entries were written and lists omitted
entries. The native file is still saved when at least one compatible entry
exists. If no entry is compatible, no file is written.
