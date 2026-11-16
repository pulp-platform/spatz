def crc16(data, poly=0x11021, order=16):
    reg = 0
    for byte in data:
        for i in range(8):
            bit = (byte >> (7-i)) & 1
            reg = (reg << 1) | bit
            if reg & (1 << order):
                reg ^= poly
    for _ in range(order):
        reg <<= 1
        if reg & (1 << order):
            reg ^= poly
    return reg & ((1 << order) - 1)

print(hex(crc16([250,121,204,205,118,133,199,173])))