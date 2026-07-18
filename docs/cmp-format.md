# CMP Database Format

GBA Cheat Converter treats CMP as a database/container layer around the
selected GBA device format. CMP does not replace CodeBreaker, GameShark,
Action Replay MAX, Xploder, or EZ-Flash code types; the rows inside each CMP
entry are still parsed by the selected input family.

## Structure

```text
!Group Name:
$GROUP_HEADER_CODE

+Code Name
%Credits: Author
$CODE_ROW
$CONTINUATION_ROW

!!
```

- `!Group Name:` opens a group.
- `!!` closes the current group.
- `+Code Name` starts a selectable code entry.
- `%Credits:` attaches an author to the active entry.
- `$` prefixes a device code row.
- Code rows before the first `+Code` are group-header rows inherited by every
  entry below that group.
- Groups may be nested. Header rows are inherited outermost-to-innermost.

## Input behavior

CMP text can be pasted, opened, or dragged into the Input editor. The selected
Input format, or Auto Detect after CMP normalization, determines how the `$`
rows are decoded.

Multiline GBA code types remain atomic because CMP removes only the database
markers before handing the rows to the existing device parser. Examples
include FCD slides and packed lists, GameShark assignment lists, and Action
Replay MAX IF/ELSE blocks.

## CMP Output option

`Options > CMP Output` is one checkable menu item.

- Checked: CodeBreaker, GameShark/AR GBX, Action Replay MAX, and Xploder text
  output is wrapped with CMP groups, entry names, credits, `$` rows, and `!!`.
- Unchecked: inherited group-header operations are prepended to each entry and
  normal device text is emitted.
- The checked state is saved in `GbaCheatConverter.ini`.

## EZ-Flash mapping

CMP hierarchy maps to native EZ-Flash Enhanced E7 grouping rather than CMP
punctuation.

CMP:

```text
!Starting Lives:
+One
$32025BC4 0001

+Three
$32025BC4 0003
!!
```

EZ-Flash Enhanced E7 multi-select:

```ini
[Starting Lives]
One=W8:25BC4,01;
Three=W8:25BC4,03;
```

To request E7 zero-or-one behavior, give the CMP leaf an exact `|ONE` suffix:

```text
!Starting Lives|ONE:
```

which exports as `[Starting Lives|ONE]`. Plain CMP groups remain multi-select.

Nested CMP paths are joined with ` / ` because one EZ-Flash section has only
one visible group name. Group-header operations are copied into each EZ-Flash
option because the EZ-Flash database has no independent inherited header row.

## Credits

CMP credits are retained independently from the device code rows:

```text
%Credits: Skiller
```

When source headings contain inline OmniConvert-style metadata such as
`, by Author`, the author is used as CMP credits when no explicit CMP credit is
already present.

## Encrypted FCD seeds

For encrypted CodeBreaker/FCD output, the stream seed row is emitted once as a
CMP group-header row so it remains ahead of every encrypted entry:

```text
!Codes:
$9ABCDEF0 1234
+First Code
$...
!!
```

## Safety

CMP wrapping never reinterprets continuation rows by itself. All code-type
semantics, encryption state, condition spans, dependency suppression, and
warnings continue to come from the selected GBA parser/exporter.
