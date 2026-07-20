# GBA Cheat Converter v2.17 — CLI Command Reference

## Executables

- `GbaCheatConverterCLI.exe` — dedicated Windows console executable. This is the preferred executable for BAT files and scripts.
- `GbaCheatConverter.exe` — Windows dual-mode executable. With no arguments it opens the GUI; with arguments it runs the same CLI engine. `--gui` forces GUI mode.
- Linux/macOS-style builds use `GbaCheatConverterCLI` without `.exe`.

## General syntax

```text
GbaCheatConverterCLI.exe --from INPUT_FORMAT --to OUTPUT_FORMAT [OPTIONS] --output OUTPUT_FILE INPUT_FILE
```

The preferred v2.17 form writes directly to a file:

```bat
GbaCheatConverterCLI.exe --from auto --to vba-clt --output "Game.clt" "Game.cht"
```

`--output` and `-o` are equivalent. Output is generated in memory first and is written only after conversion succeeds. Failed or empty conversions do not replace an existing output file.

Standard-output redirection remains supported:

```bat
GbaCheatConverterCLI.exe --from auto --to vba-clt "Game.cht" > "Game.clt"
```

Use `>` to create or replace the output. **Never use `>>` for binary formats**, because it appends a second binary file to the first and corrupts the result.

Warnings, detection messages, and completion status are written to standard error, so they remain readable on screen while output goes to a file.

## Inspection commands

```text
GbaCheatConverterCLI.exe --help
GbaCheatConverterCLI.exe --version
GbaCheatConverterCLI.exe --list-formats
GbaCheatConverterCLI.exe --detect-only game.cht
```

`--detect-only` prints a tab-separated line containing the canonical format name, human-readable name, and confidence.

## Options

| Option | Meaning |
|---|---|
| `--from FORMAT` | Select input format. Required for conversion. |
| `--to FORMAT` | Select output format. Required for conversion. |
| `--output FILE`, `-o FILE` | Write output directly to a text or binary file. |
| `--detect-only` | Detect the input format without converting. |
| `--list-formats` | Print canonical format names. |
| `--help`, `-h` | Show CLI usage. |
| `--version`, `-V` | Show program version. |
| `--cb-input-key 9XXXXXXX:YYYY` | Supply an input key for encrypted CodeBreaker or Xploder data when no embedded key is present. Alias: `--cb-input-seed`. |
| `--cb-key 9XXXXXXX:YYYY` | Supply the output key required for encrypted CodeBreaker or Xploder output. Alias: `--cb-seed`. |
| `--ez-mode original|enhanced` | Select EZ-Flash export mode. Default: `enhanced`. `cheat-mod` and `mod` are enhanced aliases. |
| `--rom-md5 MD5` | Supply a 32-hex-digit ROM MD5 for new Mednafen output. |
| `--game-name NAME` | Supply a game name for Mednafen or Action Replay MAX `.dsc` output. Quote names containing spaces. |
| `--show-warnings` | Explicitly request warnings. Warnings are enabled by default. |
| `--crypt gsa-v1|par-v3` | Select crypto-only 8+8 conversion. Combine with exactly one of `--encrypt` or `--decrypt`. |
| `--encrypt` / `--decrypt` | Direction for crypto-only conversion. |

Use `-` as the input file, or omit the input path, to read standard input. Use `--output -` to force standard output.

## Canonical format names

### Semantic text formats

- `cb-raw`, `cb-encrypted`
- `gsa-raw`, `gsa-encrypted`
- `armax-raw`, `armax-encrypted`
- `xploder-raw`, `xploder-encrypted`
- `ez`

### Native file formats

- `armax-dsc`
- `vba-clt`
- `myboy-cht` — provisional schema
- `retroarch-cht`
- `mgba-cheats`
- `mednafen-cht`
- `mister-gg` — one cheat
- `mister-zip` — multiple `.gg` files
- `ezflash-cht`

### Automatic input

- `auto` — try native formats first, then semantic text detection.
- `native` — require a recognized native cheat file.

## Binary-output rules

The following destinations are binary:

```text
armax-dsc
vba-clt
mister-gg
mister-zip
```

When binary output would otherwise be printed directly to an interactive Windows console, v2.17 stops with a readable error and asks for `--output FILE` or `> FILE`.

Do not combine standard error with binary standard output using `2>&1`.

## Common examples

```bat
GbaCheatConverterCLI.exe --detect-only "game.cht"

GbaCheatConverterCLI.exe --from auto --to vba-clt ^
  --output "game.clt" "game.cht"

GbaCheatConverterCLI.exe --from auto --to ezflash-cht ^
  --ez-mode enhanced --output "game.cht" "game.txt"

GbaCheatConverterCLI.exe --from cb-raw --to cb-encrypted ^
  --cb-key 9XXXXXXX:YYYY --output "encrypted.txt" "codes.txt"

GbaCheatConverterCLI.exe --from mgba-cheats --to mgba-cheats ^
  --output "cleaned.cheats" "game.cheats"

GbaCheatConverterCLI.exe --from mednafen-cht --to mednafen-cht ^
  --output "cleaned-gba.cht" "gba.cht"

GbaCheatConverterCLI.exe --from cb-raw --to mednafen-cht ^
  --rom-md5 0123456789abcdef0123456789abcdef ^
  --game-name "Game Name" --output "gba.cht" "codes.txt"

GbaCheatConverterCLI.exe --from auto --to mister-zip ^
  --output "game.zip" "game.cht"
```

## Drag-and-drop BAT conversion

The release includes `Convert-CHT-to-CLT.bat`. Put it beside the executables and drag one or more `.cht` files onto it. It uses automatic format detection, writes a `.clt` beside each input, and rejects missing or zero-byte output.

The essential command inside the BAT file is:

```bat
GbaCheatConverterCLI.exe --from auto --to vba-clt --output "Game.clt" "Game.cht"
```

## Exit codes

- `0` — success
- `1` — conversion, detection, argument, file, or export error
- `2` — usage error or attempted binary output to an interactive console

## EZ-Flash Original to Enhanced E7

```bat
GbaCheatConverterCLI.exe --from ezflash-original --to ezflash-enhanced --output "Enhanced.cht" "Original.cht"
```

`--from auto` may be used instead. The migration condenses adjacent writes only when the E7 form is exact.
