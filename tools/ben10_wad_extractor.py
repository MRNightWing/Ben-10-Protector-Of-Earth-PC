#!/usr/bin/env python3
"""
ben10_wad_extractor.py
======================
Extracts assets from Ben 10: Protector of Earth (PSP) BIGB WAD archives.

Usage:
  # Extract from an already-dumped .r4m / .r4s file:
  python ben10_wad_extractor.py <input.r4m|input.r4s> [output_dir]

  # Extract directly from the ISO (will dump all levels):
  python ben10_wad_extractor.py --iso <path_to.iso> [output_dir]

The script reads BIGB chunk streams and writes each chunk as a separate file.
Chunk filenames follow the pattern:
  <level>_<index>_t<type_hex>[.<ext>]

Known chunk type extensions (Ben 10 PSP):
  Determined by inspecting the first 4 bytes of the chunk data.
"""

import os, sys, struct, argparse
from pathlib import Path

# ---------------------------------------------------------------------------
# ISO 9660 helpers
# ---------------------------------------------------------------------------
SECTOR = 2048

def iso_read(data: bytes, lba: int, size: int) -> bytes:
    off = lba * SECTOR
    return data[off:off + size]

def iso_list_files(data: bytes) -> list[tuple[str, int, int]]:
    """Returns list of (path, lba, size) for all files in ISO 9660 image."""
    pvd = iso_read(data, 16, SECTOR)
    root_lba  = struct.unpack_from("<I", pvd, 156 + 2)[0]
    root_size = struct.unpack_from("<I", pvd, 156 + 10)[0]

    results = []
    _walk_dir(data, root_lba, root_size, "", results)
    return results

