# UFC 4 startup investigation

## 2026-11-22 follow-up

- The installed UFC 4 overlay INI had `SkipStartupSplashes=1` while SacredFire had `skip_movies=0`. The launch proxy now synchronizes SacredFire's saved digit from the UFC 4 overlay preference before the game initializes, in addition to setting `BB_SKIP_MOVIES` and patching the three guarded startup UI records. This allows the UFC 4 switch to remain authoritative when the launcher restores its own movie setting.
- The existing local install was aligned to `SkipStartupSplashes=1` and `skip_movies=1`. A temporary-file bridge test confirmed Off → On → Off and preservation of unrelated config bytes. Live startup behavior still needs a launch check.
- Existing UFC 4 installs with saved settings but no `WelcomeShown` marker now migrate to `WelcomeShown=1`; the local install was marked as already configured. New installs still get the first-run page. The first-run text uses the active game's name.

Status (2026-11-19): local full-startup prototype installed; user confirmed it works in-game.

## Follow-up after movie setting was confirmed (2026-11-19)

- User confirmed the SacredFire movie switch now works. Full no-intros is still incomplete: Zuffa/legal splash, UFC 4 title keypress, and the expected offline/network acknowledgement remain.
- Read-only manifest-verified extraction located `bootflowlogo.layout` in `inxp_ui_40.big`. Its `show` animation fades in for 250 ms, waits 2500 ms, then triggers `advance`; `bootflowlogo.nav` handles that as `LegalTimerComplete` and enters `done`. The layout references `legal_ufc_logo`, so this is a legal/logo splash candidate, but its identity as the visible Zuffa screen has not been verified in-game.
- The EA SPORTS intro is a different `bootflowmovie.nav` state pointing at `samplegame/data/common/movie/bootflow/easports_ufc.mkv`. SacredFire's movie filter covers the bootflow movie path, not the legal/logo animation.
- `pressstart.dd` binds a validation action and `TXT_PRESS_START`; the navigation and Lua for the title are compressed in this archive and still need exact extraction before an input-free transition can be designed safely.
- `loginflow.nav` shows `login_failure` opening a popup and waiting for `accept` or `force_accept_login_fail_popup`, then emitting `LOGIN_TRANSITION_FAILURE` on exit. Any automatic acknowledgement must be scoped to the expected startup offline error and leave other login failures visible. The root `BootflowMain` also loads boot UI, then frontend/online modules on completion, so bypassing the whole bootflow would omit required initialization.
- No game archive bytes were changed in this investigation. Private research copies are in `.build/startup_assets` and must not be distributed.

## Local full-startup prototype installed (2026-11-19)

- The saved **Skip intros** option now controls SacredFire's movie filter and three guarded UI asset records on the next launch. The installed proxy reads `[Menu] SkipStartupSplashes` before the game starts; changing the setting in the overlay still saves immediately and does not restart the game.
- Legal/logo layout: reduces both fade durations and the 2500 ms wait to 1 ms, retaining its normal `LegalTimerComplete` event and bootflow completion.
- Title: enables the game's existing `mAutoLogin` branch using controller index 0. That branch still sets the lead profile, checks the response, and follows the normal `evt_advance_press_start` transition. Its compressed `chunzstd` frame was recompressed to the exact original record length.
- Offline login: the `login_failure` state automatically invokes its existing `force_accept_login_fail_popup` transition only for `TXT_UFC_BLAZE_CONNECTION_FAILURE`, `TXT_UNABLE_TO_LOGIN`, or `TXT_UNABLE_TO_LOGIN_EA`. It retains `LOGIN_TRANSITION_FAILURE` on exit. Other login errors keep the original popup.
- The three original record bytes are saved at `E:\Jogos\PORTHEON\games\UFC4\bin\ufc4-actions-overlay-backup\ufc4-startup-originals.zip`. The local bundle and installed proxy match the prepared files byte-for-byte. The patcher checks exact original hashes and refuses unknown archive bytes; its restore command is integrated into `install-test.ps1 -Action Remove`.
- Tests: read-only archive status, compressed title round-trip and fixed record length, JSON parsing, synthetic proxy On → Off → unknown-data refusal, real closed-archive Off → On, final `installed` status. The user subsequently launched the game and confirmed the startup skip works in-game. The automatic handling of unexpected login errors remains untested in-game.

