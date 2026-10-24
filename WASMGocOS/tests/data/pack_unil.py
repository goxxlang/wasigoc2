"""Writes the .unil fixtures for ctr_test.

An independent packer: ~/WASMUniLoader/internal/bundle/format.go's layout,
written here without the kernel's or the engine's encoder, so the kernel
reads a bundle it did not pack. Rerun after changing a fixture:

    python tests/data/pack_unil.py
"""
import gzip
import os
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))


def pack(files, level):
    """files: [(name, mime, kind, bytes)]. level None writes a plain payload."""
    payload = b""
    index = b""
    for name, mime, kind, data in files:
        n, m = name.encode(), mime.encode()
        index += struct.pack("<H", len(n)) + n + struct.pack("<H", len(m)) + m
        index += struct.pack("<BBQQI", kind, 0, len(payload), len(data), zlib.crc32(data))
        payload += data
    flags = 0
    if level is not None:
        payload = gzip.compress(payload, compresslevel=level, mtime=0)
        flags = 1
    head = struct.pack("<4sHHIQQ", b"UNIL", 1, flags, len(files), 32, 32 + len(index))
    head += struct.pack("<I", zlib.crc32(head))
    return head + index + payload


def write(name, blob):
    with open(os.path.join(HERE, name), "wb") as f:
        f.write(blob)


notes = "".join(
    "line %d: GocOS containers are UniLoader bundles on the shared cage.\n" % i for i in range(200)
).encode()

write("apps.unil", pack([
    ("service", "text/plain", 12, b"apps from outside the kernel"),
    ("lib/two.js", "text/javascript", 1, b"var two = 2; two"),
    ("app.js", "text/javascript", 1, b"[1, 2, 3].map(x => x * two).join(',')"),
    ("notes.md", "text/markdown", 12, notes),
], 9))

write("plain.unil", pack([
    ("app.js", "text/javascript", 1, b"'plain ' + (6 * 7)"),
], None))

write("stored.unil", pack([
    ("app.js", "text/javascript", 1, b"'stored ' + ['a', 'b', 'c'].reverse().join('')"),
    ("notes.md", "text/markdown", 12, notes),
], 0))
