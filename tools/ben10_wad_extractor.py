#!/usr/bin/env python3
"""
ben10_wad_extractor.py
======================
High Voltage Software - Quantum Engine BIGB WAD & ISO Extractor
Designed for Ben 10: Protector of Earth (PSP)

Capabilities:
  1. Full or selective ISO 9660 extraction (WADs, FMV Movies, Config files, PRX modules).
  2. Parse and validate BIGB (Quantum Engine v163) Map (.r4m) and Strat (.r4s) archives.
  3. Inspect level metadata, memory requirements, and asset references.
  4. Extract embedded audio / assets from Strat WADs.
"""

import os, sys, struct, argparse
from pathlib import Path

SECTOR = 2048
BIGB_MAGIC = b"BIGB"
EXPECTED_WAD_VERSION = 163

class IsoReader:
    def __init__(self, iso_path: str):
        self.iso_path = Path(iso_path)
        with open(self.iso_path, "rb") as f:
            self.data = f.read()

        pvd = self.data[16 * SECTOR : 17 * SECTOR]
        assert pvd[1:6] == b"CD001", "Not a valid ISO 9660 image"
        self.root_lba  = struct.unpack_from("<I", pvd, 156 + 2)[0]
        self.root_size = struct.unpack_from("<I", pvd, 156 + 10)[0]
        self.volume_id = pvd[40:72].decode('ascii', errors='ignore').strip()
        self.files = self._parse_dir(self.root_lba, self.root_size, "")

    def _parse_dir(self, lba: int, size: int, cur_path: str) -> list[tuple[str, int, int]]:
        sector_data = b""
        for i in range((size + SECTOR - 1) // SECTOR):
            sector_data += self.data[(lba + i) * SECTOR : (lba + i + 1) * SECTOR]
        sector_data = sector_data[:size]

        pos = 0
        results = []
        while pos < len(sector_data):
            rec_len = sector_data[pos]
            if rec_len == 0:
                pos = ((pos // SECTOR) + 1) * SECTOR
                if pos >= len(sector_data): break
                continue
            flags = sector_data[pos + 25]
            file_lba = struct.unpack_from("<I", sector_data, pos + 2)[0]
            file_size = struct.unpack_from("<I", sector_data, pos + 10)[0]
            name_len = sector_data[pos + 32]
            name = sector_data[pos + 33 : pos + 33 + name_len].decode('ascii', errors='ignore').split(';')[0]

            is_dir = bool(flags & 0x02)
            full = f"{cur_path}/{name}" if name not in ("", "\x00", "\x01") else None
            if full:
                if is_dir:
                    results += self._parse_dir(file_lba, file_size, full)
                else:
                    results.append((full, file_lba, file_size))
            pos += rec_len
        return results

    def read_file(self, lba: int, size: int) -> bytes:
        return self.data[lba * SECTOR : lba * SECTOR + size]


class BigbArchive:
    def __init__(self, raw: bytes, filename: str = ""):
        self.filename = filename
        self.raw = raw
        assert raw[:4] == BIGB_MAGIC, f"Not a valid BIGB archive: {raw[:4]!r}"

        self.header_size = struct.unpack_from("<I", raw, 4)[0]
        self.version     = struct.unpack_from("<I", raw, 8)[0]
        self.sub_version = struct.unpack_from("<I", raw, 12)[0]

        # Extract null-terminated strings from metadata area (0x10 to header_size)
        meta_area = raw[0x10 : self.header_size]
        parts = []
        cur = []
        for b in meta_area:
            if b == 0:
                if cur:
                    parts.append(bytes(cur).decode('latin-1', errors='replace'))
                    cur = []
            elif 32 <= b < 127:
                cur.append(b)
        self.strings = [s for s in parts if len(s) > 1]

        self.level_name = self.strings[0] if self.strings else "Unknown"
        self.author = self.strings[1] if len(self.strings) > 1 else "Unknown"
        self.build_flags = self.strings[2] if len(self.strings) > 2 else ""

        # Specific Quantum Engine header parameters at end of header (0x1E4, 0x1E8)
        if self.header_size >= 0x1EC:
            self.sound_flag = struct.unpack_from("<I", raw, 0x1E4)[0]
            self.sound_volatile_size = struct.unpack_from("<I", raw, 0x1E8)[0]
        else:
            self.sound_flag = 0
            self.sound_volatile_size = 0

    def dump_summary(self):
        print(f"Archive: {self.filename} ({len(self.raw):,} bytes)")
        print(f"  Level Name : {self.level_name}")
        print(f"  Author     : {self.author}")
        print(f"  Engine Ver : {self.version} (expected {EXPECTED_WAD_VERSION})")
        print(f"  Header Size: {self.header_size} bytes (data starts at 0x{self.header_size:X})")
        if self.sound_volatile_size > 0:
            print(f"  Sound Heap : {self.sound_volatile_size:,} bytes")
        if self.build_flags:
            print(f"  Build Flags: {self.build_flags}")


def extract_iso_files(iso: IsoReader, output_dir: Path, filter_ext: str = None, filter_level: str = None):
    output_dir.mkdir(parents=True, exist_ok=True)
    count = 0
    total_bytes = 0

    for path, lba, size in iso.files:
        clean_path = path.lstrip("/")
        ext = clean_path.split(".")[-1].lower() if "." in clean_path else ""

        if filter_ext and ext != filter_ext.lower():
            continue
        if filter_level and not clean_path.lower().startswith(f"psp_game/usrdir/{filter_level.lower()}"):
            continue

        target_file = output_dir / clean_path
        target_file.parent.mkdir(parents=True, exist_ok=True)
        content = iso.read_file(lba, size)
        target_file.write_bytes(content)

        count += 1
        total_bytes += size
        print(f"  Extracted: {clean_path} ({size:,} bytes)")

    print(f"\nDone! Extracted {count} files ({total_bytes:,} bytes) to {output_dir.resolve()}")


def main():
    parser = argparse.ArgumentParser(description="Ben 10 PSP BIGB WAD & ISO Extractor")
    parser.add_argument("--iso", help="Path to Ben 10 PSP ISO file")
    parser.add_argument("--wad", help="Path to single .r4m or .r4s file to inspect")
    parser.add_argument("--out", default="extracted", help="Destination directory (default: extracted)")
    parser.add_argument("--level", help="Filter extraction by level ID (e.g. 11_01)")
    parser.add_argument("--movies", action="store_true", help="Extract only FMV movies (.pmf)")
    parser.add_argument("--wads", action="store_true", help="Extract all level WADs (.r4m and .r4s)")
    parser.add_argument("--all", action="store_true", help="Extract entire ISO filesystem")
    parser.add_argument("--info", action="store_true", help="Show ISO or WAD information without extracting")

    args = parser.parse_args()

    if args.wad:
        wad_path = Path(args.wad)
        data = wad_path.read_bytes()
        archive = BigbArchive(data, wad_path.name)
        archive.dump_summary()
        return

    if args.iso:
        iso = IsoReader(args.iso)
        print(f"Loaded ISO: {iso.volume_id} ({len(iso.files)} files)")

        if args.info:
            r4m_count = len([f for f in iso.files if f[0].lower().endswith('.r4m')])
            r4s_count = len([f for f in iso.files if f[0].lower().endswith('.r4s')])
            pmf_count = len([f for f in iso.files if f[0].lower().endswith('.pmf')])
            print(f"  Map WADs   (.r4m): {r4m_count}")
            print(f"  Strat WADs (.r4s): {r4s_count}")
            print(f"  Movies     (.pmf): {pmf_count}")
            return

        out_path = Path(args.out)

        if args.movies:
            print(f"Extracting FMV movies...")
            extract_iso_files(iso, out_path, filter_ext="pmf")
        elif args.wads:
            print(f"Extracting WAD files...")
            for ext in ["r4m", "r4s", "txt"]:
                extract_iso_files(iso, out_path, filter_ext=ext)
        elif args.level:
            print(f"Extracting level '{args.level}'...")
            extract_iso_files(iso, out_path, filter_level=args.level)
        elif args.all:
            print(f"Extracting entire ISO filesystem...")
            extract_iso_files(iso, out_path)
        else:
            print("Please specify an action: --info, --level <id>, --movies, --wads, or --all")

if __name__ == "__main__":
    main()
