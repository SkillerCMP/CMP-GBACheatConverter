# 🎮 GBA Cheat Converter v2.17

> **v2.17** adds one-way EZ-Flash migration from stock Original `ON=` byte lists to the latest Enhanced E7 revision-6 format, with safe W16/W32, FILL, and SLIDE condensation. In the GUI, Enhanced mode auto-migrates pasted/opened Original files; Original mode preserves EZ-Flash input unchanged.

<p align="center">
  <strong>Convert, clean, detect, import, and export Game Boy Advance cheat codes.</strong>
</p>

<p align="center">
  <code>Windows x64</code>
  &nbsp;•&nbsp;
  <code>GUI + CLI</code>
  &nbsp;•&nbsp;
  <code>GPL-3.0-only</code>
</p>
<p align="center">
  <a href="https://github.com/SkillerCMP/CMP-GBACheatConverter/releases">
    <img alt="GitHub Downloads — All Releases" src="https://img.shields.io/github/downloads/SkillerCMP/CMP-GBACheatConverter/total?style=social">
  </a>
  <a href="https://github.com/SkillerCMP/CMP-GBACheatConverter/releases/latest">
    <img alt="GitHub Downloads — Latest Release" src="https://img.shields.io/github/downloads/SkillerCMP/CMP-GBACheatConverter/latest/total?style=social">
  </a>  
</p>


---

## ✨ What It Does

GBA Cheat Converter provides one shared conversion engine for the major GBA
cheat-device families:

- **CodeBreaker / GameShark SP / Xploder Advance**
- **GameShark Advance / Action Replay GBX**
- **Action Replay MAX / Pro Action Replay v3**
- **EZ-Flash Original**
- **Original → Enhanced E7 migration** with safe W16/W32, FILL, and SLIDE condensation
- **EZ-Flash Omega DE Enhanced 1.06E7**

It supports raw and encrypted codes, automatic format detection, clipboard
cleanup, dependency-safe conversion, native cheat-file import/export, and both
graphical and command-line workflows.

---

## 🚀 Main Features

| Feature | Description |
|---|---|
| 🔄 **Multi-format conversion** | Converts between supported raw, encrypted, and EZ-Flash formats. |
| 🔍 **Auto Detect** | Detects EZ-Flash, FCD, GameShark/AR GBX, and AR MAX input. |
| 🔐 **Encryption support** | Handles FCD seeds, GameShark v1, and PAR v3/AR MAX encryption. |
| 📂 **Native file import** | Open or drag emulator/device cheat files directly into the Input editor. |
| 💾 **Native file export** | Saves EZ-Flash, AR MAX, VBA, My Boy!, MiSTer, Mednafen, mGBA, and Libretro files. |
| 🗂️ **CMP database layer** | Imports and exports nested CMP groups, credits, inherited codes, and `$` rows. |
| 🪞 **RAM mirror normalization** | Canonicalizes all `02xxxxxx` EWRAM and `03xxxxxx` IWRAM mirrors for exact EZ-Flash output. |
| ⚙️ **EZ-Flash optimization** | Merges safe repeated conditions and widens contiguous RAM writes to reduce E7 runtime-record use. |
| ↩️ **Independent Word Wrap** | Toggle wrapping separately for the Input and Output editors. |
| 🧹 **Automatic cleanup** | Repairs compact rows, mixed newlines, GameHacking.org blocks, and attached CodeTwink rows. |
| 📝 **Inline conversion notes** | Places compatibility warnings beside the affected code. |
| 🛡️ **Dependency safety** | Never exports controlled writes without their required condition, hook, pointer, patch, or block. |
| 🖥️ **GUI and dedicated CLI** | Windows builds include the dual-mode GUI executable and a dedicated console executable for BAT files and automation. |
| 🧪 **Regression-tested** | Includes 178 semantic tests for parsers, ciphers, exporters, importers, optimization, containers, and file-output safety. |

---

## 🔁 Supported Cheat Families

