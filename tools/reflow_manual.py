#!/usr/bin/env python3
"""Reflow the Field Manual body text to the game's 50-column text viewport.

The manual was authored at ~76 columns, but the logical screen is 320px wide and
the 5x7 font advances 6px, so a line holds only 50 characters. Every prose line
therefore wrapped at render time, wasting vertical space and turning numbered
lists into ragged paragraphs.

Rewriting rules, applied per blank-line-delimited block:

  * A block whose lines all already fit is a table, key listing, or short list.
    It is left exactly as authored, which preserves its column alignment.
  * Otherwise the block is split into records. A new record starts wherever the
    line uses internal padding to line a value up in columns, or wherever the
    indent changes. That keeps a run-in heading ("OUTPOSTS SELL      food,
    water, ...") from being absorbed into the paragraph above it, and keeps an
    unindented heading from swallowing the indented prose beneath it.
  * Each record is rejoined and re-wrapped, with its indent clamped, since a
    deep indent leaves too little width to read. Continuations hang two columns
    further in than the record's first row.

Usage: reflow_manual.py <path-to-scenes_manual.cpp>
"""

import re
import sys

MAX_COLS = 50
BODY_START = 28  # 0-based line index of the page table's opening line
MAX_INDENT = 4
HANG = 2


def wrap(text, first_width, rest_width):
    """Greedy wrap of `text` to the given widths for the first and later rows."""
    rows = []
    cur = ""
    width = first_width
    for word in text.split():
        if not cur:
            cur = word
        elif len(cur) + 1 + len(word) <= width:
            cur += " " + word
        else:
            rows.append(cur)
            cur = word
            width = rest_width
    if cur:
        rows.append(cur)
    return rows or [""]


def indent_of(line):
    return len(line) - len(line.lstrip(" "))


def reflow_prose(lines):
    """Re-wrap one prose paragraph at the viewport width."""
    base = min(MAX_INDENT, min(indent_of(line) for line in lines))
    hang = base + HANG
    rows = wrap(" ".join(line.strip() for line in lines), MAX_COLS - base, MAX_COLS - hang)
    return [" " * base + rows[0]] + [" " * hang + row for row in rows[1:]]


def has_column_padding(line):
    """True when a line uses an internal gap to line a value up in columns."""
    return bool(re.search(r"\S {2,}\S", line.lstrip(" ")))


def split_records(block):
    """Split a block into records that must be re-wrapped independently."""
    records = []
    for line in block:
        starts_new = (
            not records
            or has_column_padding(line)
            or indent_of(line) != indent_of(records[-1][0])
        )
        if starts_new:
            records.append([line])
        else:
            records[-1].append(line)
    return records


def reflow_block(block):
    # A block that already fits is authored layout: a table, a key listing, or a
    # list of short lines. Reflowing it would destroy its column alignment.
    if all(len(line) <= MAX_COLS for line in block):
        return list(block)

    out = []
    for record in split_records(block):
        if len(record) == 1 and len(record[0]) <= MAX_COLS:
            out.append(record[0])
        else:
            out.extend(reflow_prose(record))
    return out


def reflow_body(lines):
    out = []
    block = []
    for line in lines:
        if line.strip():
            block.append(line)
        else:
            if block:
                out.extend(reflow_block(block))
                block = []
            out.append("")
    if block:
        out.extend(reflow_block(block))
    return out


def main(path):
    lines = open(path).read().split("\n")

    literal = re.compile(r'^\s*"((?:[^"\\]|\\.)*)",?\s*$')
    out = []
    block = []

    def flush():
        if not block:
            return
        texts = [text for text, _ in block]
        raws = [raw for _, raw in block]
        new_texts = reflow_body(texts)
        if new_texts == texts:
            out.extend(raws)
        else:
            out.append(raws[0].replace('"' + texts[0] + '"', '"' + new_texts[0] + '"', 1))
            out.extend('"%s",' % text for text in new_texts[1:])
        block.clear()

    for idx, raw in enumerate(lines):
        m = literal.match(raw)
        # A page title lives on the `{"TITLE", {` line, which is not a bare
        # literal, so every bare literal here is body text.
        if not m or idx < BODY_START:
            flush()
            out.append(raw)
            continue
        block.append((m.group(1), raw))
    flush()

    open(path, "w").write("\n".join(out))


if __name__ == "__main__":
    main(sys.argv[1])