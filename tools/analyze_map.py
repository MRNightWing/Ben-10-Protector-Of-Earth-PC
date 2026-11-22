import struct

def examine_mesh(path):
    with open(path, "rb") as f:
        data = f.read()

    hdr_size = struct.unpack_from("<I", data, 4)[0]
    payload = data[hdr_size:]

    pos = payload.find(b"GBrocks03")
    print(f"Found GBrocks03 at offset {pos} (0x{pos:X})")
    start = max(0, pos - 64)
    end = min(len(payload), pos + 128)
    
    print("Preceding bytes (hex):", payload[start:pos].hex(' '))
    print("Following bytes (hex):", payload[pos:end].hex(' '))

    # Let's inspect floats around here
    num_floats = (end - start) // 4
    floats = struct.unpack_from(f"<{num_floats}f", payload, start)
    print("Floats around object name:")
    for i in range(0, num_floats, 4):
        print(f"  [{start + i*4:05X}]: " + " ".join(f"{f:10.3f}" for f in floats[i:i+4]))

if __name__ == "__main__":
    examine_mesh("assets/PSP_GAME/USRDIR/11_01.r4m")
