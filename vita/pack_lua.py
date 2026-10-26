import os
import struct
import sys

src, dst, prefix = sys.argv[1], sys.argv[2], sys.argv[3]

entries = []
for root, _, files in os.walk(src):
    for name in sorted(files):
        if not name.endswith(".lua"):
            continue
        path = os.path.join(root, name)
        rel = os.path.relpath(path, src).replace(os.sep, "/")
        with open(path, "rb") as f:
            entries.append((prefix + rel, f.read()))

with open(dst, "wb") as out:
    out.write(b"CTHP")
    out.write(struct.pack("<I", len(entries)))
    for name, data in entries:
        encoded = name.encode("utf-8")
        out.write(struct.pack("<I", len(encoded)))
        out.write(encoded)
        out.write(struct.pack("<I", len(data)))
        out.write(data)

print("%s: %d files, %d bytes" % (dst, len(entries), os.path.getsize(dst)))