```text
Input / Output
  RAW
    CodeBreaker / GameShark SP / Xploder Advance
    GameShark Advance / Action Replay GBX
    Action Replay MAX
    EZ-Flash
  Encrypted
    CodeBreaker / GameShark SP / Xploder Advance
    GameShark Advance / Action Replay GBX
    Action Replay MAX
```

The selected Input and Output formats are shown above their editors and stored
in `GbaCheatConverter.ini`.

<details>
<summary><strong>📋 Format capability summary</strong></summary>

| Family | Raw | Encrypted | Semantic conversion | Native file support |
|---|:---:|:---:|:---:|:---:|
| CodeBreaker / GameShark SP / Xploder Advance | ✅ | ✅ | ✅ | VBA, My Boy!, mGBA, Libretro |
| GameShark Advance / Action Replay GBX | ✅ | ✅ | ✅ | My Boy!, mGBA, Libretro |
| Action Replay MAX | ✅ | ✅ | ✅ | AR MAX `.dsc` |
| EZ-Flash Original | ✅ | — | ✅ | EZ-Flash `.cht` |
| EZ-Flash Enhanced 1.06E7 | ✅ | — | ✅ | EZ-Flash `.cht` |
| Mednafen | — | — | Full native codec; semantic mapping where exact | Mednafen `.cht` |
| MiSTer | — | — | Direct writes plus six one-record comparisons | MiSTer `.gg` / `.zip` |

Exact compatibility depends on the operation being converted. Unsupported
dependent operations are suppressed instead of becoming unsafe unconditional
writes.

See `docs/compatibility-matrix.md` for the complete mapping table.

</details>

---

## 🗂️ CMP Database Input and Output

CMP is handled as an independent database layer around the selected GBA code
family. The underlying rows are still parsed and exported as CodeBreaker/FCD,
GameShark/AR GBX, Action Replay MAX, Xploder, or EZ-Flash operations.

Enable the single checkable menu item:

```text
Options
  ✓ CMP Output
```

- **Checked** — non-EZ output is wrapped with CMP groups, code names, credits,
  `$` rows, and closing `!!` markers.
- **Unchecked** — ordinary device text is produced. Inherited CMP group codes
  are safely prepended to each affected entry.
- **EZ-Flash output** — CMP groups map directly into native E7 sections and
  option names instead of being wrapped in CMP punctuation.

CMP text can be pasted, opened as `.cmp`, or dragged into the Input editor:

```text
!Starting Lives:
+One
%Credits: Skiller
$32025BC4 0001

+Three
$32025BC4 0003
!!
```

Enhanced EZ-Flash output becomes:

```ini
[Starting Lives]
One=W8:25BC4,01;
Three=W8:25BC4,03;
```

<details>
<summary><strong>🧬 CMP hierarchy and code-type behavior</strong></summary>

Supported CMP structure:

```text
!Group Name:
$group-level inherited code

!Nested Group:
+Code Name
%Credits: Author
$code row
$continuation row
!!
!!
```

- `!Group:` opens a group; nested groups are retained.
- `!!` closes the current group.
- `+Code` starts an entry. A relaxed plain name followed by `$` rows is also
  accepted inside a group.
- `%Credits:` is retained independently from the code name.
- `$` is removed before the selected GBA parser receives the row.
- Continuation rows remain together, so FCD slides/packed lists, GameShark
  assignment lists, AR MAX multiline operations, conditions, and encryption
  state are decoded by their actual code types.
- Group-level codes are stored separately and inherited outermost-to-innermost.
- Encrypted FCD seed rows are emitted as CMP group-header `$` rows so stream
  encryption remains continuous.
- EZ-Flash grouped input can be exported back into CMP groups. Standalone E7
  rows remain normal CMP codes; `[Group|ONE]` maps to a CMP leaf whose name
  ends in `|ONE`.

</details>

---

## 📥 Open or Drag Native Cheat Files

`File > Open Input` and drag-and-drop use the same content-based importer.

