# 🎮 GBA Cheat Converter v2.00

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

---

## ✨ What It Does

GBA Cheat Converter provides one shared conversion engine for the major GBA
cheat-device families:

- **CodeBreaker / GameShark SP / Xploder Advance**
- **GameShark Advance / Action Replay GBX**
- **Action Replay MAX / Pro Action Replay v3**
- **EZ-Flash Original**
- **EZ-Flash Omega DE Enhanced 1.06E3**

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
| 🧹 **Automatic cleanup** | Repairs compact rows, mixed newlines, GameHacking.org blocks, and attached CodeTwink rows. |
| 📝 **Inline conversion notes** | Places compatibility warnings beside the affected code. |
| 🛡️ **Dependency safety** | Never exports controlled writes without their required condition, hook, pointer, patch, or block. |
| 🖥️ **Dual-mode executable** | One Windows executable provides both the GUI and CLI. |
| 🧪 **Regression-tested** | Includes 124 semantic tests for parsers, ciphers, exporters, importers, and containers. |

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
| EZ-Flash Enhanced 1.06E3 | ✅ | — | ✅ | EZ-Flash `.cht` |
| Mednafen | — | — | Direct-write subset | Mednafen `.cht` |
| MiSTer | — | — | Native binary subset | MiSTer `.zip` |

Exact compatibility depends on the operation being converted. Unsupported
dependent operations are suppressed instead of becoming unsafe unconditional
writes.

See `docs/compatibility-matrix.md` for the complete mapping table.

</details>

---

## 📥 Open or Drag Native Cheat Files

`File > Open Input` and drag-and-drop use the same content-based importer.

| File | Input format selected |
|---|---|
| Action Replay MAX `.dsc` | **Encrypted – Action Replay MAX** |
| VisualBoy Advance `.clt` | **RAW – CodeBreaker/FCD** |
| EZ-Flash `.cht` | **EZ-Flash** |
| My Boy! `.cht` | Detected FCD or PAR v3 family |
| mGBA `.cheats` | Detected FCD or PAR v3 family |
| Libretro / RetroArch `.cht` | Detected FCD or PAR v3 family |
| Mednafen `.cht` | **RAW – CodeBreaker/FCD** |
| MiSTer `.zip` | Lossless editable cheat text |

Content signatures take priority over file extensions because several unrelated
applications use `.cht`.

<details>
<summary><strong>🔒 Native importer validation</strong></summary>

The importer checks:

- Container signatures and record counts
- Payload boundaries
- XML and quoted-string values
- ZIP local headers and stored entries
- CRC-32 checksums
- MiSTer masks and data depths
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
- Targets supporting multiple families choose the exact FCD or PAR v3 form per cheat.
- Direct-write-only targets omit conditioned or special entries instead of flattening them.
- Mednafen requests the matching GBA ROM to generate its MD5 section header.

See `docs/save-output-formats.md` for container layouts and limits.

---

## 🔍 Auto Detect

Auto Detect recognizes:

- EZ-Flash `ON=`, IF-family, arithmetic, pointer, slide, and ROM syntax
- Raw and seeded encrypted FCD `8+4`
- Raw and encrypted GameShark Advance / AR GBX `8+8`
- Raw and encrypted Action Replay MAX `8+8`

For ambiguous `8+8` input, candidates are scored using code-type signatures,
payload widths, condition structures, decoded memory ranges, and
family-specific rules. The detector refuses to guess when candidates are too
close.

The resolved family appears in the Input heading, and the status bar displays
the confidence result.

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

## ⚡ EZ-Flash Modes

`Options > EZ-Flash` provides:

- **Original** — stock-compatible `ON=` byte writes
- **Enhanced** — Omega DE Kernel 1.06E3 (Enhanced v3)

<details>
<summary><strong>🧬 Enhanced 1.06E3 commands and examples</strong></summary>

### Runtime writes and IF/ELSE

