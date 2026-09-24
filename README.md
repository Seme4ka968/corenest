# 🪺 CoreNest

> **A cozy nest for your cores.**

CoreNest is a lightweight, cross-platform [libretro](https://www.libretro.com/)
frontend for **Windows** and **Android**. It loads standard libretro cores
(emulators), handles video, audio and input, and gives you a clean, fast,
no-nonsense way to play your retro games.

Written from scratch in C11 on top of SDL2. No bloat, no legacy — just the
essentials.

**Version:** `0.2.0`

---

## ✨ Features

- 🎮 **Standard libretro cores** — works with `.dll` / `.so` cores
- 🪟 **Windows** — native `.exe`, no installer
- 🕹️ **Gamepad** — SDL_GameController (8BitDo, Xbox, DualShock, ...)
- 🔊 **Audio** — SDL2 audio queue, low latency
- 💾 **Save RAM** — auto `.srm` on exit / on start (battery saves)
- 📸 **Save State** — F5 / F8 (freeze / restore)
- 🎛️ **Hotkeys** — Esc, F11, F1, F2, F3, F5, F8, P, F, 1-4
- 🖼️ **Video controls** — integer scale (1x–4x), fullscreen, filter
- 📊 **FPS counter** in window title
- 🪶 **Tiny** — ~2000 lines of C, easy to read and hack
- 🧩 **Portable** — same C code runs on Windows and Android
- 🪟 **Multi-window** — run two games side by side with `--pos` and `--scale`

## 🚫 What CoreNest is NOT

- Not a replacement for RetroArch (yet) — no netplay, no shaders, no rewind
- Not a bundle of emulators — bring your own cores
- Not a ROM distributor — bring your own games

## 🛠️ Building

### Windows (MinGW-w64 + MSYS2)

```bash
pacman -S mingw-w64-ucrt-x86_64-toolchain \
          mingw-w64-ucrt-x86_64-cmake \
          mingw-w64-ucrt-x86_64-ninja \
          mingw-w64-ucrt-x86_64-SDL2 \
          mingw-w64-ucrt-x86_64-pkgconf

cmake -B build -G Ninja
cmake --build build
