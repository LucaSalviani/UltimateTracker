"""Genera los sonidos de la app (propios, sin derechos de terceros).

    python tools/make_sounds.py

chime.wav : arpegio ascendente tipo "subiste de nivel" al terminar un pomodoro.
alarm.wav : bip-bip-bip-bip de despertador digital. La app lo repite en loop
            hasta que lo apagás, subiendo el volumen.
"""
import math, struct, wave, pathlib

RATE = 44100
OUT = pathlib.Path(__file__).resolve().parent.parent / "resources" / "sounds"

def write(name, samples):
    OUT.mkdir(parents=True, exist_ok=True)
    peak = max(1e-9, max(abs(s) for s in samples))
    with wave.open(str(OUT / name), "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(RATE)
        w.writeframes(b"".join(struct.pack("<h", int(s / peak * 0.89 * 32767)) for s in samples))

def chime():
    notes = [(659.25, 0.00), (830.61, 0.11), (987.77, 0.22), (1318.51, 0.33)]  # E5 G#5 B5 E6
    total = int(RATE * 1.5)
    out = [0.0] * total
    for f, t0 in notes:
        start = int(t0 * RATE)
        for i in range(int(RATE * 1.1)):
            if start + i >= total: break
            t = i / RATE
            env = min(1.0, t / 0.004) * math.exp(-t * 4.0)
            s = (math.sin(2 * math.pi * f * t)
                 + 0.35 * math.sin(2 * math.pi * 2 * f * t) * math.exp(-t * 6)
                 + 0.12 * math.sin(2 * math.pi * 3.01 * f * t) * math.exp(-t * 9))
            out[start + i] += s * env
    write("chime.wav", out)

def alarm():
    f = 2000.0
    on, off, rest = 0.085, 0.065, 0.45
    seg = []
    for _ in range(4):
        n = int(RATE * on)
        for i in range(n):
            t = i / RATE
            env = min(1.0, t / 0.003, (on - t) / 0.003)
            # onda casi cuadrada (armónicos impares) = timbre de despertador
            s = sum(math.sin(2 * math.pi * f * k * t) / k for k in (1, 3, 5, 7))
            seg.append(s * env)
        seg += [0.0] * int(RATE * off)
    seg += [0.0] * int(RATE * rest)
    write("alarm.wav", seg)

if __name__ == "__main__":
    chime(); alarm(); print("ok ->", OUT)
