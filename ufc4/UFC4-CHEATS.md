# UFC 4 cheat integration

## Verified local port interface

PORTHEON's UFC 4 port reads `%LOCALAPPDATA%\UFC4Native\cheats.json` during its game-image link step. Its bundled `ufc4-cheats.json` defines `ufc4_stamina_left` and `ufc4_stamina_right`, but those startup selections do not meet the overlay's live-cheat requirement. The UFC 4 menu no longer writes this file. The local config remains with an empty `enabled` list.

On the local v28 prepared game image, the stamina hook pattern has exactly one match. An offline call to PORTHEON's own cheat linker applied the left, right, and combined blocks with no skipped hooks. The new overlay hook uses that exact v28 site and changes left/right stamina behavior through a session-only mask. It refuses a missing, duplicated, or already-patched site. The runtime effect still needs an in-game test; the offline linker check alone does not establish it.

The port also bundles an experimental damage pack for head, body, and legs. Its own description says the v28 adaptation has not been tested in a fight. The overlay does not expose it yet. No bundled career money or skill-point cheat was found. UFC 6's memory addresses and hooks cannot be reused in UFC 4's native port.

The local save contains `PROFILE` and `EXTRADATA0` data/index pairs. A read-only header and keyword check found career-related strings in `PROFILE`, but no identified money or skill-point field. That is insufficient to edit a save safely. Career values need a controlled before/after save comparison and a backup before a write path is considered.

## Test status

- The former startup-config helper and UI were removed after the user clarified the live-cheat requirement.
- The stamina hook/link test used the installed v28 game's prepared image without writing to the game or saves.
- The new live UFC 4 Cheats page and the cheat effect still need an in-game test. No cheat was enabled in the local PORTHEON config during development.
- The tested core DLL was installed while UFC 4 was closed, and its installed hash matches the build. The previous installed core is retained in the local ignored build folder for rollback. The game was not launched during installation.
