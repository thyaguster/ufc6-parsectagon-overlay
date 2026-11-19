# UFC 4 Actions Overlay prototype

This is the UFC 4 expansion of the [existing overlay project](../README.md), kept in its `ufc4` subfolder. It is a prototype for PORTHEON's native `sacredfire-game.exe` Vulkan renderer. It does not load UFC 6 memory hooks. Live cheats, Photo Mode, and Parsectagon are still under investigation.

Open with **F10** or **LB + RB + Menu**. Navigate with the D-pad, select with A/Cross, and go back with B/Circle. The menu contains Resume, Take screenshot, Open screenshots folder, Restart game, Settings, Cheats, and Quit game. Screenshots use Windows' Pictures/Screenshots folder and carry a `UFC4_` prefix. **F12** also captures a screenshot. Settings has separate overlay labels and PlayStation, Xbox, or keyboard and mouse game prompts, plus a mute-when-unfocused switch. Changing game artwork confirms a restart, validates the archive before patching, and applies the change after UFC 4 exits. An unsupported or modified archive is left alone.

From the shared project root, build using `./ufc4/build.ps1`. UFC 4 outputs are written to `ufc4/dist`. After UFC 4 is closed, the local `./ufc4/install-test.ps1` installs into the configured PORTHEON UFC 4 `bin` folder; `./ufc4/install-test.ps1 -Action Remove` restores prior files and PlayStation game artwork. This test installer currently uses local paths and a verified UFC 4 v28 archive. The game still launches through PORTHEON. The per-game `winmm.dll` proxy activates the Vulkan layer only inside `sacredfire-game.exe`.

The Xbox button-art patch uses a compact archive diff derived from [UFC4 Native Plus v1.2.1](https://github.com/abdullah-tech7/UFC4-Native-Plus/releases/tag/1.2.1) and is validated against the original v28 button-art SHA-256 before use. The button-art bytes remain in the user's game archive; the local installer prepares its reversible payloads from that archive. See that project's GPL-2.0 license for the source implementation.

UFC 4 already includes PlayStation and Xbox artwork; the Xbox patch changes the Flash symbol mapping. The keyboard artwork is generated from the UFC 6 keycap renderer bundled in `src/ufc4_keyboard`, with its font license included there. UFC 4's button-art file has no keyboard symbols, so this variant replaces the existing controller bitmap contents while preserving their dimensions and tag offsets. The labels currently follow the overlay's default keyboard mapping; custom UFC 4 key bindings are not yet reflected in the game prompts.

Verified in a running UFC 4 session: the overlay appears in the Vulkan frame; F10 and controller navigation work; controller input is withheld from UFC 4 while the overlay is open and returns when it closes. Screenshot capture saved a `UFC4_` PNG in the Windows Screenshots folder, and the folder and Settings pages worked in-game. Restart and quit have not been tested yet. The build also passed a raster preview, Vulkan loader check, and compilation. This is a test prototype, not a packaged UFC 4 release.

## Startup work

Settings includes **Skip intros**. Changing it saves immediately in the overlay settings and updates SacredFire’s current `skip_movies` value. On the next UFC 4 launch, the startup proxy applies the saved choice to the movie filter and a reversible local patch for the legal/logo animation, title keypress, and known offline login prompt. No forced restart is required. The user confirmed the local startup skip works in-game. See [startup investigation](UFC4-STARTUP.md).

**Cheats → Infinite stamina** uses a dormant eight-byte hook linked at launch and changes a writable flag for immediate left/right stamina control. The user verified left, right, and both controls during a fight without a crash. If the bridge is missing, the controls show **Unavailable**. The overlay does not edit PORTHEON's `cheats.json`. See [cheat investigation](UFC4-CHEATS.md).

**Settings → Mute when unfocused** saves to `ufc4_actions.ini` and applies immediately. It mutes UFC 4's own Windows audio sessions whenever its window loses focus, including when the Windows key is pressed, then restores them when UFC 4 regains focus. The previous `MuteWhenMinimized` choice is read as a migration fallback.
