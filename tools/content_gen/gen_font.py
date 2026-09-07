#!/usr/bin/env python3
"""Emits src/presentation/FontData.hpp: two original bitmap faces.

`ui` is a proportional 6x9 face with true lowercase, ascenders and descenders.
Cap height is 6 rows (0-5), x-height 5 rows (1-5), the baseline sits below row 5
and descenders occupy rows 6-7. Advance width is derived from the rightmost lit
pixel, so the face is proportionally spaced rather than monospaced.

`micro` is a compact 4x6 face for dense numeric readouts where the UI face would
be too tall.

Both are authored here as ASCII art so the glyphs stay editable, and both are
original to this project - no third-party font is used or derived from.

    python3 tools/content_gen/gen_font.py
"""
import os

UI = {}
MICRO = {}

UI_W, UI_H = 7, 9
MICRO_W, MICRO_H = 4, 6


def u(ch, *rows):
    """Registers a UI glyph. Missing trailing rows are treated as blank."""
    padded = list(rows) + ["." * UI_W] * (UI_H - len(rows))
    assert len(padded) == UI_H, (ch, len(padded))
    padded = [r.ljust(UI_W, '.') for r in padded]
    for r in padded:
        assert len(r) == UI_W, (ch, r, len(r))
    UI[ch] = padded


def m(ch, *rows):
    padded = list(rows) + ["." * MICRO_W] * (MICRO_H - len(rows))
    assert len(padded) == MICRO_H, (ch, len(padded))
    for r in padded:
        assert len(r) == MICRO_W, (ch, r)
    MICRO[ch] = padded


B = "......."  # blank UI row

# ---------------------------------------------------------------------------
# UI face: uppercase (rows 0-5)
# ---------------------------------------------------------------------------
u('A', '.###..', '#...#.', '#...#.', '#####.', '#...#.', '#...#.')
u('B', '####..', '#...#.', '####..', '#...#.', '#...#.', '####..')
u('C', '.####.', '#.....', '#.....', '#.....', '#.....', '.####.')
u('D', '####..', '#...#.', '#...#.', '#...#.', '#...#.', '####..')
u('E', '#####.', '#.....', '####..', '#.....', '#.....', '#####.')
u('F', '#####.', '#.....', '####..', '#.....', '#.....', '#.....')
u('G', '.####.', '#.....', '#.....', '#..##.', '#...#.', '.####.')
u('H', '#...#.', '#...#.', '#####.', '#...#.', '#...#.', '#...#.')
u('I', '.###..', '..#...', '..#...', '..#...', '..#...', '.###..')
u('J', '..###.', '....#.', '....#.', '....#.', '#...#.', '.###..')
u('K', '#...#.', '#..#..', '###...', '#.#...', '#..#..', '#...#.')
u('L', '#.....', '#.....', '#.....', '#.....', '#.....', '#####.')
u('M', '#...#.', '##.##.', '#.#.#.', '#...#.', '#...#.', '#...#.')
u('N', '#...#.', '##..#.', '#.#.#.', '#..##.', '#...#.', '#...#.')
u('O', '.###..', '#...#.', '#...#.', '#...#.', '#...#.', '.###..')
u('P', '####..', '#...#.', '#...#.', '####..', '#.....', '#.....')
u('Q', '.###..', '#...#.', '#...#.', '#.#.#.', '#..#..', '.##.#.')
u('R', '####..', '#...#.', '#...#.', '####..', '#..#..', '#...#.')
u('S', '.####.', '#.....', '.###..', '....#.', '....#.', '####..')
u('T', '#####.', '..#...', '..#...', '..#...', '..#...', '..#...')
u('U', '#...#.', '#...#.', '#...#.', '#...#.', '#...#.', '.###..')
u('V', '#...#.', '#...#.', '#...#.', '#...#.', '.#.#..', '..#...')
u('W', '#.....#', '#.....#', '#..#..#', '#..#..#', '#.#.#.#', '.#...#.')
u('X', '#...#.', '#...#.', '.#.#..', '.#.#..', '#...#.', '#...#.')
u('Y', '#...#.', '#...#.', '.#.#..', '..#...', '..#...', '..#...')
u('Z', '#####.', '....#.', '...#..', '..#...', '.#....', '#####.')

