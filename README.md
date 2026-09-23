# 🪺 CoreNest

> **A cozy nest for your cores.**

CoreNest is a lightweight, cross-platform [libretro](https://www.libretro.com/)
frontend for **Windows** and **Android**. It loads standard libretro cores
(emulators), handles video, audio and input, and gives you a clean, fast,
no-nonsense way to play your retro games.

Written from scratch in C11 on top of SDL2. No bloat, no legacy — just the
essentials.

---

## ✨ Features

- 🎮 **Standard libretro cores** — works with `.dll` / `.so` cores
- 🪟 **Windows** — native `.exe`, no installer
- 🤖 **Android** — native `.so`, ARM64
- 🔊 **Audio** — SDL2 audio queue, low latency
- 🕹️ **Input** — keyboard + gamepad via SDL2
- 🪶 **Tiny** — easy to read and hack
- 🧩 **Portable** — same C code runs on Windows and Android

## 🚫 What CoreNest is NOT

- Not a replacement for RetroArch (yet) — no netplay, no shaders, no rewind
- Not a bundle of emulators — bring your own cores
- Not a ROM distributor — bring your own games

## 🛠️ Building

### Windows (MinGW-w64 + MSYS2)

```bash
pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake \
          mingw-w64-x86_64-ninja mingw-w64-x86_64-SDL2

cmake -B build -G Ninja
cmake --build build
