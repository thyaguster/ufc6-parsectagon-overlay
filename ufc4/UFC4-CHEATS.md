# UFC 4 cheat integration

## Verified local port interface

PORTHEON's UFC 4 port reads `%LOCALAPPDATA%\UFC4Native\cheats.json` during its game-image link step. Its bundled `ufc4-cheats.json` defines `ufc4_stamina_left` and `ufc4_stamina_right`, but those startup selections do not meet the overlay's live-cheat requirement. The UFC 4 menu no longer writes this file. The local config remains with an empty `enabled` list.

On the local v28 prepared game image, the stamina hook pattern has exactly one match. An offline call to PORTHEON's own cheat linker applied the left, right, and combined blocks with no skipped hooks. The first runtime hook caused repeatable fight-start guest faults even when its selection was Off. The installed core was rolled back to the verified pre-hook build. A revised detour avoids stack writes and skips the fighter-side read when Off, but is **disabled in default builds** until a controlled in-game test establishes that the rollback restores normal fights. Use `build.ps1 -ExperimentalLiveStamina` only for that explicit test.

The port also bundles an experimental damage pack for head, body, and legs. Its own description says the v28 adaptation has not been tested in a fight. The overlay does not expose it yet. No bundled career money or skill-point cheat was found. UFC 6's memory addresses and hooks cannot be reused in UFC 4's native port.

The local save contains `PROFILE` and `EXTRADATA0` data/index pairs. A read-only header and keyword check found career-related strings in `PROFILE`, but no identified money or skill-point field. That is insufficient to edit a save safely. Career values need a controlled before/after save comparison and a backup before a write path is considered.

## Test status

- The former startup-config helper and UI were removed after the user clarified the live-cheat requirement.
- The stamina hook/link test used the installed v28 game's prepared image without writing to the game or saves.
- The first live UFC 4 Cheats page failed its in-game fight-start test. The offline synthetic hook tests did not catch the game fault. A revised detour passes those synthetic tests but has not been installed or tested in UFC 4.
- The verified pre-hook core DLL was restored while UFC 4 was closed. The local PORTHEON startup cheat selection is empty; the pre-reset config is backed up in the ignored build folder.