- The installed port saves `skip_movies` in `%LOCALAPPDATA%\UFC4Native\sacredfire.ini`. Its engine identifies the intro/first-start movie skip as `BB_SKIP_MOVIES`. The prior value was 0; it is now 1, with the exact previous config backed up at `.build/startup/sacredfire.ini.before-skip-movies`.
- The UFC 4 overlay now exposes **Settings → Skip intro videos**. It changes only the existing port setting and tells the player to restart. The new core DLL is installed; the full native build passed.
- The game's UI archive contains `samplegame/data/common/flowengine/bootflow.nav`, `samplegame/ui/features/bootflow/bootflowlogo.nav`, `bootflowmovie.nav`, `pressstart.nav`, and associated VVMs. A bootflow logo state waits for `LegalTimerComplete`; title and offline failure flows have separate navigation records. The movie switch is not evidence that those transitions are skipped.
- The archive also contains `login_failure` and `roster_download_failure` popup states. Any native bypass must preserve profile selection and the offline/failure state, and use the game's normal completion transition. No UFC 4 bootflow or network archive bytes have been modified.

Next: observe one normal PORTHEON launch with the movie switch enabled, record which of Zuffa, title keypress, and network acknowledgement remain, then trace only those exact states before preparing a reversible, version-checked patch. Avoid a second launch for the same observation.

## Live result after enabling the SacredFire switch

- Three fresh SacredFire logs at 23:25–23:27 record `Runtime: intro and first-start videos skipped (BB_SKIP_MOVIES: /movie/bootflow/,/movie/ftue/)`. The running process sees `LOCALAPPDATA=C:\Users\tiago\AppData\Local`; that directory's `sacredfire.ini` still contains `skip_movies=1`. The switch is active for those movie paths, but it does not implement the requested complete no-intros flow.
- The UI archive's `bootflowlogo.nav` waits for `LegalTimerComplete`; this is separate from the movie filter. `pressstart.nav` and `pressstart.lua` are compressed records, so the title keypress path still needs precise extraction and state validation. Network/roster failure states also remain separate.
- A test DLL exercised the overlay config read/write routine against a temporary `sacredfire.ini`: Off → On → Off updated the file and read back correctly. This synthetic test does not prove the control was used successfully in the running session. No archive patch or second installation was made while the game was running.

## SacredFire switch persistence fix prepared

- The user reported that the overlay row could toggle visually without changing the SacredFire setting. A live-process test was not available, but the original C file routine wrote both values correctly against a temporary fixture and the real config from a separate test process. Runtime config writes can be overwritten when SacredFire exits, so the new design confirms the requested value in the overlay and passes `movies_on`/`movies_off` to the restart helper. The helper waits for the game to exit, writes exactly the one `skip_movies` digit, flushes, reads it back, then relaunches. A failed readback stops the restart with an error.
- New core and helper compile; temporary-file On/Off/readback checks pass; four existing tests pass; package integrity passes. Game was running, so this revised build has not been installed or live-tested.

## Setting fix installed

- After user confirmed UFC 4 was closed, installed the rebuilt core and restart helper. Both installed files match the prepared binaries byte-for-byte. The installer reported 149 prepared icon slots and completed successfully. `sacredfire.ini` remains `skip_movies=1`; the game was not relaunched. The new switch still needs one live On/Off confirmation through the overlay.

## Immediate setting revision (prepared, not installed)

- User rejected the restart confirmation for this option. Replaced it with the UFC 6 overlay pattern: one selection toggles and saves `[Menu] SkipStartupSplashes` immediately to `ufc4_actions.ini`; the row displays the saved state. The overlay also updates SacredFire’s current `skip_movies` digit immediately when possible.
- The UFC 4 `winmm.dll` bootstrap reads the saved overlay choice at the next process start and sets or clears SacredFire’s `BB_SKIP_MOVIES` environment variable before the engine initializes. This keeps the choice effective even if SacredFire rewrites its own INI on exit. No restart is triggered by selecting the setting. The game is currently running, so this revised build has not been installed.
- Tests: full build passed; temporary-file core test saved On and Off immediately; bootstrap test produced `BB_SKIP_MOVIES=1` for On and removed the variable for Off; ZIP integrity passed; four existing Python tests pass. In-game next-launch behavior remains to be verified.

## Immediate setting build installed (2026-11-19)

- User closed UFC 4 normally. Installed the revised core and startup proxy; the installer completed and reported 149 prepared icon slots. The installed core DLL, winmm proxy, and restart helper exactly match dist.
- SacredFire had `skip_movies=0` at install time. Initialized `[Menu] SkipStartupSplashes=0` in the overlay INI from that value. Future installations seed only when the overlay preference is absent. The game was not relaunched; live toggle and next-launch behavior still need confirmation.
