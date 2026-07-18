# Compatibility matrix

This matrix describes the release behavior of the semantic converter. “Exact”
means the source operation can be reconstructed in the destination family.
“Expanded” means a compact source operation becomes equivalent ordinary writes.
“Suppressed” means the complete dependent operation or entry is skipped and an
inline compatibility note is emitted; controlled writes are never allowed to
become unconditional.

| Operation | FCD / CodeBreaker / Xploder | GameShark Advance / AR GBX | Action Replay MAX | EZ-Flash Original | EZ-Flash Enhanced |
|---|---|---|---|---|---|
| 8/16/32-bit RAM writes | Exact or width-expanded | Exact | Exact | Expanded to bytes | Exact `W8/W16/W32` where aligned; arbitrary payloads safely split |
| FCD type-4 slide/fill | Exact | Expanded | Expanded | Expanded | Exact fixed-cost `FILL` or `SLIDE` when width/range semantics match |
| FCD type-5 packed list | Exact | Expanded | Expanded | Expanded | Expanded into aligned width-aware writes |
| Equal condition | Exact | Exact where width/form matches | Exact where width/form matches | Suppressed | Exact width-aware `IF` |
| Non-equal / relational condition | Exact FCD variants; B/C are unsigned | Exact EQ/NE/unsigned LE/GE native variants | Exact signed/unsigned native variants | Suppressed | Exact `IFNE/IFLT/IFGT/IFLE/IFGE` when semantics match |
| Compound discontiguous equality | Suppressed | Suppressed | Suppressed | Suppressed | Exact nested width-aware equality conditions |
| GBA KEYINPUT condition | Exact FCD D-type button form | Only when an exact register comparison exists | Only when exact | Suppressed | Exact masked `IFM` for FCD button masks; normal IF when exact |
| Physical cheat-device button | Exact only in its native compatible form | Exact native 8/16-bit form | Exact native 8/16/32-bit form | Suppressed | Suppressed |
| AR MAX block IF without ELSE | Suppressed when not exactly representable | Suppressed when not exactly representable | Exact | Suppressed | Exact when condition/writes fit Enhanced |
| AR MAX block IF/ELSE | Suppressed as one unit | Suppressed as one unit | Exact | Suppressed | Exact Enhanced `ELSE/ENDIF` when the block fits |
| AR MAX pointer write | Suppressed | Suppressed | Exact | Suppressed | Exact one-level `PTR:` when ranges/offset fit |
| GameShark assignment list | Expanded | Exact compact list | Expanded | Expanded | Expanded |
| GameShark type-3 32-bit add/subtract | Suppressed | Exact 8/16/32-bit immediate forms | Add exact; subtract via two's complement when representable | Suppressed | Exact width-aware `ADD:` / `SUB:` when semantics match |
| Dynamic `DEADFACE` key row | FCD uses its native 9-type key system | Exact rolling v1/v2 key | Exact rolling PAR v3 key | Suppressed | Suppressed |
| GameShark ROM patch | Suppressed as a dependency | Exact canonical form | Suppressed | Suppressed | Exact for canonical mode-0 image patches |
| AR MAX ROM patch / slowdown | Suppressed | Suppressed | Exact same-family preservation | Suppressed | ROM patch exact to Enhanced image bytes; slowdown suppressed |
| GameShark `80F00000` slowdown | Suppressed | Exact | Suppressed | Suppressed | Suppressed |
| Hook / master dependency | Exact native rows | Exact where representable | Exact where representable | Suppressed with dependent entry | Suppressed with dependent entry |
| FCD OR / AND | Exact | Suppressed | Suppressed | Suppressed | Suppressed |
| FCD ADD | Exact | Suppressed unless exact native form exists | Exact | Suppressed | Exact `ADD:` when semantics match |

## EZ-Flash modes

- **Original** emits stock byte-list option values.
- **Enhanced** follows Omega DE Kernel 1.06E7 format revision 6: standalone `CodeName=commands;` toggles, multi-select `[Group]`, zero-or-one `[Group|ONE]`, W8/W16/W32 writes, normal and masked IF comparison families, ELSE/ENDIF, width-aware ADD/SUB, one-level PTR, fixed-cost FILL/SLIDE, ROM/ROMIF, continuation rows, 49-byte group/code names, 298-character physical rows, the shared 128-record table, and the 4,096-write-per-pass budget.

## Safety rule

Whenever a condition, button, pointer, patch, hook, or block cannot be encoded
exactly, the converter suppresses its complete controlled/dependent unit and
adds an inline note. It never exports the controlled write by itself.

## Native Save Output As targets

| Target | Exact code families | Safe subset / limits |
|---|---|---|
| EZ-Flash `.cht` | Kernel Original byte lists or Enhanced E7 revision-6 `CodeName=commands;` streams | Follows current EZ-Flash option; kernel name/line/runtime limits enforced |
| Action Replay MAX `.dsc` | Encrypted PAR v3 | Selectable non-master entries; fixed device name limits |
| VisualBoy Advance-M `.clt` | Exact CodeBreaker/FCD or encrypted PAR v3 records with decoded VBA-M fields | Current type-1 84-byte output, legacy type-0 import, variable count, maximum 16,384 records; unsupported entries omitted whole |
| My Boy! `.cht` | Raw FCD (`cb`) and raw PAR v3 (`gs3`) | Per-entry exact-family selection; provisional until upstream parser verification |
| MiSTer `.gg` / `.zip` | Binary `.gg` | Aligned 8/16/32-bit writes plus EQ/NE/GT/GE/LT/LE one-record conditions; multi-write blocks repeat the condition; maximum 32 records per cheat; Stored/Deflate ZIP and raw `.gg` import |
| Mednafen `.cht` | Full native `R/A/T/S/C` records | 1–8-byte LE/BE values, 64-bit compare/value fields, extended repeats/transfers, all condition operators, multiple MD5 sections, exact native re-export; irregular semantic groups omitted rather than split |
| mGBA `.cheats` | CodeBreaker 8+4, GSAv1 raw/encrypted, PAR v3 raw/encrypted, VBA colon rows | Stateful directives, enabled state, mixed/native-only preservation, and exact CLI re-export |
| Libretro / RetroArch `.cht` | Handler-0 core codes plus complete handler-1 native records | Exact imported field preservation; new semantic entries use core-code mode; native memory offsets require core-specific verification |

Any entry outside a target's exact subset is omitted as a whole. Dependent
writes are never emitted without their condition, pointer, hook, or patch.


## Cross-format result classifications

The converter uses these compatibility outcomes:

| Result | Meaning |
|---|---|
| Exact | Native or semantic data is preserved without changing execution behavior. |
| Expanded but equivalent | One source operation becomes multiple target records while retaining behavior, such as repeated MiSTer conditions. |
| Partially representable | Compatible entries are exported and incompatible complete entries are omitted with warnings. |
| Unsupported | The destination cannot safely represent the operation; no uncontrolled writes are emitted. |
| Native-only preservation | Emulator-specific fields are retained for lossless same-format re-export but are not guessed into a device-code family. |

The v2.15 regression matrix retains verification of a direct 8-bit RAM write through AR MAX `.dsc`, VBA-M `.clt`, My Boy! `.cht`, RetroArch `.cht`, mGBA `.cheats`, Mednafen `.cht`, raw MiSTer `.gg`, MiSTer ZIP, and EZ-Flash `.cht`. Complex operations continue to use the per-format rules above.
