# CoreNest architecture

The refactor branch separates the frontend into explicit runtime layers.

## Layout

- `src/main.c` — process entry point.
- `src/launcher.c` — temporary application bootstrap and orchestration layer.
- `src/core/` — libretro dynamic loading, core lifecycle and environment handling.
- `src/drivers/video/` — video backend.
- `src/drivers/audio/` — audio backend.
- `src/drivers/input/` — input backend.
- `src/save/` — persistent RAM and save-state handling.

## Dependency direction

```
application
    |
    +--> core runtime
    +--> input driver
    +--> video driver
    +--> audio driver
    +--> save manager
```

CoreNest-specific code owns the frontend lifecycle. A libretro core is treated as a runtime component and is not allowed to know about SDL implementation details.

## Refactoring rules

1. Keep the libretro ABI at the core boundary.
2. Keep SDL/platform details inside drivers or platform code.
3. Keep file-system/content discovery outside the core runtime.
4. Avoid global state in new modules.
5. Keep the frame hot path free of allocation and unnecessary file I/O.
6. Every subsystem gets an explicit init/deinit lifecycle.
7. New features should be added to the appropriate subsystem instead of growing `launcher.c`.

The long-term goal is to make `launcher.c` a thin application coordinator rather than the place where every subsystem is implemented.
