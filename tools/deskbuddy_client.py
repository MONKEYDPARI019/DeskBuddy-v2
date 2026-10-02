#!/usr/bin/env python3
"""
DeskBuddy v2 test client: talks to the device over the WebSocket protocol
in docs/PROTOCOL.md. Use it from a PC to test the firmware before the
Android app is ready.

    pip install websockets
    python tools/deskbuddy_client.py monitor
    python tools/deskbuddy_client.py notify "WhatsApp" "Mom" "Dinner is ready"
    python tools/deskbuddy_client.py face happy
    python tools/deskbuddy_client.py screen weather
    python tools/deskbuddy_client.py sound notify
    python tools/deskbuddy_client.py set volume 60 --save
    python tools/deskbuddy_client.py say reply.wav        # 16-bit mono WAV, 8-48 kHz
    python tools/deskbuddy_client.py echo                 # hold BTN3, talk, Mochi repeats it back

Use --host 192.168.x.y if mDNS (deskbuddy.local) doesn't resolve on your PC.
"""
import argparse
import asyncio
import json
import sys
import time
import wave

try:
    import websockets
except ImportError:
    sys.exit("pip install websockets")

CHUNK_MS = 64  # speech is sent in 64 ms chunks, paced at real time


async def send_json(ws, obj):
    await ws.send(json.dumps(obj))


async def stream_pcm(ws, pcm: bytes, rate: int):
    """Send 16-bit mono PCM as a spoken reply, paced slightly faster than real time."""
    await send_json(ws, {"type": "say_start", "rate": rate})
    chunk = rate * 2 * CHUNK_MS // 1000
    start = time.monotonic()
    for i in range(0, len(pcm), chunk):
        await ws.send(pcm[i:i + chunk])
        # stay ~200 ms ahead of playback so the device buffer never runs dry or overflows
        ahead = (i / 2 / rate) - (time.monotonic() - start)
        if ahead > 0.2:
            await asyncio.sleep(ahead - 0.2)
    await send_json(ws, {"type": "say_end"})


def read_wav(path):
    with wave.open(path, "rb") as w:
        if w.getsampwidth() != 2:
            sys.exit("WAV must be 16-bit PCM")
        frames = w.readframes(w.getnframes())
        rate, ch = w.getframerate(), w.getnchannels()
    if ch == 2:  # downmix: keep left channel
        frames = b"".join(frames[i:i + 2] for i in range(0, len(frames), 4))
    return frames, rate


async def print_until_closed(ws, seconds=None):
    try:
        while True:
            msg = await asyncio.wait_for(ws.recv(), timeout=seconds)
            if isinstance(msg, bytes):
                continue
            print("<", msg)
    except (asyncio.TimeoutError, websockets.ConnectionClosed):
        pass


async def main():
    p = argparse.ArgumentParser(description="DeskBuddy v2 WebSocket test client")
    p.add_argument("--host", default="deskbuddy.local")
    p.add_argument("--port", type=int, default=81)
    sub = p.add_subparsers(dest="cmd", required=True)
    sub.add_parser("monitor", help="print everything the device sends")
    n = sub.add_parser("notify"); n.add_argument("app"); n.add_argument("title"); n.add_argument("body")
    f = sub.add_parser("face"); f.add_argument("name")
    s = sub.add_parser("screen"); s.add_argument("name")
    so = sub.add_parser("sound"); so.add_argument("name")
    st = sub.add_parser("set"); st.add_argument("key"); st.add_argument("value"); st.add_argument("--save", action="store_true")
    sub.add_parser("state")
    sa = sub.add_parser("say", help="play a 16-bit mono WAV through Mochi"); sa.add_argument("wav")
    e = sub.add_parser("echo", help="push-to-talk echo test (records to ptt_*.wav)")
    e.add_argument("--no-play", action="store_true", help="only record, don't play back")
    args = p.parse_args()

    uri = f"ws://{args.host}:{args.port}/"
    async with websockets.connect(uri, max_size=None, ping_interval=10) as ws:
        await send_json(ws, {"type": "hello", "name": "pc-client"})
        print("connected to", uri)

        if args.cmd == "monitor":
            await print_until_closed(ws)
        elif args.cmd == "notify":
            await send_json(ws, {"type": "notify", "app": args.app, "title": args.title, "body": args.body})
        elif args.cmd == "face":
            await send_json(ws, {"type": "face", "name": args.name})
        elif args.cmd == "screen":
            await send_json(ws, {"type": "screen", "name": args.name})
        elif args.cmd == "sound":
            await send_json(ws, {"type": "sound", "name": args.name})
        elif args.cmd == "set":
            await send_json(ws, {"type": "set", "key": args.key, "value": args.value, "save": args.save})
        elif args.cmd == "state":
            await send_json(ws, {"type": "get"})
        elif args.cmd == "say":
            pcm, rate = read_wav(args.wav)
            await stream_pcm(ws, pcm, rate)
        elif args.cmd == "echo":
            print("hold BTN3 and talk; release to hear it back. Ctrl+C to quit.")
            buf, rate, recording = bytearray(), 16000, False
            async for msg in ws:
                if isinstance(msg, bytes):
                    if recording:
                        buf.extend(msg)
                    continue
                m = json.loads(msg)
                if m.get("type") == "ptt_start":
                    buf.clear(); rate = m.get("rate", 16000); recording = True
                    print("recording...")
                elif m.get("type") == "ptt_end" and recording:
                    recording = False
                    name = time.strftime("ptt_%H%M%S.wav")
                    with wave.open(name, "wb") as w:
                        w.setnchannels(1); w.setsampwidth(2); w.setframerate(rate); w.writeframes(bytes(buf))
                    print(f"got {len(buf) / 2 / rate:.1f} s -> {name}")
                    if args.no_play or not buf:
                        await send_json(ws, {"type": "think", "on": False})
                    else:
                        await stream_pcm(ws, bytes(buf), rate)
                else:
                    print("<", msg)

        # give the device a moment to answer, then show replies
        if args.cmd not in ("monitor", "echo"):
            await print_until_closed(ws, seconds=1.0)


if __name__ == "__main__":
    try:
        asyncio.run(main())
    except KeyboardInterrupt:
        pass
