# Mochi face simulator

Renders `src/face.cpp` on your PC, without the ESP32, so you can check every mood and its animation frames.
A mock `U8g2` class draws into a 128×64 bitmap.

```bash
cd tools/face-sim
g++ -std=c++17 -Imock -I../../src -o facesim harness.cpp ../../src/face.cpp
./facesim faces.bin          # one frame per mood/state
python3 sheet.py faces.bin faces.png
./facesim anim.bin 4         # 4 frames per mood, 160 ms apart
python3 sheet.py anim.bin anim.png 4
```

`sheet.py` needs Pillow (`pip install pillow`). Text is drawn as placeholder blocks, since the real U8g2 fonts aren't included.