| File | Input format selected |
|---|---|
| Action Replay MAX `.dsc` | **Encrypted – Action Replay MAX** |
| VisualBoy Advance `.clt` | **RAW – CodeBreaker/FCD** |
| EZ-Flash `.cht` | **EZ-Flash** |
| My Boy! `.cht` | Detected FCD or PAR v3 family; provisional schema pending upstream verification |
| mGBA `.cheats` | Detected CodeBreaker, GSAv1, PAR v3, or VBA family; mixed/native-only sets retained for CLI re-export |
| Libretro / RetroArch `.cht` | Core-code family when editable; native handler fields are retained losslessly for CLI re-export |
| Mednafen `.cht` | Editable compatible family when exact; complex records retained natively for CLI re-export |
| MiSTer `.gg` / `.zip` | Lossless editable direct-write and conditional text |

Every candidate parser is evaluated and scored. File extensions add confidence but do not override the content. This reliably separates EZ-Flash, My Boy!, RetroArch, and Mednafen files that all use `.cht`. If more than one parser matches, the selected format and competing matches are reported instead of silently discarding the ambiguity.

<details>
<summary><strong>🔒 Native importer validation</strong></summary>

The importer checks:

- Container signatures and record counts
- Complete RetroArch indexed fields, defaults, handler modes, repeats, bit masks, endian state, and rumble metadata
- mGBA directive state, enabled/disabled sets, raw/encrypted family selection, and VBA colon rows
- Complete Mednafen section/record grammar, 64-bit fields, endian state, extended records, conditions, and multiple ROM sections
- Payload boundaries
- XML and quoted-string values
- ZIP central directories, Stored/Deflate entries, and data descriptors
- CRC-32 checksums
- MiSTer byte masks, operation types, record structure, and 32-record limit
- Truncated or malformed binary data

Unsupported binary files are rejected instead of being pasted into the editor
as corrupted text.

After import, the converter decodes editable cheat text, selects the matching
Input format, updates seed controls, and runs Auto Convert when enabled.

</details>

---

## 📤 Save Output As

```text
File
  Save Output As
    Current Output Text...       Ctrl+S
    EZ-Flash (.cht)...
    Action Replay MAX (.dsc)...
    VisualBoy Advance (.clt)...
    My Boy! (.cht)...
    MiSTer (.zip)...
    Mednafen (.cht)...
    mGBA (.cheats)...
    Libretro / RetroArch (.cht)...
```

Native files are generated from the **current Output editor**, including manual
edits.

- EZ-Flash output uses the selected Original or Enhanced mode.
- `--to ezflash-enhanced` is a direct alias for migrating stock Original `.cht` files to current E7 output.
- Targets supporting multiple families choose the exact FCD or PAR v3 form per cheat.
- RetroArch re-export preserves imported handler-0 and handler-1 records exactly, including toggle state and native metadata.
- Targets omit complete entries when their behavior cannot be represented safely.
- New Mednafen output uses the matching GBA ROM to generate its MD5 section header in the GUI; CLI output accepts `--rom-md5` and `--game-name`. Imported Mednafen metadata is retained for native re-export.

See `docs/save-output-formats.md` for container layouts and limits.

---

## 🔍 Auto Detect

Auto Detect recognizes:

- EZ-Flash Original byte lists and E7 revision-6 standalone/grouped `CodeName=commands;` syntax
- Raw and seeded encrypted FCD `8+4`
- Raw and encrypted GameShark Advance / AR GBX `8+8`
- Raw and encrypted Action Replay MAX `8+8`

For ambiguous `8+8` input, candidates are scored using code-type signatures,
payload widths, condition structures, decoded memory ranges, and
family-specific rules. The detector refuses to guess when candidates are too
close.

The resolved family appears in the Input heading, and the status bar displays
the confidence result. Native files use a separate scored parser layer. CLI
inspection is available without conversion:

```text
GbaCheatConverter.exe --detect-only game.cht
GbaCheatConverter.exe --list-formats
```