def _walk_dir(data, lba, size, path, results):
    sector_data = b""
    for i in range((size + SECTOR - 1) // SECTOR):
        off = (lba + i) * SECTOR
        sector_data += data[off:off + SECTOR]
    sector_data = sector_data[:size]

    pos = 0
    while pos < len(sector_data):
        rec_len = sector_data[pos]
        if rec_len == 0:
            pos = ((pos // SECTOR) + 1) * SECTOR
            if pos >= len(sector_data):
                break
            continue
        flags     = sector_data[pos + 25]
        file_lba  = struct.unpack_from("<I", sector_data, pos + 2)[0]
        file_size = struct.unpack_from("<I", sector_data, pos + 10)[0]
        name_len  = sector_data[pos + 32]
        name_raw  = sector_data[pos + 33: pos + 33 + name_len]
        try:
            name = name_raw.decode("ascii").split(";")[0]
        except Exception:
            name = name_raw.hex()

        is_dir = bool(flags & 0x02)
        full_path = f"{path}/{name}" if name not in ("", "\x00", "\x01") else None
        if full_path:
            if is_dir:
                _walk_dir(data, file_lba, file_size, full_path, results)
            else:
                results.append((full_path, file_lba, file_size))
        pos += rec_len

# ---------------------------------------------------------------------------
# BIGB format
# ---------------------------------------------------------------------------
BIGB_MAGIC = b"BIGB"

# Map chunk type IDs to file extensions (expand as discovered)
CHUNK_EXT = {
    0x00000001: "p3d",   # Pure3D root-like chunk
    0x00000003: "geo",   # Geometry
    0x00000004: "tex",   # Texture
    0x00000005: "anim",  # Animation
    0x00000006: "skel",  # Skeleton
    0x00000007: "cam",   # Camera
    0x00000008: "col",   # Collision
    0x0000000D: "chunk", # Generic/unknown
    0x00000010: "snd",   # Sound
    0x00000020: "phys",  # Physics
    0x00000080: "script",# Script/AI
}

# Common file magic → extension (for inner chunk content detection)
MAGIC_EXT = {
    b"\x89PNG":  "png",
    b"RIFF":     "wav",  # could be WAV or RIFF
    b"GIF8":     "gif",
    b"BM":       "bmp",
    b"\xff\xd8\xff": "jpg",
    b"OggS":     "ogg",
    b"BIGB":     "bigb",  # nested BIGB
}

def detect_ext(data: bytes, chunk_type: int) -> str:
    """Guess a file extension from chunk data magic bytes."""
    if len(data) >= 4:
        for magic, ext in MAGIC_EXT.items():
            if data[:len(magic)] == magic:
                return ext
    return CHUNK_EXT.get(chunk_type, f"t{chunk_type:08x}")


class BigbHeader:
    def __init__(self, raw: bytes):
        assert raw[:4] == BIGB_MAGIC, f"Not a BIGB file (got {raw[:4]!r})"
        self.header_size = struct.unpack_from("<I", raw, 4)[0]
        self.num_chunks  = struct.unpack_from("<I", raw, 8)[0]
        self.version     = struct.unpack_from("<I", raw, 12)[0]

        # Metadata block: starts at offset 16
        meta = raw[16:self.header_size]
        # Extract null-terminated strings (level name, author name, build flags)
        strings = []
        i, s = 0, []
        for b in meta:
            if b == 0:
                if s:
                    strings.append(bytes(s).decode("latin-1", errors="replace"))
                    s = []
            elif 0x20 <= b < 0x7f:
                s.append(b)
            # else: skip non-printable
        self.meta_strings = [x for x in strings if len(x) > 2]

        self.level_name  = self.meta_strings[0] if self.meta_strings else "unknown"
        self.author      = self.meta_strings[1] if len(self.meta_strings) > 1 else ""
        self.build_flags = self.meta_strings[2] if len(self.meta_strings) > 2 else ""

    def __repr__(self):
        return (f"BigbHeader(level={self.level_name!r}, author={self.author!r}, "
                f"num_chunks={self.num_chunks}, header_size=0x{self.header_size:X})")


def parse_chunks(raw: bytes, header: BigbHeader) -> list[tuple[int, int, bytes]]:
    """
    Parse the chunk stream starting at header.header_size.
    Returns list of (chunk_type, offset_in_file, chunk_data).
    
    Chunk format: type(u32_le) + size(u32_le) + data[size bytes]
    Size field appears to INCLUDE the 8-byte header (based on cross-checking).
    """
    chunks = []
    pos = header.header_size
    total = len(raw)

    while pos + 8 <= total:
        chunk_type = struct.unpack_from("<I", raw, pos)[0]
        chunk_size = struct.unpack_from("<I", raw, pos + 4)[0]

        if chunk_size < 8:
            # size includes header; minimum is 8
            # Try treating size as excluding header (alternative encoding)
            data_end = pos + 8 + chunk_size
        else:
            data_end = pos + chunk_size  # size is inclusive of 8-byte header

        if data_end > total:
            print(f"  [!] Chunk at 0x{pos:X}: size 0x{chunk_size:X} exceeds file — stopping")
            break

        if chunk_size >= 8:
            chunk_data = raw[pos + 8: data_end]  # data only (strip the 8-byte header)
        else:
            chunk_data = raw[pos + 8: pos + 8 + chunk_size]

        chunks.append((chunk_type, pos, chunk_data))
        pos = data_end

    return chunks


def extract_wad(raw: bytes, out_dir: Path, prefix: str = "") -> BigbHeader:
    """Extract all chunks from a BIGB raw blob into out_dir."""
    out_dir.mkdir(parents=True, exist_ok=True)

    header = BigbHeader(raw)
    print(f"  BIGB  level={header.level_name!r}  chunks={header.num_chunks}"
          f"  header_size=0x{header.header_size:X}")

    # Save metadata
    meta_path = out_dir / f"{prefix}metadata.txt"
    meta_path.write_text(
        f"Level   : {header.level_name}\n"
        f"Author  : {header.author}\n"
        f"Flags   : {header.build_flags}\n"
        f"Chunks  : {header.num_chunks}\n"
        f"Version : {header.version}\n"
        f"All meta strings:\n" +
        "\n".join(f"  {s!r}" for s in header.meta_strings),
        encoding="utf-8"
    )

    chunks = parse_chunks(raw, header)
    print(f"  Parsed {len(chunks)} chunks (declared: {header.num_chunks})")

    # Count by type for summary
    type_counts: dict[int, int] = {}
    for chunk_type, offset, chunk_data in chunks:
        type_counts[chunk_type] = type_counts.get(chunk_type, 0) + 1

    # Extract each chunk
    for idx, (chunk_type, offset, chunk_data) in enumerate(chunks):
        ext  = detect_ext(chunk_data, chunk_type)
        name = f"{prefix}{idx:04d}_t{chunk_type:08x}.{ext}"
        path = out_dir / name
        path.write_bytes(chunk_data)

    # Print type summary
    print("  Chunk type summary:")
    for t, count in sorted(type_counts.items()):
        ext = CHUNK_EXT.get(t, "?")
        print(f"    type 0x{t:08X}  ({ext})  x{count}")

    return header


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    parser = argparse.ArgumentParser(
        description="Ben 10 PSP BIGB WAD extractor"
    )
    parser.add_argument("input", nargs="?", help="Input .r4m/.r4s file (or --iso mode)")
    parser.add_argument("output", nargs="?", default="wad_extracted",
                        help="Output directory (default: wad_extracted)")
    parser.add_argument("--iso", metavar="ISO",
                        help="Extract directly from PSP ISO image")
    parser.add_argument("--level", metavar="LEVEL",
                        help="Only extract this level (e.g. 11_01) when using --iso")
    parser.add_argument("--list", action="store_true",
                        help="List WAD files in ISO without extracting")
    args = parser.parse_args()

    out_root = Path(args.output)

    if args.iso:
        print(f"Reading ISO: {args.iso}")
        with open(args.iso, "rb") as f:
            iso_data = f.read()

        files = iso_list_files(iso_data)
        wads = [(p, lba, sz) for p, lba, sz in files
                if p.lower().endswith((".r4m", ".r4s"))]

        print(f"Found {len(wads)} WAD files (.r4m/.r4s)")

        if args.list:
            for p, lba, sz in sorted(wads):
                print(f"  {p:60s} {sz:>12,} bytes  LBA={lba}")
            return

        for iso_path, lba, size in sorted(wads):
            level_id = Path(iso_path).stem          # e.g. "11_01"
            wad_type = Path(iso_path).suffix[1:]    # "r4m" or "r4s"

            if args.level and level_id != args.level:
                continue

            print(f"\n--- {iso_path} ({size:,} bytes) ---")
            raw = iso_read(iso_data, lba, size)

            level_dir = out_root / level_id / wad_type
            prefix    = f"{level_id}_{wad_type}_"
            try:
                extract_wad(raw, level_dir, prefix=prefix)
            except AssertionError as e:
                print(f"  [!] Skipped: {e}")

    elif args.input:
        input_path = Path(args.input)
        print(f"Reading: {input_path}")
        raw = input_path.read_bytes()
        wad_type = input_path.suffix[1:]  # r4m or r4s
        level_id = input_path.stem
        out_dir  = out_root / level_id / wad_type
        prefix   = f"{level_id}_{wad_type}_"
        extract_wad(raw, out_dir, prefix=prefix)

    else:
        parser.print_help()
        sys.exit(1)

    print(f"\nDone! Output: {out_root.resolve()}")


if __name__ == "__main__":
    main()
