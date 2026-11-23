# UFC 4 fight-report investigation

Goal: capture a verified UFC 4 v28 result and statistics for a Parsectagon
match, following the UFC 6 result-then-complete-report flow. A capture is
evidence for the lobby; both fighters still report their result separately.

## Evidence found

- The local CUSA14209 v28 prepared image contains `Global_MatchState`,
  `Global_MatchResultType`, `Global_RoundNum`, `Global_RoundLength`,
  `Online_MatchTimeSecs`, `Online_MatchResultString`, and per-fighter names such
  as `Online_KnockdownsLanded`, `Online_StrikesThrown`,
  `Online_TakedownsLanded`, and `Online_Control_Duration`.
- These are UFC 4 image strings, not verified live field addresses or proof of
  their result/corner semantics. UFC 6's fixed slots must not be reused.
- The local diagnostic `ufc4/.build/probe_report_memory.py` reads a running
  SacredFire process and looks for UFC 4's named fields. It does not change
  game memory or create/upload a report.
- In a live completed fight with **Fight Statistics opened manually**, the
  SacredFire game image mapped at `0x800000000`. The game hashes field names
  using multiply-then-XOR FNV-1 (`0x811c9dc5`, `0x1000193`), and active
  records contain those keys. Several retained match-state blocks are stale:
  one held `Global_MatchState=4`, `Global_MaxRounds=3`, and
  `Global_RoundNum=2`, although the displayed fight had **one round**. A
  report exporter must bind all fields to the same current fight context.
- The screenshots of that fight show **blue Shavkat Rakhmonov defeated red Leon
  Edwards by submission**, with one Round 1 tab. The visible Overall stats
  were red/blue knockdowns `0/0`, significant strikes red `1/2` and blue
  `4/3` as displayed, total strikes red `5/8` and blue `5/5`, takedowns
  red `0/0` and blue `1/1`, and control red `0:00` and blue `1:51`.
  The live `Online_*` stat objects shared one world pointer and matched the
  displayed first-value records: `Online_StrikesLanded` had blue/red `5/5`,
  `Online_StrikesThrown` blue/red `5/8`, the two significant-strike fields
  blue/red `4/1` and `3/2`, and `Online_Control_Duration` was about 110.05
  seconds for blue. `Online_MatchResultTypeString` held the FNV-1 hash of
  `SUBMISSION`. `Online_MatchTimeSecs` held `133`, but its meaning and the
  exact finish clock still need validation. Later values in these objects
  include stale or unrelated data and must not be exported blindly.
- A bounded memory snapshot of the blue-win submission was saved locally in
  ignored `ufc4/.build/fight-report-live-20261120.json` for offline analysis.
  `Fighter_MatchResult` held the paired result codes `1,2` in this blue win.
  In a second, user-confirmed **red win**, the same live object's pair changed
  to `2,1`, supporting red-first order with `2=win` and `1=loss`. The second
  fight's bounded snapshot is in ignored
  `ufc4/.build/fight-report-red-win-20261120.json`. Its
  `Online_MatchResultTypeString` still hashed to `SUBMISSION` and
  `Online_MatchTimeSecs` changed to `106`. The user-provided screenshot confirms
  **red Leon Edwards defeated blue Leon Edwards by submission in Round 1**.
  The visible Round 1 stats are red/blue knockdowns `3/0`, significant strikes
  `17/22` and `2/9`, total strikes `17/22` and `4/12`, takedowns `0/0` for
  both, and control `0:55` and `0:00`. The first live values of the matching
  `Online_*` objects agree. The two identical fighter names make the flipped
  result codes essential for determining the corner. The exact finish clock
  is still unverified.
- The local Parsectagon lobby still includes `ufc4` in
  `RANKED_DISABLED_GAMES`. Its validator now preserves a `game` discriminator,
  defaults old reports to `ufc6`, and checks a capture's game against the
  accepted room's game. The ranked capture and multi-game room tests pass,
  including rejection of a UFC 4 report for a UFC 6 match. These are local
  development changes only; ranked UFC 4 is still disabled.
- `ufc4/tools/export_fight_report.py` is a read-only, manual v28 prototype.
  Against the red-win fight it exported the correct red winner, submission
  method, knockdowns, and strike/takedown totals. The current lobby validator
  accepted its schema and now preserves its `game: ufc4` field. The export has
  no uploader and is not ranked evidence yet. It omits finish round and clock
  because those live values are unverified. It also omits control time: the
  raw red timer rounded to 53 seconds while the screen showed `0:55`.
  When a displayed strike pair is internally inconsistent (the earlier blue
  sample showed `4/3`), the exporter emits a result-stage report without stats.