---

## 🧹 Input Cleanup

Code entering the editors is normalized during paste, open, drag-and-drop,
Swap, Convert, and CLI input.

### Compact code rows

```text
98816CA98172       -> 98816CA9 8172
0515877376E0       -> 05158773 76E0
156739CB40DA6751   -> 156739CB 40DA6751
```

Cleanup includes:

- Compact `8+4` and `8+8` rows
- Colon-separated rows
- Flattened clipboard streams
- CodeTwink names with attached code rows
- UTF-8 BOM removal
- CR, LF, CRLF, and mixed newlines
- Legacy underscore separator removal

Comments, cheat names, metadata, and EZ-Flash syntax remain intact.

<details>
<summary><strong>🏷️ GameHacking.org metadata cleanup</strong></summary>

```text
[M] Must Be On by MadCatz
Codebreaker/GameShark SP/Xploder
98816CA98172
```

becomes:

```text
[M] Must Be On , by MadCatz , Crypt_Codebreaker/GameShark SP/Xploder
98816CA9 8172
```

Normalized format:

```text
Code name , by Credits , Crypt_Device
Code
```

</details>

---

## 📝 Inline Conversion Notes

Compatibility warnings are written directly into converted output:

```text
[Mixed Code]
// Conversion Note: Line 3: unsupported source operation
// Source: 62000010 00001234

32025BC4 00C2

// Conversion Summary: 1 inline note(s); output may be partial.
```

EZ-Flash output uses `#` comments so notes can be loaded again without parser
warnings.

The safety model prevents:

- A controlled write from becoming unconditional
- Hook/master dependencies from being separated
- Pointer writes from being flattened unsafely
- ROM patches from losing their dependency rules
- Unsupported ELSE branches from leaking either branch
- Physical-device-button writes from losing their trigger

---


## ⚙️ Automatic EZ-Flash E7 Optimization

When Enhanced output is selected, the converter automatically reduces runtime
record use without changing code behavior:

- Adjacent identical conditions are merged into one IF block when every branch
  is made only of direct RAM writes.
- Conditions are not merged if a controlled write overlaps the compared memory,
  because the later condition must be evaluated again.
- Adjacent aligned RAM writes are packed into the widest safe `W16` or `W32`
  operations.
- ELSE branches, nested control flow, pointer writes, arithmetic, ROM patches,
  and other order-sensitive operations remain structurally separate.

Example command stream before optimization:

```ini
Merged Example=IF:W16,80130,02BE;W16:405C0,F7FA;ENDIF;
IF:W16,80130,02BE;W16:405C2,0819;ENDIF;
```

becomes:

```ini
Merged Example=IF:W16,80130,02BE;W32:405C0,0819F7FA;ENDIF;
```

This reduces condition/control records as well as write records in the shared
128-record E7 runtime table.

---

## ⚡ EZ-Flash Modes

`Options > EZ-Flash` provides:

- **Original** — stock-compatible byte-list output
- **Enhanced** — Omega DE Kernel 1.06E7 format revision 6

<details>
<summary><strong>🧬 Enhanced 1.06E7 format revision 6 syntax</strong></summary>

Every visible code is a `CodeName=commands;` row. Standalone rows are independent toggles and may appear before the first heading.

```ini
Moon Jump=IFM:W16,80130,0001,0000;W8:1505C,80;ENDIF;
Infinite Health=W16:10A78,0064;
```

A plain `[Group]` is multi-select, while `[Group|ONE]` permits zero or one selected sibling. The `|ONE` suffix is case-sensitive and hidden by the kernel menu.

```ini
[Movement Codes]
Walk Through Walls=W8:15060,01;
Fast Movement=ADD:W16,15062,0001;

[Starting Lives|ONE]
One=W8:25BC4,01;
Three=W8:25BC4,03;
Nine=W8:25BC4,09;
```

Width-aware direct writes:

