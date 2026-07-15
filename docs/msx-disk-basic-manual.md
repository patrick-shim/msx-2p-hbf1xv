# MSX Disk-BASIC — Full Reference Manual (Sony HB-F1XV, MSX2+ / MSX-BASIC V3.0)

A complete language reference for the BASIC dialect available on the Sony HB-F1XV: **MSX-BASIC
version 3.0** (the MSX2+ SUB-ROM upgrade of MSX-BASIC 2.0) plus the **Disk-BASIC** extensions
supplied by the DISK ROM. It covers every statement, function, command, and system variable, the
DRAW/PLAY macro languages, sequential and random file I/O, graphics/sprites/sound, event traps,
and the error-code table.

> Grounding: keyword set and syntax are cross-checked against the **MSX2 Technical Handbook,
> Chapter 2 (Konamiman)** and the MSX Wiki, then adapted for the HB-F1XV (MSX2+, V9958 VDP,
> built-in 720 KB drive). MSX2+-only additions (SCREEN 10-12 YJK, etc.) are marked **[2+]**;
> keywords added/changed in BASIC 2.0 keep the handbook's `*`; Disk-BASIC-only keywords are
> marked **[disk]**; MSX-DOS 2 Disk-BASIC extras **[dos2]**.

---

## 1. Getting in and out of BASIC

- **Cold boot with no disk / no DOS**: the machine starts directly in MSX-BASIC (the `Ok` prompt).
- **From MSX-DOS**: type `BASIC` at the `A>` prompt to enter BASIC; optionally `BASIC PROG.BAS`
  to load and run a program. Return to MSX-DOS with `CALL SYSTEM` (or `_SYSTEM`).
- **Direct mode vs program mode**: a line typed **without** a leading line number executes
  immediately (direct/command mode); a line typed **with** a line number (`10 PRINT "HI"`) is
  stored as program text and runs on `RUN`.
- `CALL` may be abbreviated `_` (underscore): `_SYSTEM` ≡ `CALL SYSTEM`.

---

## 2. The screen editor & direct-mode commands

MSX uses a full-screen editor: move the cursor with the arrow keys onto any listed line, edit it,
and press **RETURN** to enter the change.

| Command | Syntax | Function |
|---|---|---|
| `LIST` | `LIST [[<start>]-[<end>]]` | List program to screen (`.` = current line) |
| `LLIST` | `LLIST [[<start>]-[<end>]]` | List program to printer |
| `RUN` | `RUN [<line>]` / `RUN "<file>"[,R]` | Run program (from line, or load+run a disk file) |
| `NEW` | `NEW` | Erase program and all variables |
| `AUTO` | `AUTO [<line>[,<inc>]]` | Auto-generate line numbers while typing |
| `RENUM` | `RENUM [<new>[,<old>[,<inc>]]]` | Renumber lines (fixes GOTO/GOSUB targets) |
| `DELETE` | `DELETE <start>-<end>` | Delete a range of program lines |
| `CONT` | `CONT` | Resume after STOP / CTRL-STOP |
| `TRON` / `TROFF` | `TRON` / `TROFF` | Turn line-number execution trace on/off |
| `CLEAR` | `CLEAR [<string-space>[,<himem>]]` | Clear variables; set string-space bytes & top-of-memory |
| `WIDTH` | `WIDTH <n>` * | Set text columns (SCREEN 0: up to 40/80; SCREEN 1: up to 32) |
| `KEY LIST` | `KEY LIST` | Show the 10 function-key string definitions |

Control keys: **CTRL-STOP** breaks a running program; **STOP** (or CTRL-S) pauses output;
**CTRL-G** beeps; **CTRL-L** clears the screen; **SELECT** and the function keys are user-definable
(see `KEY`).

---

## 3. Program structure

- **Line numbers**: 0-65529. Multiple statements per line separated by `:` — e.g.
  `10 A=1 : B=2 : PRINT A+B`.
