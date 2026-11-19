# UFC 4 cheat integration

## Verified local port interface

PORTHEON's UFC 4 port reads `%LOCALAPPDATA%\UFC4Native\cheats.json` during its game-image link step. Its bundled `ufc4-cheats.json` defines `ufc4_stamina_left` and `ufc4_stamina_right`, but those startup selections do not meet the overlay's live-cheat requirement. The UFC 4 menu no longer writes this file. The local config remains with an empty `enabled` list.

On the local v28 prepared game image, the stamina hook pattern has exactly one match at guest offset `0x698113`. An offline call to PORTHEON's own cheat linker applied the left, right, and combined blocks with no skipped hooks. Two runtime hook variants caused fight-start guest faults at offsets `0x69811b`–`0x69811d` even when Off. Those addresses are **8–10 bytes after the hook start**, inside the 15-byte absolute jump we had installed. The game enters the instruction at byte 8 independently. PORTHEON's own hook replaces only the first 8 bytes; the runtime absolute jump overwrote a valid entry point. Both runtime variants have been removed from the production build.

The new prototype follows PORTHEON's link-time eight-byte hook and adds a separate writable four-byte mask with marker `UFC4LIVE_STAMINA1`. The hook starts with mask `0` (normal stamina). The overlay scans for the marker in a writable page and changes only the mask during the fight. It does not patch running executable code or write PORTHEON's `cheats.json`. The wrapper in `port_bridge/cheats.py` calls the port's original cheat linker for its existing packs, then appends the dormant hook and data page. The original script is backed up as `scripts/cheats_portheon.py` in the local PORTHEON install.

The port also bundles an experimental damage pack for head, body, and legs. Its own description says the v28 adaptation has not been tested in a fight. The overlay does not expose it yet. No bundled career money or skill-point cheat was found. UFC 6's memory addresses and hooks cannot be reused in UFC 4's native port.

The local save contains `PROFILE` and `EXTRADATA0` data/index pairs. A read-only header and keyword check found career-related strings in `PROFILE`, but no identified money or skill-point field. That is insufficient to edit a save safely. Career values need a controlled before/after save comparison and a backup before a write path is considered.

## Test status

- The former startup-config helper and UI were removed after the user clarified the live-cheat requirement.
- The stamina hook/link test used the installed v28 game's prepared image without writing to the game or saves.
- Both runtime hook variants failed their in-game fight-start test with the cheat Off; the safe focus-mute build restored normal fights and passed the Windows-key audio check.
- The launch-time bridge was tested offline against a synthetic image and the actual prepared UFC 4 image. The eight-byte patch preserves the instruction at byte 8, its code reads the separate writable mask, and its extra segments have read/execute and read/write flags respectively. The overlay's marker discovery and mask changes pass a synthetic writable-page test.
- The bridge and its core passed staged in-game checks: a fight starts normally, opening Cheats with both options Off leaves the fight stable, and the left, right, and combined stamina controls work immediately during a fight without a crash. The local PORTHEON startup cheat selection remains empty; the pre-reset config and safe core are backed up in the ignored build folder.
