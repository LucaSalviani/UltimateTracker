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

# ---- Alternativas (M27) -----------------------------------------------------
def tone(out, f, t0, dur, decay, amp=1.0, partials=((1, 1.0),), attack=0.004):
    start = int(t0 * RATE)
    for i in range(int(RATE * dur)):
        if start + i >= len(out): break
        t = i / RATE
        env = min(1.0, t / attack) * math.exp(-t * decay)
        out[start + i] += amp * env * sum(a * math.sin(2 * math.pi * f * k * t) for k, a in partials)

def chime_soft():      # dos notas suaves y largas
    out = [0.0] * int(RATE * 1.8)
    tone(out, 1046.5, 0.00, 1.6, 2.6, partials=((1, 1.0), (2, 0.12)), attack=0.02)
    tone(out, 1568.0, 0.18, 1.5, 2.6, partials=((1, 1.0), (2, 0.10)), attack=0.02)
    write("chime_soft.wav", out)

def chime_bell():      # una campana (parciales inarmónicos)
    out = [0.0] * int(RATE * 2.2)
    tone(out, 880.0, 0.0, 2.1, 1.8, partials=((1, 1.0), (2.76, 0.45), (5.40, 0.22), (8.93, 0.08)))
    write("chime_bell.wav", out)

def chime_wood():      # dos golpes de madera
    import random
    random.seed(7)
    out = [0.0] * int(RATE * 0.6)
    for t0 in (0.0, 0.14):
        start = int(t0 * RATE)
        for i in range(int(RATE * 0.12)):
            t = i / RATE
            env = math.exp(-t * 45)
            s = 0.8 * math.sin(2 * math.pi * 920 * t) + 0.35 * math.sin(2 * math.pi * 1840 * t) + 0.25 * (random.random() * 2 - 1) * math.exp(-t * 200)
            out[start + i] += s * env
    write("chime_wood.wav", out)

def chime_digital():   # bip-bip de reloj digital, subiendo
    out = [0.0] * int(RATE * 0.5)
    for t0, f in ((0.0, 2093.0), (0.16, 2637.0)):
        start = int(t0 * RATE)
        n = int(RATE * 0.09)
        for i in range(n):
            t = i / RATE
            env = min(1.0, t / 0.002, (0.09 - t) / 0.004)
            out[start + i] += env * sum(math.sin(2 * math.pi * f * k * t) / k for k in (1, 3, 5))
    write("chime_digital.wav", out)

def achievement_soft():   # un acorde mayor que crece y se apaga
    out = [0.0] * int(RATE * 1.8)
    for f in (523.25, 659.25, 783.99, 1046.5):
        tone(out, f, 0.0, 1.7, 2.2, amp=0.6, partials=((1, 1.0), (2, 0.15)), attack=0.18)
    write("achievement_soft.wav", out)

def alarm_soft():      # marimba: tres notas que se repiten (la app lo pone en loop)
    out = [0.0] * int(RATE * 1.4)
    for t0, f in ((0.0, 784.0), (0.18, 988.0), (0.36, 1175.0)):
        tone(out, f, t0, 0.5, 9.0, partials=((1, 1.0), (4, 0.25), (10, 0.05)))
    write("alarm_soft.wav", out)

def alarm_bells():     # ding-ding de campanitas
    out = [0.0] * int(RATE * 1.3)
    for t0 in (0.0, 0.32):
        tone(out, 1318.5, t0, 0.9, 4.0, partials=((1, 1.0), (2.76, 0.35), (5.4, 0.12)))
    write("alarm_bells.wav", out)

if __name__ == "__main__":
    chime(); alarm()
    chime_soft(); chime_bell(); chime_wood(); chime_digital(); achievement_soft(); alarm_soft(); alarm_bells()
    print("ok ->", OUT)
