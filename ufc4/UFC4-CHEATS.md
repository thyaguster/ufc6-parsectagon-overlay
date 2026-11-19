# UFC 4 cheat integration

## Verified local port interface

PORTHEON's UFC 4 port reads `%LOCALAPPDATA%\UFC4Native\cheats.json` during its game-image link step. The current installed config has an empty `enabled` list. Its bundled `ufc4-cheats.json` defines `ufc4_stamina_left` and `ufc4_stamina_right`; selecting both uses a combined code block. The overlay saves these two IDs without replacing other selected IDs or extra JSON fields. A first-change backup is written as `cheats.json.ufc4-overlay.bak`.

On the local v28 prepared game image, the stamina hook pattern has exactly one match. An offline call to PORTHEON's own cheat linker applied the left, right, and combined blocks with no skipped hooks. This checks linking against the installed image; it does not establish how the cheat behaves in a fight. The image linker ran at the last observed game launch, so saved selections are presented as **Next launch: On/Off**.

The port also bundles an experimental damage pack for head, body, and legs. Its own description says the v28 adaptation has not been tested in a fight. The overlay does not expose it yet. No bundled career money or skill-point cheat was found. UFC 6's memory addresses and hooks cannot be reused in UFC 4's native port.

The local save contains `PROFILE` and `EXTRADATA0` data/index pairs. A read-only header and keyword check found career-related strings in `PROFILE`, but no identified money or skill-point field. That is insufficient to edit a save safely. Career values need a controlled before/after save comparison and a backup before a write path is considered.

## Test status

- The config helper was tested against temporary `cheats.json` files: toggles preserve unrelated IDs and JSON fields, create the one-time backup, and leave invalid or missing configs untouched.
- The stamina hook/link test used the installed v28 game's prepared image without writing to the game or saves.
- The new UFC 4 Cheats page and the cheat effect still need an in-game test. No cheat was enabled in the local PORTHEON config during development.
- The tested core DLL was installed while UFC 4 was closed, and its installed hash matches the build. The previous installed core is retained in the local ignored build folder for rollback. The game was not launched during installation.
