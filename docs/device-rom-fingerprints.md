# Uploaded device ROM fingerprints

The ROMs are analysis inputs only and are not included in this source package.

| Device ROM | Size | CRC32 | SHA-256 |
|---|---:|---:|---|
| `Action Replay GBX (Europe) (En,Fr,De,It) (Alt 1) (Unl).gba` | 262,144 | `45BB6F4E` | `4e609d57bf080050f56a509cbd0475a3a398a0feb0546fd57361b209151a5e6a` |
| `Action Replay MAX (Europe) (Unl).gba` | 1,048,576 | `3FC75439` | `7f0de64fdfe763940573087159666e4a170e4e451e6a43639a3f56ffefaf9901` |
| `GameShark GBA (USA) (Alt 1) (Unl).gba` | 262,144 | `9AD94C62` | `733f5b0bfc28562a1921a2361e15db90a9b5c060453ec9a83b6b711e3c087ba9` |
| `Xploder Advance (Europe) (Alt 1) (Unl).gba` | 131,072 | `4F3E7427` | `f1a99e2874dfc19b7843a64f28e8588e1dcdf3edf3469c3e3fd8ed178a5eab59` |
| `CodeBreaker (USA) (Unl).gba` | 65,536 | `F5B2F65E` | `25f233be580c705f32f98772b58d9955feee1458183b364e2f8e4ceee0fe800c` |

Initial binary comparison:

- CodeBreaker and Xploder share substantial Future Console Design code, but
  encrypted-code compatibility will remain disabled until verified with known
  encrypted/raw pairs.
- Action Replay GBX and GameShark GBA share substantial Datel-family code.
- Action Replay MAX uses the newer Pro Action Replay v3/v4 family and a
  different semantic code layout from the older GameShark/AR GBX format.
