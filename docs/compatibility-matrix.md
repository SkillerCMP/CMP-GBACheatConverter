# Compatibility matrix

This matrix describes the release behavior of the semantic converter. “Exact”
means the source operation can be reconstructed in the destination family.
“Expanded” means a compact source operation becomes equivalent ordinary writes.
“Suppressed” means the complete dependent operation or entry is skipped and an
inline compatibility note is emitted; controlled writes are never allowed to
become unconditional.

| Operation | FCD / CodeBreaker / Xploder | GameShark Advance / AR GBX | Action Replay MAX | EZ-Flash Original | EZ-Flash Enhanced |
|---|---|---|---|---|---|
| 8/16/32-bit RAM writes | Exact or width-expanded | Exact | Exact | Expanded to bytes | Expanded to bytes |
| FCD type-4 slide/fill | Exact | Expanded | Expanded | Expanded | Expanded |
| FCD type-5 packed list | Exact | Expanded | Expanded | Expanded | Expanded |
| Equal condition | Exact | Exact where width/form matches | Exact where width/form matches | Suppressed | Exact byte-equality IF |
| Non-equal / relational condition | Exact FCD variants; B/C are unsigned | Exact EQ/NE/unsigned LE/GE native variants | Exact signed/unsigned native variants | Suppressed | Exact `IFNE/IFLT/IFGT/IFLE/IFGE` when semantics match |
| Compound discontiguous equality | Suppressed | Suppressed | Suppressed | Suppressed | Exact Enhanced byte-equality AND group |
| GBA KEYINPUT condition | Exact FCD button form | Only when an exact register comparison exists | Only when exact | Suppressed | Exact read-only I/O IF when byte-equality compatible |
| Physical cheat-device button | Exact only in its native compatible form | Exact native 8/16-bit form | Exact native 8/16/32-bit form | Suppressed | Suppressed |
| AR MAX block IF without ELSE | Suppressed when not exactly representable | Suppressed when not exactly representable | Exact | Suppressed | Exact when condition/writes fit Enhanced |
| AR MAX block IF/ELSE | Suppressed as one unit | Suppressed as one unit | Exact | Suppressed | Exact Enhanced `ELSE/ENDIF` when the block fits |
| AR MAX pointer write | Suppressed | Suppressed | Exact | Suppressed | Exact one-level `PTR=` when ranges/offset fit |
| GameShark assignment list | Expanded | Exact compact list | Expanded | Expanded | Expanded |
| GameShark type-3 32-bit add/subtract | Suppressed | Exact 8/16/32-bit immediate forms | Add exact; subtract via two's complement when representable | Suppressed | Exact `ADD=` / `SUB=` byte arrays when semantics match |
| Dynamic `DEADFACE` key row | FCD uses its native 9-type key system | Exact rolling v1/v2 key | Exact rolling PAR v3 key | Suppressed | Suppressed |
| GameShark ROM patch | Suppressed as a dependency | Exact canonical form | Suppressed | Suppressed | Exact for canonical mode-0 image patches |
| AR MAX ROM patch / slowdown | Suppressed | Suppressed | Exact same-family preservation | Suppressed | ROM patch exact to Enhanced image bytes; slowdown suppressed |
| GameShark `80F00000` slowdown | Suppressed | Exact | Suppressed | Suppressed | Suppressed |
| Hook / master dependency | Exact native rows | Exact where representable | Exact where representable | Suppressed with dependent entry | Suppressed with dependent entry |
| FCD OR / AND | Exact | Suppressed | Suppressed | Suppressed | Suppressed |
| FCD ADD | Exact | Suppressed unless exact native form exists | Exact | Suppressed | Exact `ADD=` when semantics match |

## EZ-Flash modes

- **Original** emits only `ON=` byte writes.
- **Enhanced** follows Omega DE Kernel 1.06 Enhanced v3, including all IF comparison families, ELSE/ENDIF, arbitrary-byte ADD/SUB, one-level PTR, FILL/SLIDE, ROM/ROMIF, continuation rows, 49-byte section names, 298-character physical rows, and the shared 128-runtime-record table.

## Safety rule

Whenever a condition, button, pointer, patch, hook, or block cannot be encoded
exactly, the converter suppresses its complete controlled/dependent unit and
adds an inline note. It never exports the controlled write by itself.

## Native Save Output As targets

| Target | Exact code families | Safe subset / limits |
|---|---|---|
| EZ-Flash `.cht` | Kernel Original ON-only or Enhanced v3 IF/ON/ROM | Follows current EZ-Flash option; kernel name/line/runtime limits enforced |
| Action Replay MAX `.dsc` | Encrypted PAR v3 | Selectable non-master entries; fixed device name limits |
| VisualBoy Advance `.clt` | Decoded direct writes | 8/16/32-bit writes only; maximum 100 records |
| My Boy! `.cht` | Raw FCD (`cb`) and raw PAR v3 (`gs3`) | Per-entry exact-family selection |
| MiSTer `.zip` | Binary `.gg` | Direct writes and equality-controlled writes; maximum 128 records per cheat |
| Mednafen `.cht` | Decoded direct writes | Requires matching ROM MD5; independent writes only |
| mGBA `.cheats` | Raw FCD and encrypted PAR v3 | Per-entry exact-family selection; cheats disabled by default |
| Libretro `.cht` | Raw FCD and raw PAR v3 | Per-entry exact-family selection; cheats disabled by default |

Any entry outside a target's exact subset is omitted as a whole. Dependent
writes are never emitted without their condition, pointer, hook, or patch.