# Lowercase. x-height occupies rows 1-5; ascenders start at row 0; descenders
# run into rows 6-7.
u('a', B, '.###..', '....#.', '.####.', '#...#.', '.####.')
u('b', '#.....', '#.....', '####..', '#...#.', '#...#.', '####..')
u('c', B, '.###..', '#.....', '#.....', '#.....', '.###..')
u('d', '....#.', '....#.', '.####.', '#...#.', '#...#.', '.####.')
u('e', B, '.###..', '#...#.', '#####.', '#.....', '.###..')
u('f', '..##..', '.#....', '####..', '.#....', '.#....', '.#....')
u('g', B, '.####.', '#...#.', '#...#.', '.####.', '....#.', '.###..')
u('h', '#.....', '#.....', '####..', '#...#.', '#...#.', '#...#.')
u('i', '..#...', B, '.##...', '..#...', '..#...', '.###..')
u('j', '...#..', B, '..##..', '...#..', '...#..', '...#..', '#..#..', '.##...')
u('k', '#.....', '#.....', '#..#..', '###...', '#.#...', '#..#..')
u('l', '.##...', '..#...', '..#...', '..#...', '..#...', '.###..')
u('m', B, '##.##.', '#.#.#.', '#.#.#.', '#.#.#.', '#.#.#.')
u('n', B, '####..', '#...#.', '#...#.', '#...#.', '#...#.')
u('o', B, '.###..', '#...#.', '#...#.', '#...#.', '.###..')
u('p', B, '####..', '#...#.', '#...#.', '####..', '#.....', '#.....')
u('q', B, '.####.', '#...#.', '#...#.', '.####.', '....#.', '....#.')
u('r', B, '#.###.', '##....', '#.....', '#.....', '#.....')
u('s', B, '.####.', '#.....', '.###..', '....#.', '####..')
u('t', '.#....', '.#....', '####..', '.#....', '.#....', '..##..')
u('u', B, '#...#.', '#...#.', '#...#.', '#...#.', '.####.')
u('v', B, '#...#.', '#...#.', '#...#.', '.#.#..', '..#...')
u('w', B, '#...#.', '#...#.', '#.#.#.', '#.#.#.', '.#.#..')
u('x', B, '#...#.', '.#.#..', '..#...', '.#.#..', '#...#.')
u('y', B, '#...#.', '#...#.', '#...#.', '.####.', '....#.', '.###..')
u('z', B, '#####.', '...#..', '..#...', '.#....', '#####.')

# Digits. Authored to a common 5px width so numeric readouts stay tabular.
u('0', '.###..', '#..##.', '#.#.#.', '##..#.', '#...#.', '.###..')
u('1', '..#...', '.##...', '..#...', '..#...', '..#...', '.###..')
u('2', '.###..', '#...#.', '....#.', '..##..', '.#....', '#####.')
u('3', '####..', '....#.', '.###..', '....#.', '#...#.', '.###..')
u('4', '...#..', '..##..', '.#.#..', '#..#..', '#####.', '...#..')
u('5', '#####.', '#.....', '####..', '....#.', '#...#.', '.###..')
u('6', '..##..', '.#....', '####..', '#...#.', '#...#.', '.###..')
u('7', '#####.', '....#.', '...#..', '..#...', '.#....', '.#....')
u('8', '.###..', '#...#.', '.###..', '#...#.', '#...#.', '.###..')
u('9', '.###..', '#...#.', '#...#.', '.####.', '...#..', '.##...')

