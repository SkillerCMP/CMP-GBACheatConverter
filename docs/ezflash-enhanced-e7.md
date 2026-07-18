# EZ-Flash Omega DE Enhanced E7 format revision 6

GBA Cheat Converter targets the `.cht` grammar implemented by Omega DE Kernel
1.06 Enhanced E7. Every visible code is written as:

```ini
Code Name=command:arguments;more:commands;
```

The first `=` separates the visible code name from its command stream. Enhanced
commands use `:`. A file may contain standalone rows only, or standalone rows
before the first group.

## Standalone codes

```ini
moonjump=IFM:W16,80130,0001,0000;W8:1505C,80;ENDIF;
infinitehealth=W16:10A78,0064;
```

Standalone rows are independent on/off toggles.

## Multi-select groups

```ini
[Movement Codes]
Moon Jump=IFM:W16,80130,0001,0000;W8:1505C,80;ENDIF;
Walk Through Walls=W8:15060,01;
```

A plain `[Group]` is multi-select. Every sibling row is an independent toggle,
so any number may be active together.

## Zero-or-one groups

```ini
[Starting Lives|ONE]
1 Life=W8:15070,01;
3 Lives=W8:15070,03;
9 Lives=W8:15070,09;
```

`[Group|ONE]` allows zero or one selected sibling. Selecting a different row
clears only the previous sibling in that group. Pressing the selected row again
turns it off. The suffix is case-sensitive, must be exactly `|ONE`, and is not
shown in the kernel's visible heading.

The converter retains the distinction in its internal model and through CMP
output. A CMP leaf named `!Starting Lives|ONE:` maps to the E7 zero-or-one mode;
a plain CMP group maps to multi-select.

## Width-aware runtime commands

```text
W8:address,value
W16:address,value
W32:address,value
```

Each direct width-aware write consumes one runtime record. `W16` must be
halfword-aligned and `W32` must be word-aligned.

## Conditions

```text
IF:width,address,value
IFNE:width,address,value
IFLT:width,address,value
IFGT:width,address,value
IFLE:width,address,value
IFGE:width,address,value
```

Masked counterparts append `M` and add a mask field:

```text
IFM:width,address,mask,value
IFNEM:width,address,mask,value
IFLTM:width,address,mask,value
IFGTM:width,address,mask,value
IFLEM:width,address,mask,value
IFGEM:width,address,mask,value
ELSE
ENDIF
```

Masked conditions compare `(memory & mask)` with `value`. Width is `W8`, `W16`,
or `W32`. Ordered comparisons are unsigned. Conditions may nest to 16 levels.

```ini
Moon Jump=IFM:W16,80130,0001,0000;W8:1505C,80;ENDIF;
```

## Arithmetic and pointers

```ini
[Math]
Add=ADD:W32,10A78,00000001;
Subtract=SUB:W16,10A80,0001;
Pointer=PTR:W8,10,00000123,7F;
```

`ADD` and `SUB` consume one record. `PTR` consumes two records.

## Fixed-cost fill and slide

```ini
[Inventory]
Fill=FILL:W32,10000,00000100,11223344;
Slide=SLIDE:W16,20000,00000100,00000002,00000001,1234;
```

`FILL` consumes two runtime records and `SLIDE` consumes four, independent of
count. Generated addresses must remain aligned and inside one compact RAM
region. The maximum operation count is `00010000`.

## ROM guards and patches

```ini
Safe ROM Patch=ROMIF:09014CD3,FD,00;W8:239F8,04;ROM:080EA0C0,00,20;
```

`ROMIF` and `ROM` use Game Pak addresses. Their bytes are stored in pre-launch
tables and consume no runtime records. `ROMIF` must precede runtime actions.
`ROM` cannot appear inside a runtime IF/ELSE branch. A malformed or overflowing
row is rejected transactionally as a complete code.

## Compact address map

| Compact range | Full address range | Use |
|---|---|---|
| `00000-3FFFF` | `02000000-0203FFFF` | EWRAM writes and conditions |
| `40000-47FFF` | `03000000-03007FFF` | IWRAM writes and conditions |
| `80000-803FF` | `04000000-040003FF` | I/O conditions only |

ROM commands accept relative offsets `00000000-03FFFFFF` or full Game Pak
addresses in `08000000-0DFFFFFF`. EWRAM and IWRAM mirrors are normalized to the
compact physical region.

## Runtime record costs

| Operation | Records |
|---|---:|
| `W8`, `W16`, `W32` | 1 |
| normal `IF*` | 1 |
| masked `IF*M` | 2 |
| `ELSE`, `ENDIF` | 1 each |
| `ADD`, `SUB` | 1 |
| `PTR` | 2 |
| `FILL` | 2 |
| `SLIDE` | 4 |
| `ROM`, `ROMIF` | 0 |

Capacity accounting follows E7 selection behavior:

- standalone rows are summed because they may all be active;
- siblings in plain `[Group]` sections are summed because the group is
  multi-select;
- only the largest sibling in each `[Group|ONE]` section contributes to the
  maximum selectable combination.

## Physical and runtime limits

| Limit | Value |
|---|---:|
| Runtime records across selected rows | 128 |
| Runtime writes per pass across selected rows | 4,096 |
| Text condition nesting | 16 |
| Group name | 49 UTF-8 bytes, including `|ONE` physically |
| Code name | 49 UTF-8 bytes |
| Physical database row | 298 visible characters |
| ROM groups | 64 |
| ROM condition bytes | 256 |
| ROM patch bytes | 512 |

The exporter shortens names without splitting UTF-8 sequences, reserves four
bytes for a `|ONE` suffix, and rejects any complete code that cannot be emitted
within the kernel's transactional limits.

## CMP-style groups and credits (v2.15)

Relaxed CMP/database blocks are accepted during device-code conversion:

```text
!Have:
Golden Aku Aku Mask , by MadCatz , Crypt_Codebreaker/GameShark SP/Xploder
320026F0 0002
!!
```

They export as an E7 multi-select group. Inline crypto labels are removed from the 49-byte visible code name, and author metadata becomes a source-safe comment:

```ini
[Have]
// Credits: MadCatz
Golden Aku Aku Mask=W8:26F0,02;
```

`%Credits: Author` attached to a CMP `+Code` entry uses the same comment form. E7 ignores lines beginning with `/`, so `// Credits:` is used rather than backslashes.