```ini
[Compare With Else]
IFNE=202,01,00;ON=10A78,CC;ELSE;ON:10A78,64;ENDIF;
```

Supported comparisons:

```text
IF=    Equal
IFNE=  Not equal
IFLT=  Unsigned less than
IFGT=  Unsigned greater than
IFLE=  Unsigned less than or equal
IFGE=  Unsigned greater than or equal
```

### Wide arithmetic

```ini
[Wide Arithmetic]
ADD=10A78,01,02,03,04,05,06;SUB:10A80,01,00;
```

`ADD` and `SUB` carry or borrow across the entire little-endian byte array.

### Pointer writes

```ini
[Pointer Write]
PTR=10,00000123,7F;
```

`PTR` uses one 32-bit pointer, a signed 32-bit offset, and a byte payload.

### Fill and slide

```ini
[Fill]
FILL=10000,00000003,34,12;

[Slide]
SLIDE=10020,00000004,00000002,FFFFFFFF,34,12;
```

### ROM patching

```ini
[ROM Patch]
ROM=080EA0C0,00,20,70,47;

[Guarded Mixed Patch]
ROMIF=09014CD3,FD,00;ON=239F8,04;ROM=080EA0C0,00,20;
```

Enhanced mode supports:

- Arbitrary-length little-endian byte arrays
- IF/ELSE/ENDIF runtime blocks
- Wide ADD/SUB
- One-level PTR
- Transactional FILL/SLIDE expansion
- Pre-launch ROM and ROMIF tables
- Continuation rows
- 49-byte section names
- 298 visible characters per physical line
- One shared 128-record runtime table

Malformed or overflowing dependent entries are suppressed as complete units.

See `docs/ezflash-enhanced-v3.md`.

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

---

## 🖥️ Command-Line Usage

The same Windows executable supports GUI and CLI modes:

```text
GbaCheatConverter.exe                 Open the GUI
GbaCheatConverter.exe --gui           Force GUI mode
GbaCheatConverter.exe --version       Show version
GbaCheatConverter.exe --help          Show CLI usage
```

Example conversion:

```text
GbaCheatConverter.exe --from cb-raw --to ez --ez-mode enhanced codes.txt
```

Auto Detect:

```text
GbaCheatConverter.exe --from auto --to armax-raw codes.txt
```

<details>
<summary><strong>🧾 Common CLI format names</strong></summary>

```text
--from auto
--from cb-raw
--from cb-encrypted
--from gs-raw
--from gs-encrypted
--from armax-raw
--from armax-encrypted
--from ez

--to cb-raw
--to cb-encrypted
--to gs-raw
--to gs-encrypted
--to armax-raw
--to armax-encrypted
--to ez
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

The v2.00 source contains **124 semantic tests** covering:

- Raw and encrypted parsers
- Cipher reseeding
- Code-type mappings
- Conditions and dependent spans
- Native import/export containers
- ZIP and CRC-32 validation
- EZ-Flash Enhanced planning and limits
- Clipboard and metadata cleanup
- Auto Detect confidence
- Inline note placement
- CLI success and failure behavior

---

## 📚 Documentation

| Document | Purpose |
|---|---|
| `GBA_Cheat_Device_Code_Type_Reference_v2.00.docx` | Illustrated device and Enhanced 1.06E3 code-type reference |
| `docs/compatibility-matrix.md` | Exact, expanded, and suppressed operation mappings |
| `docs/save-output-formats.md` | Native file layouts and limits |
| `docs/ezflash-enhanced-v3.md` | Enhanced 1.06E3 syntax and runtime rules |
| `docs/device-rom-code-type-audit.md` | Firmware and Enhanced source audit |
| `NOTICE.md` | Technical-reference acknowledgements |

---

## 📜 License

GBA Cheat Converter is licensed under the:

**GNU General Public License version 3 only — `GPL-3.0-only`**

See `LICENSE` for the complete terms. `NOTICE.md` records projects and
materials used as technical references; those materials are not redistributed
with this project.
