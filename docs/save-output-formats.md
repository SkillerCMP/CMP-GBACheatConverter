# Save Output As formats

The current release provides native emulator/device files under:

```text
File
  Save Output As
    Current Output Text...
    EZ-Flash (.cht)...
    Action Replay MAX (.dsc)...
    VisualBoy Advance (.clt)...
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
| VisualBoy Advance `.clt` | Raw CodeBreaker/FCD |
| EZ-Flash `.cht` | EZ-Flash |
| My Boy! `.cht` | Contained family, or a lossless common format when mixed |
| mGBA `.cheats` | Contained family, or a lossless common format when mixed |
| Libretro `.cht` | Contained family, or a lossless common format when mixed |
| Mednafen `.cht` | Raw CodeBreaker/FCD |
| MiSTer `.zip` | Lossless FCD, AR MAX, or EZ-Flash text |

Detection is content-based because several unrelated formats share `.cht`.
Binary record counts and lengths are bounds-checked. MiSTer ZIP local headers,
stored sizes, CRC-32 values, `.gg` masks, and condition depths are validated.
The importer supports the uncompressed ZIP layout produced by this converter;
a ZIP using compression or data descriptors is rejected explicitly.

VBA `.clt` records are reconstructed from their decoded width, address, and
value fields instead of relying on the short display-code field. This is
necessary to preserve 32-bit writes.


## EZ-Flash `.cht`

The writer follows the currently selected `Options > EZ-Flash` mode. It does
not merely copy the Output text: the current Output editor is parsed and then
re-emitted through the audited EZ-Flash exporter, so manual edits are included
and the kernel limits are enforced.

### Original

- emits byte writes using `ON=` only
- expands safe 16- and 32-bit writes into sequential bytes
- omits condition-controlled or otherwise dependent entries as a complete unit
- never writes to the compact I/O range

### Enhanced

- emits equality, inequality, and unsigned ordered IF-family groups
- preserves supported `ELSE` / `ENDIF` branches
- emits arbitrary-byte `ADD=`, `SUB=`, one-level `PTR=`, `FILL=`, and `SLIDE=`
- emits standalone `ROM=` image patches
- emits canonical `ROMIF=...;ON=...;ROM=...;` guarded mixed entries
- preserves runtime `IF=...;ON=...;ROM=...;` ordering with an independent ROM tail
- preserves multiple sequential IF groups and compound AND terms
- supports continuation rows when a menu line would exceed the safe length
- enforces 49-byte section names, 298-character physical rows, and the shared
  128-runtime-record limit

Both modes use `.cht` as the file extension. Compatibility omissions are shown
after the file is saved; if nothing can be represented safely, no file is
created.

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

## VisualBoy Advance `.clt`

The writer produces the legacy VBA fixed table:

- version `1`
- list type `1`
- up to 100 fixed 80-byte records
- decoded address, value, and width fields
- original-style compact code text and description fields

Only independent 8-, 16-, and 32-bit memory writes are safe in this legacy
record layout. Conditions, pointers, patches, hooks, and other dependent
operations are omitted as complete entries.

## My Boy! `.cht`

The XML writer can mix formats per entry:

- `type="cb"` for exact raw FCD/CodeBreaker-compatible entries
- `type="gs3"` for exact raw PAR v3/Action Replay MAX entries

Names are XML escaped. Entries that cannot be represented exactly by either
family are omitted.

## MiSTer `.zip`

The writer creates a standard ZIP archive containing one `.gg` file per cheat.
Each `.gg` record is the 16-byte little-endian unit used by the attached
MiSTer archive.

Supported operations:

- independent 8-, 16-, and 32-bit writes
- exact equality conditions controlling one or more writes

Unsupported condition types, ELSE blocks, pointers, cartridge patches, hooks,
and device-only operations are omitted as complete entries. A single `.gg`
file is limited to 128 records, matching the MiSTer runtime table.

## Mednafen `.cht`

Mednafen identifies a GBA cheat section by the ROM MD5. After choosing the
output filename, the GUI asks for the matching `.gba` ROM and calculates the
MD5 automatically. The ROM filename becomes the game title in the header.

Only independent direct writes are emitted. Condition-controlled writes and
other dependent operations are omitted rather than made unconditional.

## mGBA `.cheats`

The writer creates disabled cheats and selects the exact format per entry:

- raw CodeBreaker/FCD rows by default
- encrypted PAR v3 rows preceded by `!PARv3` when FCD cannot represent the
  entry exactly

## Libretro / RetroArch `.cht`

The writer creates the indexed Libretro format:

```text
cheats = N
cheat0_desc = "Name"
cheat0_code = "CODE+CODE"
cheat0_enable = false
```

Each entry uses exact raw FCD rows when possible, otherwise exact raw PAR v3
rows. Entries that cannot be represented by either are omitted.

## Safety and warnings

The save operation reports how many entries were written and lists omitted
entries. The native file is still saved when at least one compatible entry
exists. If no entry is compatible, no file is written.