# Punctuation and symbols.
u(' ')
u('.', B, B, B, B, '.##...', '.##...')
u(',', B, B, B, B, '.##...', '.##...', '.#....')
u(':', B, '.##...', '.##...', B, '.##...', '.##...')
u(';', B, '.##...', '.##...', B, '.##...', '.##...', '.#....')
u('!', '..#...', '..#...', '..#...', '..#...', B, '..#...')
u('?', '.###..', '#...#.', '...#..', '..#...', B, '..#...')
u("'", '..#...', '..#...')
u('"', '.#.#..', '.#.#..')
u('-', B, B, B, '####..')
u('+', B, B, '..#...', '#####.', '..#...')
u('=', B, B, '#####.', B, '#####.')
u('*', B, '#.#.#.', '.###..', '#####.', '.###..', '#.#.#.')
u('/', '....#.', '...#..', '..#...', '..#...', '.#....', '#.....')
u('\\', '#.....', '.#....', '..#...', '..#...', '...#..', '....#.')
u('(', '...#..', '..#...', '.#....', '.#....', '..#...', '...#..')
u(')', '.#....', '..#...', '...#..', '...#..', '..#...', '.#....')
u('[', '.###..', '.#....', '.#....', '.#....', '.#....', '.###..')
u(']', '.###..', '...#..', '...#..', '...#..', '...#..', '.###..')
u('{', '..##..', '.#....', '##....', '.#....', '.#....', '..##..')
u('}', '.##...', '...#..', '...##.', '...#..', '...#..', '.##...')
u('<', B, '...#..', '..#...', '.#....', '..#...', '...#..')
u('>', '.#....', '..#...', '...#..', '...#..', '..#...', '.#....')
u('%', '##..#.', '##.#..', '...#..', '..#...', '.#.##.', '#..##.')
u('#', '.#.#..', '#####.', '.#.#..', '#####.', '.#.#..')
u('_', B, B, B, B, B, B, '#####.')
u('|', '..#...', '..#...', '..#...', '..#...', '..#...', '..#...')
u('^', '..#...', '.#.#..', '#...#.')
u('~', B, B, B, '.##.#.', '#..##.')
u('@', '.###..', '#...#.', '#.###.', '#.#.#.', '#.###.', '#.....', '.###..')
u('$', '..#...', '.####.', '#.#...', '.###..', '..#.#.', '####..', '..#...')
u('&', '.##...', '#..#..', '.##...', '#.#.#.', '#..#..', '.##.#.')