```ini
[Money]
Maximum=W32:10A78,0098967F;
Half=W32:10A78,004C4B40;
```

Conditions and ELSE:

```ini
[Difficulty]
Hard=IFNE:W16,202,0001;W32:10A78,00000001;ELSE;
W32:10A78,00000000;ENDIF;
```

Masked conditions compare `(memory & mask)` with a value:

```ini
[Moon Jump]
ON=IFM:W16,80130,0001,0000;W8:1505C,80;ENDIF;
```

The masked family is `IFM`, `IFNEM`, `IFLTM`, `IFGTM`, `IFLEM`, and
`IFGEM`. A masked condition consumes two runtime records because the mask is
stored in a continuation record. CodeBreaker `D0000020 0001` maps exactly to
the `IFM` example above, so A may be held with other buttons.

GBA RAM mirrors are normalized before compact addressing. For example, an
Action Replay MAX write to `020426EC` targets the same EWRAM byte as
`020026EC`, so it emits `W8:26EC,...`. IWRAM mirrors in the full `03xxxxxx`
bank are handled the same way.

Compact operations:

```ini
[Operations]
Add=ADD:W32,10A78,00000001;
Pointer=PTR:W8,10,00000123,7F;
Fill=FILL:W32,10000,00000100,11223344;
Slide=SLIDE:W16,20000,00000100,00000002,00000001,1234;
```

ROM guards and patches:

```ini
[ROM Choice]
Safe=ROMIF:09014CD3,FD,00;W8:239F8,04;ROM:080EA0C0,00,20;
```

### Runtime record savings

| Command | Runtime records |
|---|---:|
| `W8`, `W16`, `W32` | 1 |
| normal `IF*`, `ADD`, `SUB` | 1 |
| masked `IF*M` | 2 |
| `PTR`, `FILL` | 2 |
| `SLIDE` | 4 |
| `ROM`, `ROMIF` | 0 runtime records |

`FILL` and `SLIDE` are executed as compact runtime loops. Their record cost no
longer grows with repetition count. A 256-value 32-bit fill uses 2 runtime
records instead of expanding to 1,024 byte records.

Enhanced mode also supports normal and masked comparisons, nested IF blocks, 16 nesting levels, aligned
width-aware accesses, separate ROM tables, 49-byte group and code names,
298-character physical rows, the shared 128-record runtime table, and a 4,096-write-per-pass safety budget.

Format revision 6 retains the colon-based Enhanced command grammar and adds
standalone rows plus explicit `|ONE` selection behavior. See
`docs/ezflash-enhanced-e7.md`.

</details>

---

## 🔐 FCD Seeds

```text
Swap  In Key: [.............]  [ ] Use     Convert     Out Key: [9ABCDEF0:1234]
```

- `In Key` appears for encrypted FCD-family input.
- A plaintext `9XXXXXXX YYYY` row automatically populates the detected key.
- A manual key can initialize decryption when the seed row is missing.
- An embedded seed is authoritative and replaces a different manual key with a warning.
- `Out Key` controls encrypted FCD output only.
- Swap transfers the relevant seed and re-detects seed rows.
- Both `9ABCDEF0:1234` and `9ABCDEF0 1234` are accepted.

CLI options:

```text
--cb-input-key 9XXXXXXX:YYYY
--cb-key       9XXXXXXX:YYYY
```

---

## 🧠 Advanced Code-Type Support

<details>
<summary><strong>🔷 CodeBreaker / GameShark SP / Xploder Advance</strong></summary>

Supported FCD operations include:

- Type `4` slides/fills
- Type `5` packed 16-bit write lists
- Equality, inequality, ordered, and mask conditions
- GBA `KEYINPUT` button activators
- Game ID and hook rows
- 16-bit OR, AND, and ADD
- Raw/encrypted exact re-export
- Semantic expansion to compatible families

A condition before a packed list controls the complete list. When expanded,
its condition span remains attached to every generated write.

</details>

