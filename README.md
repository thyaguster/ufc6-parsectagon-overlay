# UFC 6 Parsectagon Overlay

An all-in-one modern Vulkan in-frame overlay for **EA SPORTS UFC 6** (Kyty / PORTHEON), integrating **Parsectagon** online matchmaking, multiple customizable UI layouts, live fight/career utilities, Photo Mode + effects, and native Xbox / PlayStation controller prompts.

## Features

### Multiple Modern UI Types & 1-Click Presets
Switch layouts or presets live from **Settings → Master preset** or **Appearance & theme → Layout style** without editing config files:
- **Top Bar (`Parsectagon Apex` / `Championship Solid`)** — Floating top navigation bar with flyout submenus.
- **Floating Island (`Octagon Pill` / `Stealth Island`)** — Compact centered floating island bar.
- **Side Blade** — Left-docked console-style vertical category rail leaving most of the Octagon visible.
- **Bottom Command Dock** — Bottom-centered dock with an upward drawer and controller hints bar, keeping the top fight clock clear.
- **Bento Split Deck** — 2-column dashboard with vertical tabs, live pad/renderer tiles, and quick actions.
- **Broadcast Lower-Third** — Compact UFC PPV-style corner card pinned to the bottom-left.
- **Octagon Split Hub** — Octagonal category hub paired with the active panel.
- **Classic Box** — Original centered layout (available via config).

Each layout supports **Glass** or **Opaque** backdrops, **Rounded / Square / Outline / Pill / Octagon** corner styles, and **7 color themes** including the signature **Parsectagon** theme.

### Parsectagon Online Matchmaking
- **Auto-Detection**: Switching to the in-game **Online** tab automatically detects the highlight via the Vulkan frame probe and opens the **Parsectagon Lobby**.
- **Room Browser & Hosting**: Browse live fight rooms, configure Parsec stream quality presets, or host your own Octagon room directly inside the overlay.

### Controller-First Navigation
- Authored vector controller glyphs (<kbd>LB</kbd>/<kbd>RB</kbd> on Xbox, <kbd>L1</kbd>/<kbd>R1</kbd> on PlayStation, D-pad, face buttons).
- <kbd>LB</kbd>/<kbd>RB</kbd> switches top-level tabs; D-pad / Left Stick navigates items; <kbd>A</kbd> confirms; <kbd>B</kbd> goes back or closes.

### In-Fight, Career & Photo Mode Tools
- **Live Cheats**: Player 1 Infinite Stamina, No Damage, Next Hit KO (25s), plus Career Grade A / Economy / Savegame backups.
- **Photo Mode & Effects**: Freecam, Freeze Fight Action, 10 visual FX sliders, and custom PORTHEON watermark.
- **Performance & Diagnostics**: Instant Replay controls, screenshots, startup options, and diagnostic report export.

## Building from Source

Run from the repository root in PowerShell (requires `zig` / Windows SDK build tooling configured via `build.ps1`):

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\build.ps1
```

Compiled binaries (`version.dll`, `ufc6_menu_layer.dll`, `winmm.dll`, `libwinpthread-1.dll`, `ufc6_menu_restart.exe`) are output to `dist\`.

## License & Notices

See [LICENSE](LICENSE) and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
