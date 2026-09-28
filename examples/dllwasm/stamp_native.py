import pathlib
import sys

p = pathlib.Path(sys.argv[1])
b = bytearray(p.read_bytes())
name = b"native.dll"
body = b"payload.dll\x00"


def uleb(n):
    out = bytearray()
    while True:
        c = n & 0x7F
        n >>= 7
        if n:
            out.append(c | 0x80)
        else:
            out.append(c)
            return bytes(out)


payload = uleb(len(name)) + name + body
p.write_bytes(b + bytes([0]) + uleb(len(payload)) + payload)