<details>
<summary><strong>🟢 GameShark Advance / Action Replay GBX</strong></summary>

Supported operations include:

- Dynamic `DEADFACE` keys
- Assignment lists
- 32-bit add/subtract forms
- Single and multiline conditions
- Physical GameShark button writes
- Equal, not-equal, unsigned `<=`, and unsigned `>=`
- ROM patches and slowdown rows
- Game IDs and hooks
- Raw/encrypted exact round trips

Physical GameShark buttons remain distinct from the GBA `KEYINPUT` register.

</details>

<details>
<summary><strong>🟠 Action Replay MAX</strong></summary>

Supported operations include:

- Dynamic `DEADFACE` keys
- Pointer writes
- Fill and slide operations
- Add and subtract
- Next-one, next-two, and block conditions
- IF/ELSE/ENDIF structures
- Physical AR MAX button writes
- ROM patches and slowdown rows
- Game IDs and hooks
- Raw/encrypted exact round trips

AR MAX `.dsc` files import directly as **Encrypted – Action Replay MAX**.

</details>

---

## ⌨️ Editor Shortcuts

| Shortcut | Action |
|---|---|
| `Ctrl+A` | Select all |
| `Ctrl+C` | Copy |
| `Ctrl+X` | Cut |
| `Ctrl+V` | Paste |
| `Ctrl+P` | Paste alias |
| `Ctrl+D` | Clear active pane |
| `Ctrl+S` | Save current output text |

The main control row keeps **Swap** and **Convert** beside the FCD seed fields.

### Word Wrap

The Edit menu includes two independent checkable items:

```text
Edit
  ✓ Input Word Wrap
  ✓ Output Word Wrap
```

- **Checked** — long lines wrap inside that editor and the horizontal scrollbar
  is removed.
- **Unchecked** — lines remain on one physical row and the horizontal scrollbar
  is available.
- The Input and Output settings are independent and stored in
  `GbaCheatConverter.ini`.
- Changing either setting preserves the editor text, selection, scroll
  position, focus, font, and modified state.

---

## 🖥️ Command-Line Usage

Windows builds include both a dedicated console executable and the dual-mode GUI executable:

```text
GbaCheatConverter.exe                    Open the GUI
GbaCheatConverter.exe --gui              Force GUI mode
GbaCheatConverterCLI.exe --version       Show version
GbaCheatConverterCLI.exe --help          Show CLI usage
GbaCheatConverterCLI.exe --list-formats  Show canonical input/output names
GbaCheatConverterCLI.exe --detect-only game.cht
```

Example conversion with direct file output:

```text
GbaCheatConverterCLI.exe --from cb-raw --to ez --ez-mode enhanced --output game.cht codes.txt
```

Auto-detect `.cht` and create VBA-M `.clt`:

```text
GbaCheatConverterCLI.exe --from auto --to vba-clt --output game.clt game.cht
```

The same conversion using shell redirection is still supported:

```text
GbaCheatConverterCLI.exe --from auto --to vba-clt game.cht > game.clt
```

Use `>` to replace a file. Never use `>>` for `.clt`, `.dsc`, `.gg`, or `.zip`, because appending binary output corrupts the destination.

Lossless mGBA native conversion:

```text
GbaCheatConverterCLI.exe --from mgba-cheats --to mgba-cheats --output cleaned.cheats game.cheats
```

Lossless Mednafen native conversion:

```text
GbaCheatConverterCLI.exe --from mednafen-cht --to mednafen-cht --output cleaned-gba.cht gba.cht
```

New semantic Mednafen output:

```text
GbaCheatConverterCLI.exe --from cb-raw --to mednafen-cht --rom-md5 0123456789abcdef0123456789abcdef --game-name "Game Name" --output gba.cht codes.txt
```

`--output FILE` and `-o FILE` generate data in memory first, then replace the destination only after a successful non-empty conversion. Warnings and the readable `wrote output:` message stay on the console. Binary output sent to an interactive console is rejected; use `--output` or `> FILE`.

