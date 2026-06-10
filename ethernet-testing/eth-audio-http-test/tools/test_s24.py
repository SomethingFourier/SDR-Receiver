import time
import os

def s24_to_s32(raw_chunk: bytes) -> bytes:
    n_samples = len(raw_chunk) // 3
    out = bytearray(n_samples * 4)
    out[1::4] = memoryview(raw_chunk)[0::3]
    out[2::4] = memoryview(raw_chunk)[1::3]
    out[3::4] = memoryview(raw_chunk)[2::3]
    return bytes(out)

# 115200 bytes = 100ms
raw = os.urandom(115200)

t0 = time.time()
for _ in range(100): # 10 seconds of audio
    res = s24_to_s32(raw)
t1 = time.time()

print("Time for 10 seconds of audio:", t1 - t0)