- **Comments**: `REM <text>` or `'<text>` (apostrophe) to end of line.
- **Continuation**: none — each logical line is one physical line (max 255 chars in the input
  buffer).

### 3.1 Variables and data types

Names: up to 2 significant characters + type suffix (longer names allowed but only the first two
are significant on classic MSX-BASIC). A type suffix or a `DEF` declaration sets the type:

| Type | Suffix | Range / precision | Bytes |
|---|---|---|---|
| Integer | `%` | −32768 … 32767 | 2 |
| Single-precision real | `!` | ~6 significant digits, ±1E±38 | 4 |
| Double-precision real | `#` | ~14 significant digits | 8 |
| String | `$` | 0-255 characters | var |

- `DEFINT`/`DEFSNG`/`DEFDBL`/`DEFSTR <range>` — e.g. `DEFINT A-H, X` sets the default type for
  variables whose name starts with those letters. Default (no `DEF`, no suffix) is **single**.
- **Constants**: decimal (`3.14`), integer, hex `&H1F`, octal `&O17`, binary `&B1010`; exponent
  form `1.5E3` (single) / `1.5D3` (double).
- **Arrays**: `DIM A(10), B$(4,4)`. Default subscript base is **0** (no `OPTION BASE`); an
  undimensioned array is auto-dimensioned to subscript 10. `ERASE <array>` frees it.

### 3.2 Operators & precedence (high → low)