A ready-to-use `examples/Convert-CHT-to-CLT.bat` file supports drag-and-drop conversion of one or more `.cht` files.

<details>
<summary><strong>🧾 Common CLI format names</strong></summary>

```text
--from auto
--from native
--from cb-raw | cb-encrypted
--from gsa-raw | gsa-encrypted
--from armax-raw | armax-encrypted
--from xploder-raw | xploder-encrypted
--from ez
--from armax-dsc
--from vba-clt
--from myboy-cht
--from retroarch-cht
--from mgba-cheats
--from mednafen-cht
--from mister-gg | mister-zip
--from ezflash-cht

--to cb-raw | cb-encrypted
--to gsa-raw | gsa-encrypted
--to armax-raw | armax-encrypted
--to xploder-raw | xploder-encrypted
--to ez
--to armax-dsc
--to vba-clt
--to myboy-cht
--to retroarch-cht
--to mgba-cheats
--to mednafen-cht
--to mister-gg | mister-zip
--to ezflash-cht
```

EZ-Flash mode:

```text
--ez-mode original
--ez-mode enhanced
```

Run `--help` for the complete option list.

</details>

---

## 🛠️ Build for Windows x64

Requirements:

- Windows 10 or newer
- CMake 3.20 or newer
- Visual Studio with **Desktop development with C++**

Run:

```bat
build-windows-x64.cmd
```

The self-contained script:

1. Configures a clean x64 Release build.
2. Treats compiler warnings as errors.
3. Builds the dual-mode executable and tests.
4. Runs CTest.
5. Writes a timestamped build log.
6. Creates `dist-windows-x64` with the executable, documentation, and SHA-256 file.

Reuse the current build directory:

```bat
build-windows-x64.cmd -KeepBuild
```

---

## 🧪 Testing and Safety

The v2.17 source contains **178 semantic tests** covering:

- Raw and encrypted parsers
- Cipher reseeding
- Code-type mappings
- Conditions and dependent spans
- Native import/export containers
- Complete RetroArch handler/core-code/native-memory field preservation
- Stateful mGBA directives, enabled-only files, VBA colon rows, and native CLI round trips
- MiSTer `.gg` operation truth tables, condition expansion, 32-record limits, and raw/ZIP input
- Complete Mednafen `R/A/T/S/C` records, 64-bit values, endian modes, extended fields, all condition operators, multiple MD5 sections, and native CLI round trips
- Stored and Deflate ZIP parsing, data descriptors, central-directory validation, and CRC-32 checks
- EZ-Flash Enhanced planning and limits
- Clipboard and metadata cleanup
- Auto Detect confidence
- Inline note placement
- CLI success and failure behavior
- Direct `--output` file creation, binary stdout guarding, and preservation of existing files after failed conversions
- Canonical Action Replay MAX Raw condition operands at 8-, 16-, and 32-bit widths without changing non-condition upper fields

---

## 📚 Documentation

| Document | Purpose |
|---|---|
| `GBA_Cheat_Device_Code_Type_Reference_v2.01.docx` | Illustrated device and Enhanced code-type reference |
| `docs/cmp-format.md` | CMP groups, inherited rows, credits, output wrapping, and EZ-Flash mapping |
| `docs/compatibility-matrix.md` | Exact, expanded, and suppressed operation mappings |
| `docs/save-output-formats.md` | Native file layouts and limits |
| `docs/ezflash-enhanced-e7.md` | Enhanced 1.06E7 revision-6 standalone/grouped syntax, record costs, and limits |
| `docs/device-rom-code-type-audit.md` | Firmware and Enhanced source audit |
| `NOTICE.md` | Technical-reference acknowledgements |

---

## 📜 License

GBA Cheat Converter is licensed under the:

**GNU General Public License version 3 only — `GPL-3.0-only`**

See `LICENSE` for the complete terms. `NOTICE.md` records projects and
materials used as technical references; those materials are not redistributed
with this project.