# ---------------------------------------------------------------------------
# Micro face: 4x6, uppercase-mapped, for dense numeric readouts.
# ---------------------------------------------------------------------------
MICRO_GLYPHS = {
    'A': ('.##.', '#..#', '####', '#..#', '#..#'),
    'B': ('###.', '#..#', '###.', '#..#', '###.'),
    'C': ('.###', '#...', '#...', '#...', '.###'),
    'D': ('###.', '#..#', '#..#', '#..#', '###.'),
    'E': ('####', '#...', '###.', '#...', '####'),
    'F': ('####', '#...', '###.', '#...', '#...'),
    'G': ('.###', '#...', '#.##', '#..#', '.###'),
    'H': ('#..#', '#..#', '####', '#..#', '#..#'),
    'I': ('###.', '.#..', '.#..', '.#..', '###.'),
    'J': ('..##', '...#', '...#', '#..#', '.##.'),
    'K': ('#..#', '#.#.', '##..', '#.#.', '#..#'),
    'L': ('#...', '#...', '#...', '#...', '####'),
    'M': ('#..#', '####', '####', '#..#', '#..#'),
    'N': ('#..#', '##.#', '#.##', '#..#', '#..#'),
    'O': ('.##.', '#..#', '#..#', '#..#', '.##.'),
    'P': ('###.', '#..#', '###.', '#...', '#...'),
    'Q': ('.##.', '#..#', '#..#', '#.#.', '.#.#'),
    'R': ('###.', '#..#', '###.', '#.#.', '#..#'),
    'S': ('.###', '#...', '.##.', '...#', '###.'),
    'T': ('####', '.#..', '.#..', '.#..', '.#..'),
    'U': ('#..#', '#..#', '#..#', '#..#', '.##.'),
    'V': ('#..#', '#..#', '#..#', '.##.', '.##.'),
    'W': ('#..#', '#..#', '####', '####', '#..#'),
    'X': ('#..#', '.##.', '.##.', '.##.', '#..#'),
    'Y': ('#..#', '#..#', '.##.', '.#..', '.#..'),
    'Z': ('####', '...#', '.##.', '#...', '####'),
    '0': ('.##.', '#.##', '##.#', '#..#', '.##.'),
    '1': ('.#..', '##..', '.#..', '.#..', '###.'),
    '2': ('.##.', '#..#', '..#.', '.#..', '####'),
    '3': ('###.', '...#', '.##.', '...#', '###.'),
    '4': ('#..#', '#..#', '####', '...#', '...#'),
    '5': ('####', '#...', '###.', '...#', '###.'),
    '6': ('.##.', '#...', '###.', '#..#', '.##.'),
    '7': ('####', '...#', '..#.', '.#..', '.#..'),
    '8': ('.##.', '#..#', '.##.', '#..#', '.##.'),
    '9': ('.##.', '#..#', '.###', '...#', '.##.'),
    ' ': ('....', '....', '....', '....', '....'),
    '.': ('....', '....', '....', '....', '.#..'),
    ',': ('....', '....', '....', '.#..', '#...'),
    ':': ('....', '.#..', '....', '.#..', '....'),
    '-': ('....', '....', '###.', '....', '....'),
    '/': ('...#', '..#.', '.#..', '#...', '#...'),
    '%': ('#..#', '..#.', '.#..', '#..#', '#..#'),
    '+': ('....', '.#..', '###.', '.#..', '....'),
    '<': ('..#.', '.#..', '#...', '.#..', '..#.'),
    '>': ('#...', '.#..', '..#.', '.#..', '#...'),
    '=': ('....', '###.', '....', '###.', '....'),
    '#': ('#.#.', '####', '#.#.', '####', '#.#.'),
    '(': ('..#.', '.#..', '.#..', '.#..', '..#.'),
    ')': ('#...', '.#..', '.#..', '.#..', '#...'),
    '*': ('#.#.', '.#..', '#.#.', '....', '....'),
    '!': ('.#..', '.#..', '.#..', '....', '.#..'),
    '?': ('##..', '..#.', '.#..', '....', '.#..'),
}
for _ch, _rows in MICRO_GLYPHS.items():
    m(_ch, *_rows)


def embolden_rows(rows, width):
    """Widens each stem one pixel to the right without closing a one-pixel
    counter. The few glyphs this still spoils are overridden below."""
    out = []
    for r in rows:
        cols = [c == '#' for c in r]
        new = list(cols)
        for i in range(width):
            if not cols[i]:
                continue
            if i + 1 < width and not cols[i + 1]:
                # Do not fill a gap that has ink on both sides: that is a counter.
                closes_counter = i + 2 < width and cols[i + 2]
                if not closes_counter:
                    new[i + 1] = True
        out.append("".join('#' if v else '.' for v in new))
    return out


# Glyphs whose bold form is drawn by hand because the mechanical widening above
# makes them ambiguous at this size (a smeared 'W' reads as 'N').
BOLD_OVERRIDE_UI = {
    # Three 1px stems cannot all be thickened inside the cell, so the bold W
    # thickens the outer pair and keeps the middle single.
    'W': ('##...##', '##...##', '##.#.##', '##.#.##', '##.#.##', '.##.##.'),
    'w': (B,         '##...##', '##.#.##', '##.#.##', '##.#.##', '.##.##.'),
}