- A third live fight ended by **red Leon Edwards KO over blue Shavkat
  Rakhmonov**, confirmed by a Fight Statistics screenshot. The result type
  string held the FNV-1 hash of `KO` and the result code was `0`. The exporter
  mapped that to `Knockout` and matched the screen's red/blue knockdowns
  `1/0`, significant strikes `10/13` and `1/5`, total strikes `10/13` and
  `3/8`, and takedowns `0/0`. The local report is ignored under
  `ufc4/.build/fight-report-ko-test.json`. Round and exact finish clock remain
  unverified from live fields.
- A later one-hit test KO was captured while Fight Statistics was open. The
  manual exporter produced a complete red-corner Knockout report with both
  fighters' strike totals in ignored
  `ufc4/.build/fight-report-ko-second-test.json`. In the same finished fight,
  the active `Global_RoundNum` was `0` and `Online_MatchTimeSecs` was `38`,
  while the game's displayed finish clock showed `0` according to the user.
  The clock discrepancy is unresolved, so the exporter continues to emit
  `clockSeconds: null`; it must not present `38` as the official finish time.
- Another KO was sampled at the first post-fight menu, before Fight Statistics
  was opened. The current fight's `Global_MatchState` changed from `1` during
  the fight to `3` at that menu; the result was already red-corner Knockout and
  the strike totals were populated. The manual exporter produced a complete
  report in ignored `ufc4/.build/fight-report-first-menu-test.json`. A second
  capture after opening Fight Statistics had the same winner, method, and both
  fighters' statistics. This supports automatic capture at the first post-fight
  menu, without requiring the user to open Fight Statistics. The current
  prototype still creates a new UUID each invocation and does not watch for
  fight transitions or upload the report.
- A local read-only watcher prototype, `ufc4/tools/watch_fight_reports.py`, now
  tracks in-fight state `1` and completed state `3` and saves only once per
  fight. Its one-shot run against the completed live fight wrote an ignored
  report under `ufc4/.build/watch-test`; the result, both fighters' statistics,
  and `complete` stage matched the first-menu capture. The watcher is now
  packaged as `ufc4_fight_report_watcher.exe` and the staged UFC 4 core starts
  it once per overlay process. The packaged executable completed the same
  read-only capture test. The core/watcher pair is installed locally and the
  overlay launched the watcher in a live game. The watcher is not connected
  to the uploader; this remains local development, not ranked evidence.
- The first installed watcher captured two separate finished fights, a red
  Knockout and a red Submission. The Submission file's winner, method, and
  fighter statistics matched a fresh read of the open Fight Statistics
  screen. It initially wrote to `C:\Users\tiago\Documents`, while the configured
  Windows Documents folder and existing UFC 6 reports are under
  `D:\C\Users\tiago\Documents`. The watcher and manual exporter now read the
  configured Documents path. The corrected watcher was installed locally, and
  the two first-run reports were moved to the configured Documents folder.
- The watcher now reuses the UFC 6 overlay's existing Parsectagon upload client,
  linked-account token, server setting convention, upload markers, and error
  files. It always saves the UFC 4 JSON locally before attempting submission;
  an unlinked account or network failure cannot discard the capture.
- Fight reports now use the neutral per-game layout:
  `Documents\UFC Parsectagon Overlay\UFC4\Fight Reports`. UFC 6 uses the
  sibling `UFC6\Fight Reports` folder. Existing `ufc4-*` reports and upload
  sidecars migrate automatically from the former shared UFC 6 folder.

## Next verification

1. Compare live named-field values on a completed UFC 4 Fight Statistics
   screen against the displayed winner, red/blue corners, finish method,
   round, elapsed time, and available Overall statistics.
2. Recheck a different finish method and verify the paired winner codes after
   a game relaunch. Reject unknown or changing fields instead of guessing a
   report value.
3. Verify automatic watcher capture and upload across a new fight and rematch
   with the existing linked Parsectagon account.
4. Verify captures after a game relaunch and with a different finish method.
   Then test a complete two-player ranked match before enabling UFC 4 ranked
   rooms.

Do not upload UFC 4 development changes to GitHub yet.
