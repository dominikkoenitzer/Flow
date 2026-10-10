# The .rec file format

A `.rec` file is binary. Every field has a fixed width and is little-endian, one after another, with no padding.

## Version 2 (what Flow writes)

Header, 24 bytes:

| Offset | Type | Value |
|---|---|---|
| 0 | 4 bytes | ASCII `FLOW` |
| 4 | u64 | `0xFFFFFFFFFFFFFFFF`, the version marker |
| 12 | u32 | format version, `2` |
| 16 | u64 | number of events |

Then one 36-byte record per event:

| Offset in record | Type | Field |
|---|---|---|
| 0 | u32 | event type, see the table below |
| 4 | i32 | x, screen pixels (mouse events) |
| 8 | i32 | y, screen pixels (mouse events) |
| 12 | u32 | virtual key code (key events) |
| 16 | u64 | time since the recording started, in microseconds |
| 24 | u32 | hardware scan code (key events) |
| 28 | u32 | flags as reported by the Windows low-level hook |
| 32 | i32 | wheel delta, 120 per notch (wheel events), otherwise 0 |

x and y are virtual desktop coordinates, so they can be negative on a monitor left of or above the main one. Key events store 0 for x and y.

Event types:

| Value | Event |
|---|---|
| 0 | mouse move |
| 1 | left button down |
| 2 | left button up |
| 3 | right button down |
| 4 | right button up |
| 5 | middle button down |
| 6 | middle button up |
| 7 | key down |
| 8 | key up |
| 9 | vertical wheel |
| 10 | horizontal wheel |
| 11 | first side button down (usually Back) |
| 12 | first side button up |
| 13 | second side button down (usually Forward) |
| 14 | second side button up |

## Version 1 (older builds)

`FLOW`, then a u64 event count (no marker, no version), then 28-byte records: u32 type, i32 x, i32 y, u32 virtual key, u32 time in whole milliseconds, u32 scan code, u32 flags. Only types 0 to 8 exist. Flow still opens these files and saves them again as version 2.

## What Flow refuses to load

- A file that does not start with `FLOW`.
- A marked file whose version is not 2.
- An event count larger than the rest of the file can hold.
- An event type the version does not know.
- A timestamp smaller than the one before it.

## Reading and writing with Python

```python
import struct

HEADER = struct.Struct("<4sQIQ")   # magic, marker, version, count
EVENT = struct.Struct("<IiiIQIIi")  # type, x, y, vk, time_us, scan, flags, mouse_data

def read_rec(path):
    data = open(path, "rb").read()
    magic, marker, version, count = HEADER.unpack_from(data, 0)
    assert magic == b"FLOW" and marker == 0xFFFFFFFFFFFFFFFF and version == 2
    return [EVENT.unpack_from(data, HEADER.size + i * EVENT.size) for i in range(count)]

def write_rec(path, events):
    out = bytearray(HEADER.pack(b"FLOW", 0xFFFFFFFFFFFFFFFF, 2, len(events)))
    for e in events:
        out += EVENT.pack(*e)
    open(path, "wb").write(out)
```

## Rules when you edit a recording

- Work on a copy and keep the original.
- Keep the timestamps in order; each one must be equal to or larger than the one before.
- Keep every down event paired with its up event, or the button or key stays pressed until playback ends. Flow releases anything still held at the end of a run.
- To change the overall speed, prefer the Speed field in Flow over rewriting timestamps.