1. `( )` grouping
2. `^` exponentiation
3. unary `-` (negation)
4. `*` `/` (multiply, real divide)
5. `\` (integer divide)
6. `MOD` (integer remainder)
7. `+` `-` (add/subtract; `+` also concatenates strings)
8. relational: `=` `<>` `<` `>` `<=` `>=`
9. `NOT`
10. `AND`
11. `OR`
12. `XOR` `EQV` `IMP`

Logical/relational results are `−1` (true) or `0` (false); any non-zero value is "true" in `IF`.
Logical operators are **bitwise** on 16-bit integers.

---

## 4. Program flow control

| Keyword | Syntax | Function |
|---|---|---|
| `GOTO` | `GOTO <line>` | Jump |
| `GOSUB` / `RETURN` | `GOSUB <line>` … `RETURN [<line>]` | Call/return a subroutine |
| `ON…GOTO` | `ON <expr> GOTO <l1>,<l2>,…` | Jump to the *n*-th line for `<expr>`=1,2,… |
| `ON…GOSUB` | `ON <expr> GOSUB <l1>,<l2>,…` | Call the *n*-th subroutine |
| `FOR…NEXT` | `FOR V=<a> TO <b> [STEP <s>]` … `NEXT [V[,V2…]]` | Counted loop |
| `WHILE…WEND` | `WHILE <cond>` … `WEND` | Condition-top loop |
| `IF…THEN…ELSE` | `IF <cond> THEN {<stmt>\|<line>} [ELSE {<stmt>\|<line>}]` | Branch |
| `IF…GOTO` | `IF <cond> GOTO <line> [ELSE …]` | Branch variant |
| `END` | `END` | Stop, close files, return to `Ok` |
| `STOP` | `STOP` | Break to `Ok` (resumable with `CONT`) |
| `CONT` | `CONT` | Continue after STOP/break |

`DATA`/`READ`/`RESTORE`: `READ` pulls successive constants from `DATA` lists; `RESTORE [<line>]`
resets the read pointer (optionally to a specific `DATA` line).

`DEF FN`: `DEF FN <name>(<args>)=<expr>` defines a one-line user function, called as `FN<name>(…)`.

---

## 5. Console input / output

| Keyword | Syntax | Function |
|---|---|---|
| `PRINT` | `PRINT [<expr>[{;\|,}<expr>…]]` (short form `?`) | Output; `;`=no gap, `,`=next tab zone |
| `PRINT USING` | `PRINT USING <fmt$>; <expr>[,…]` | Formatted output (see §5.1) |
| `LPRINT` / `LPRINT USING` | as PRINT | Same, to the printer |
| `INPUT` | `INPUT ["<prompt>";] <var>[,<var>…]` | Prompt (`?`) and read comma-separated values |
| `LINE INPUT` | `LINE INPUT ["<prompt>";] <str$>` | Read a whole line (commas included) |
| `INPUT$` | `INPUT$(<n>[,[#]<file>])` | Read exactly `n` chars (no echo, no RETURN needed) |
| `INKEY$` | `INKEY$` | Return the currently-pressed key char, or `""` |
| `LOCATE` | `LOCATE [<x>[,<y>[,<cur>]]]` * | Move text cursor; `<cur>`=1 shows / 0 hides cursor |
| `CLS` | `CLS` | Clear the text screen |
| `TAB` / `SPC` | `TAB(<n>)` / `SPC(<n>)` | Column tab / print n spaces (inside PRINT) |
| `POS` / `CSRLIN` | `POS(0)` / `CSRLIN` | Current cursor column / row |
| `KEY` | `KEY <n>,<s$>` / `KEY {ON\|OFF}` | Define / show-hide function-key strings |

### 5.1 `PRINT USING` format characters

| Field char | Meaning |
|---|---|
| `#` | one digit position |
| `.` | decimal point |
| `,` | insert commas every 3 digits (place left of the point) |
| `+` / `-` | print sign (leading or trailing) |
| `**` | fill leading blanks with `*` |
| `$$` | floating dollar sign |
| `^^^^` | scientific (exponential) notation |
| `!` | first character of a string |
| `\  \` | string field of (2 + spaces-between) characters |
| `&` | whole string, variable length |

Example: `PRINT USING "###.##"; 3.14159` → `  3.14`.

---

## 6. Numeric & math functions

`ABS(x)` `SGN(x)` `INT(x)` (floor) `FIX(x)` (truncate) `SQR(x)` `SIN` `COS` `TAN` `ATN`
`LOG` (natural) `EXP(x)` (e^x) `RND[(x)]` (0≤r<1; `x<0` seeds, `x=0` repeats last, `x>0`/none
advances) `CINT(x)` `CSNG(x)` `CDBL(x)` `FRE(0)` (free BASIC bytes) `FRE("")` (free string
space) `PI` is not built in — use `4*ATN(1)`.

---

## 7. String functions

`LEN(a$)` `LEFT$(a$,n)` `RIGHT$(a$,n)` `MID$(a$,s[,n])` (also as a statement:
`MID$(a$,s[,n])=b$` overwrites in place) `INSTR([start,]a$,b$)` (find substring, 0 if none)
`CHR$(n)` `ASC(a$)` `VAL(a$)` `STR$(x)` `SPACE$(n)` `STRING$(n,{a$\|code})`
`HEX$(n)` `OCT$(n)` `BIN$(n)` (number → base string). Concatenate with `+`.

---

## 8. Graphics

### 8.1 Screen modes (`SCREEN <mode>[,<sprite>[,<click>[,<baud>[,<printer>[,<interlace>]]]]]`)

| SCREEN | Type | Resolution / layout | Colors |
|---|---|---|---|
| 0 | Text | 40×24 (or 80×24 via `WIDTH 80`) | 2 (fg/bg from COLOR) |
| 1 | Text/tile | 32×24 | 16, per 8-char group |
| 2 | Graphic | 256×192 | 16, per 8-pixel row |
| 3 | Multicolor | 64×48 blocks | 16 |
| 4 | Graphic | 256×192 (MSX2 sprites) | 16 of 512 palette |
| 5 | Graphic | 256×212 | 16 of 512 |
| 6 | Graphic | 512×212 | 4 of 512 |
| 7 | Graphic | 512×212 | 16 of 512 |
| 8 | Graphic | 256×212 | 256 (fixed GRB) |
| 10 **[2+]** | Graphic YJK+YAE | 256×212 | 12499 + 16 palette |
| 11 **[2+]** | Graphic YJK+YAE | 256×212 | 12499 + 16 |
| 12 **[2+]** | Graphic YJK | 256×212 | 19268 natural colors |

`<sprite>`: 0=8×8/single, 1=8×8/double, 2=16×16/single, 3=16×16/double.

### 8.2 Colour & palette

- `COLOR [<fg>[,<bg>[,<border>]]]` * — set text/drawing colours.
- `COLOR=(<pal>,<r>,<g>,<b>)` * — program palette entry `<pal>` (0-15) with 3-bit R/G/B (0-7).
- `COLOR=NEW` / `COLOR=RESTORE` * — reset palette to power-on / restore saved.

### 8.3 Drawing primitives (coordinate origin top-left; `STEP(dx,dy)` = relative)

| Keyword | Syntax |
|---|---|
| `PSET` | `PSET (x,y)[,<col>[,<op>]]` — plot a dot |
| `PRESET` | `PRESET (x,y)[,<col>[,<op>]]` — erase a dot |
| `LINE` | `LINE [(x1,y1)]-(x2,y2)[,<col>[,{B\|BF}[,<op>]]]` — line / box / filled box |
| `CIRCLE` | `CIRCLE (x,y),<r>[,<col>[,<start>[,<end>[,<aspect>]]]]` — circle/arc/ellipse |
| `PAINT` | `PAINT (x,y)[,<col>[,<border>]]` — flood-fill |
| `POINT` | `POINT(x,y)` — return the colour at (x,y) |
| `DRAW` | `DRAW <macro$>` — turtle-graphics macro (see §8.4) |

`<op>` logical operators: `PSET`(replace), `PRESET`, `AND`, `OR`, `XOR`, `TAND`, `TOR`, `TXOR`,
`TPSET`, `TPRESET` (the `T…` forms treat colour 0 as transparent).

### 8.4 `DRAW` macro language

Commands in the string: `U`n up, `D`n down, `L`n left, `R`n right, `E/F/G/H`n diagonals (n dots);
`M x,y` move (absolute; prefix `+`/`-` for relative); `B` = move only (blank, no draw); `N` = draw
then return to start; `C`n = colour n; `A`n = angle (0-3 → 0/90/180/270°); `S`n = scale (n/4);
`X<var$>;` = execute another DRAW string variable. Numbers may be variables via `=<var>;`.
Example: `DRAW "C15 R50 D30 L50 U30"` draws a rectangle in colour 15.

### 8.5 Screen/VRAM helpers

- `COPY <src> TO <dst>` * / `COPY SCREEN [<mode>]` * — block-copy between screen rectangle, array,
  or disk file (`COPY (x1,y1)-(x2,y2) TO (x3,y3)` etc.).
- `SET PAGE <display>,<active>` * — choose which VRAM page is shown vs drawn (SCREEN 5-8 have
  multiple pages → smooth page-flip animation).
- `SET ADJUST (<x>,<y>)` * — shift the whole display −7…+8 pixels (also the R#18 register).
- `BASE(<n>)` * — VRAM table base addresses (name, pattern, colour, sprite tables per mode).
- `VDP(<r>)` * — read/write V9958 register `<r>`; `VPEEK(<addr>)` / `VPOKE <addr>,<d>` * —
  read/write VRAM directly.

---

## 9. Sprites

- `SPRITE$(<n>) = <pattern$>` — define sprite pattern plane `<n>` (8 or 32 bytes).
- `PUT SPRITE <plane>[,(x,y)|STEP(dx,dy)][,<col>[,<pattern#>]]` * — display a sprite plane.
- `COLOR SPRITE(<plane>) = <col>` * — one colour for the whole sprite.
- `COLOR SPRITE$(<plane>) = <colours$>` * — per-scanline colours (MSX2 line-colour sprites).
- Collision trapping: `ON SPRITE GOSUB <line>` + `SPRITE {ON\|OFF\|STOP}` (see §14).

---

## 10. Sound & music

- `BEEP` — the click/beep; timbre/volume via `SET BEEP <timbre 1-4>,<volume 1-4>` *.
- `SOUND <reg>,<data>` — write the PSG (AY-3-8910/YM2149) register directly (reg 0-13:
  0/1 tone A period, 2/3 B, 4/5 C, 6 noise, 7 mixer, 8-10 volumes, 11/12 envelope period,
  13 envelope shape).
- `PLAY <s1$>[,<s2$>[,<s3$>]]` — play up to 3 PSG voices via **MML** (Music Macro Language):
  - `A`-`G`[#/+/-][len] notes; `N`n note number; `R`len rest; `O`n octave (1-8); `<`/`>` octave
    down/up; `L`n default length (1-64); `T`n tempo (32-255); `V`n volume (0-15); `S`n envelope
    shape; `M`n envelope period; `.`= dotted; `&`= tie; `X<var$>;` execute substring.
  - `PLAY(<voice>)` (function) returns −1 while that voice is still sounding, else 0.
- **Built-in MSX-MUSIC (YM2413/OPLL) FM**: driven from BASIC via the FM-BIOS extensions —
  `CALL MUSIC`, then `PLAY #n,"…"` with FM MML and preset instruments `@0-@63`, rhythm on
  channel `#10`. See the repo's `references/music_in_basic.md` for worked examples.

---

## 11. Input devices (joystick / paddle / mouse-pad)

`STICK(<n>)` (0=keyboard arrows, 1/2=joystick port) → direction 0-8. `STRIG(<n>)` → trigger
state (−1 pressed / 0 not). `PDL(<n>)` → paddle value. `PAD(<n>)` * → touch-tablet / mouse /
light-pen / trackball status. Trigger trapping via `ON STRIG GOSUB` + `STRIG(n) {ON\|OFF\|STOP}`.

---

## 12. Clock / calendar (RP5C01 RTC)

`GET DATE <a$>[,A]` * and `SET DATE <a$>[,A]` * — read/set the date (`"YY/MM/DD"`).
`GET TIME <a$>[,A]` * and `SET TIME <a$>[,A]` * — read/set the time (`"HH:MM:SS"`).
`TIME` — the free-running interval counter (frames since power-on; `TIME=0` resets it).

---

## 13. Machine-level access

| Keyword | Syntax | Function |
|---|---|---|
| `PEEK` / `POKE` | `PEEK(<addr>)` / `POKE <addr>,<d>` | Read / write CPU memory |
| `INP` / `OUT` | `INP(<port>)` / `OUT <port>,<d>` | Read / write an I/O port |
| `WAIT` | `WAIT <port>,<and>[,<xor>]` | Spin until `(INP(port) XOR xor) AND and` ≠ 0 |
| `DEF USR` | `DEF USR[<n>]=<addr>` | Set entry of machine-code routine `<n>` (0-9) |
| `USR` | `USR[<n>](<arg>)` | Call that routine, passing/returning one value |
| `CALL` | `CALL <name>[(<args>)]` (≡ `_<name>`) | Invoke a cartridge/extension statement |
| `VARPTR` | `VARPTR(<var>)` / `VARPTR(#<file>)` | Address of a variable / of a file control block |

---

## 14. Interrupts & event trapping

Define a handler, then enable trapping. A trapped event does a hidden `GOSUB`; end the handler
with `RETURN`. `…OFF` disables; `…STOP` defers (the event is remembered and fires when `…ON`
resumes).

| Event | Define | Enable/disable |
|---|---|---|
| Interval timer | `ON INTERVAL=<n> GOSUB <line>` | `INTERVAL {ON\|OFF\|STOP}` |
| Function key | `ON KEY GOSUB <l1>,…` | `KEY(<n>) {ON\|OFF\|STOP}` |
| Sprite collision | `ON SPRITE GOSUB <line>` | `SPRITE {ON\|OFF\|STOP}` |
| CTRL-STOP | `ON STOP GOSUB <line>` | `STOP {ON\|OFF\|STOP}` |
| Joystick trigger | `ON STRIG GOSUB <l1>,…` | `STRIG(<n>) {ON\|OFF\|STOP}` |

---

## 15. Cassette (legacy; HB-F1XV has a cassette interface)

`CSAVE "<name>"[,<baud>]` / `CLOAD ["<name>"]` / `CLOAD?` (verify) — save/load/verify a BASIC
program on tape. `MOTOR [{ON\|OFF}]` toggles the tape motor relay. `SAVE`/`LOAD` with a
`CAS:`-style device also target tape on a stock MSX. (Disk is the normal path on the HB-F1XV.)

---

## 16. Disk & file I/O  **[disk]**

### 16.1 File management

| Keyword | Syntax | Function |
|---|---|---|
| `FILES` / `LFILES` | `FILES ["<pattern>"][,L]` ** | Directory to screen / printer (`,L` = long: size+attrs) |
| `LOAD` | `LOAD "<file>"[,R]` | Load a BASIC program (`,R` = then run) |
| `SAVE` | `SAVE "<file>"[,A]` | Save program (tokenised; `,A` = ASCII text) |
| `MERGE` | `MERGE "<file>"` | Merge an ASCII-saved program into memory |
| `RUN` | `RUN "<file>"[,R]` | Load and run |
| `KILL` | `KILL "<file>"` | Delete a file |
| `NAME` | `NAME "<old>" AS "<new>"` | Rename a file |
| `BSAVE` | `BSAVE "<file>",<start>,<end>[,<exec>\|S]` | Save a raw memory block (`,S` = VRAM) |
| `BLOAD` | `BLOAD "<file>"[,R[,<offset>]]` | Load a raw block (`,R` = then execute) |
| `DSKF` | `DSKF(<drive>)` | Free clusters on the drive |
| `DSKI$`/`DSKO$` | `DSKI$(<drv>,<sec>)` / `DSKO$(<drv>,<sec>)` | Raw read/write of a 512-byte sector |
| `CALL FORMAT` | `CALL FORMAT` | Format a floppy |

Filenames follow MSX-DOS 8.3 (`NAME.EXT`); drive prefix `A:`…`H:`; `*`/`?` wildcards in `FILES`.

### 16.2 Sequential files

```
OPEN "<file>" FOR {INPUT|OUTPUT|APPEND} AS #<n>
PRINT #<n>, <expr>[;/,<expr>…]      ' write (or PRINT #<n>, USING <fmt$>; …)
INPUT #<n>, <var>[,<var>…]          ' read comma/CR-delimited fields
LINE INPUT #<n>, <str$>             ' read a whole line
a$ = INPUT$(<len>, #<n>)            ' read exactly len bytes
CLOSE [#<n>[,#<n>…]]
```
Status functions: `EOF(<n>)` (−1 at end), `LOC(<n>)` (record/position), `LOF(<n>)` (length).
`MAXFILES=<count>` sets how many files may be open at once (default 1; raise before OPENing many).

### 16.3 Random-access files

```
OPEN "<file>" AS #<n> [LEN=<reclen>]      ' random mode (no FOR clause)
FIELD #<n>, <w1> AS f1$, <w2> AS f2$, …   ' map buffer fields (fixed widths)
LSET f1$ = a$   /   RSET f2$ = STR$(x)    ' put data into the buffer (left/right justified)
PUT #<n>[,<record>]                       ' write the buffer as record <record>
GET #<n>[,<record>]                       ' read a record into the buffer
CLOSE #<n>
```
Numbers ↔ fixed-width strings for the buffer: `MKI$/MKS$/MKD$` (integer/single/double → 2/4/8-byte
string) and back with `CVI/CVS/CVD`.

### 16.4 MSX-DOS 2 Disk-BASIC extras  **[dos2]**

`CALL CHDRV("A:")` set drive · `CALL CHDIR("\PATH")` change dir · `CALL MKDIR("DIR")` /
`CALL RMDIR("DIR")` make/remove dir · `CALL RAMDISK(<size>)` create a RAM disk ·
`CALL SYSTEM[("<cmd>")]` return to MSX-DOS (optionally run a command).
(RAM-disk BASIC helpers on some kits: `CALL MEMINI/MFILES/MKILL/MNAME`.)

---

## 17. Error handling

```
ON ERROR GOTO <line>     ' install a handler (ON ERROR GOTO 0 disables/re-raises)
… ERROR <code>           ' raise an error deliberately
… RESUME [0]             ' retry the offending statement
… RESUME NEXT            ' continue at the statement after it
… RESUME <line>          ' continue at a specific line
```
Read-only in the handler: `ERR` (error code) and `ERL` (line number where it happened).

### 17.1 Error codes

| # | Message | # | Message |
|---|---|---|---|
| 1 | NEXT without FOR | 14 | Out of string space |
| 2 | Syntax error | 15 | String too long |
| 3 | RETURN without GOSUB | 16 | String formula too complex |
| 4 | Out of DATA | 17 | Can't CONTINUE |
| 5 | Illegal function call | 18 | Undefined user function |
| 6 | Overflow | 19 | Device I/O error |
| 7 | Out of memory | 20 | Verify error |
| 8 | Undefined line number | 21 | No RESUME |
| 9 | Subscript out of range | 22 | RESUME without error |
| 10 | Redimensioned array | 23 | Unprintable error |
| 11 | Division by zero | 24 | Missing operand |
| 12 | Illegal direct | 25 | Line buffer overflow |
| 13 | Type mismatch | | |

Disk-BASIC / MSX-DOS errors (50+): 50 FIELD overflow · 51 Internal error · 52 Bad file number ·
53 File not found · 54 File already open · 55 Input past end · 56 Bad file name · 57 Direct
statement in file · 58 Sequential I/O only · 59 File not OPEN · 60 Bad FAT · 61 Bad file mode ·
62 Bad drive name · 63 Bad sector number · 64 File still open · 65 File already exists · 66 Disk
full · 67 Too many files · 68 Disk write-protected · 69 Disk I/O error · 70 Disk offline ·
71 Rename across disks. **[dos2]** adds 72-75 (e.g. directory already exists / not found, etc.).

---

## 18. Quick appendices

### 18.1 Useful control codes (CHR$)

`CHR$(7)` bell · `CHR$(11)` home · `CHR$(12)` clear screen · `CHR$(13)` CR · `CHR$(28-31)`
cursor right/left/up/down · `CHR$(8)` backspace.

### 18.2 Typing `:` and `*` on this emulator

On the HB-F1XV keyboard `*` is **Shift + the `:`/`*` key**. In this emulator that key is bound to
**Right-Ctrl**: `Right-Ctrl` → `:`, `Shift + Right-Ctrl` → `*`.

### 18.3 Handy one-liners

```
10 SCREEN 5 : COLOR 15,1,1 : CLS
20 LINE (10,10)-(200,150),8,B
30 CIRCLE (128,106),40,12
40 PAINT (128,106),12,12
50 A$=INKEY$ : IF A$="" THEN 50
```

---

*Reference compiled for the Sony HB-F1XV emulator. Keyword set/syntax grounded in the MSX2
Technical Handbook (Konamiman) and MSX Wiki; MSX2+/V3.0 and HB-F1XV specifics added. This is a
concise reference, not a tutorial — consult the original MSX2/2+ manuals for full narrative
explanations.*