def bold_table(table, width, overrides):
    out = {}
    for ch, rows in table.items():
        if ch in overrides:
            ov = list(overrides[ch]) + ['.' * width] * (len(rows) - len(overrides[ch]))
            assert all(len(r) == width for r in ov), (ch, ov)
            out[ch] = ov[:len(rows)]
        else:
            out[ch] = embolden_rows(rows, width)
    return out


def bits(rows, width):
    """Packs one glyph into per-row bytes; bit (width-1) is the leftmost pixel."""
    out = []
    for r in rows:
        v = 0
        for i, c in enumerate(r):
            if c == '#':
                v |= 1 << (width - 1 - i)
        out.append(v)
    return out


def advance(rows, width, fallback):
    """Proportional advance: one pixel of sidebearing past the rightmost pixel."""
    right = -1
    for r in rows:
        for i, c in enumerate(r):
            if c == '#':
                right = max(right, i)
    if right < 0:
        return fallback
    return min(width, right + 2)


def emit(name, table, width, height, blank_advance):
    lines = []
    lines.append(f"inline constexpr int k{name}Width = {width};")
    lines.append(f"inline constexpr int k{name}Height = {height};")
    lines.append(f"inline constexpr int k{name}First = 32;")
    lines.append(f"inline constexpr int k{name}Count = 95;")
    lines.append("")
    lines.append(f"inline constexpr u8 k{name}Rows[k{name}Count][k{name}Height] = {{")
    advances = []
    for code in range(32, 127):
        ch = chr(code)
        rows = table.get(ch)
        if rows is None:
            rows = table.get(ch.upper())
        if rows is None:
            rows = table[' ']
        vals = ", ".join(f"0x{v:02X}" for v in bits(rows, width))
        label = ch if ch != '\\' else '\\\\'
        lines.append(f"    {{{vals}}}, // '{label}'")
        advances.append(advance(rows, width, blank_advance))
    lines.append("};")
    lines.append("")
    lines.append(f"/// Proportional advance per glyph, in pixels at 1x scale.")
    lines.append(f"inline constexpr u8 k{name}Advance[k{name}Count] = {{")
    for i in range(0, len(advances), 16):
        chunk = ", ".join(str(a) for a in advances[i:i + 16])
        lines.append(f"    {chunk},")
    lines.append("};")
    return lines


OUT = os.path.join(os.path.dirname(__file__), "..", "..", "src", "presentation", "FontData.hpp")

header = [
    "// Generated by tools/content_gen/gen_font.py - do not edit by hand.",
    "//",
    "// Two original bitmap faces, both authored for this project:",
    "//   Ui    - proportional 6x9 with true lowercase, ascenders and descenders",
    "//   Micro - compact 4x6 for dense numeric readouts",
    "//",
    "// No third-party font is used, embedded or derived from.",
    "#pragma once",
    "",
    "#include \"core/Types.hpp\"",
    "",
    "namespace df {",
    "",
    "/// Rows above the baseline in the Ui face; rows below are descenders.",
    "inline constexpr int kUiBaseline = 6;",
    "",
]
UI_BOLD = bold_table(UI, UI_W, BOLD_OVERRIDE_UI)
MICRO_BOLD = bold_table(MICRO, MICRO_W, {})

body = emit("Ui", UI, UI_W, UI_H, 3)
body.append("")
body += emit("UiBold", UI_BOLD, UI_W, UI_H, 3)
body.append("")
body += emit("Micro", MICRO, MICRO_W, MICRO_H, 2)
body.append("")
body += emit("MicroBold", MICRO_BOLD, MICRO_W, MICRO_H, 2)
footer = ["", "} // namespace df"]

with open(os.path.abspath(OUT), "w", encoding="utf-8") as f:
    f.write("\n".join(header + body + footer) + "\n")
print("wrote", os.path.abspath(OUT))
print(f"  ui glyphs: {len(UI)}   micro glyphs: {len(MICRO)}")
