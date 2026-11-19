# Agent Instructions (`ufc6-parsectagon-overlay`)

## Single Source of Truth
- **Always work in this Git repository** (`thyaguster/ufc6-parsectagon-overlay`, `main` branch).
- Do **not** edit legacy copies such as `.build/new-engine-port`; this repo is self-contained (`.tools/`, `.deps/`, `dist/`, `release/` are gitignored).
- The UFC 4 expansion lives in this repository's `ufc4/` directory. The old project-root `ufc4/` copy is read-only history.

## Git & Sync Workflow (Codex & Antigravity)
1. Before starting any task, pull the latest `main`:
   ```powershell
   git pull --ff-only origin main
   ```
   If Windows Schannel returns `SEC_E_NO_CREDENTIALS`, retry with `git -c http.sslBackend=openssl pull --ff-only origin main`.
2. Make edits, build, and run tests here.
3. Commit and push to `origin main` when finished so both agents stay in sync:
   ```powershell
   git status
   git add <files>
   git commit -m "<message>"
   git push origin main
   ```
   *(Never force-push or commit `.tools/`, `.deps/`, `.build/`, `release/`, or compiled `dist/*.dll` / `dist/*.exe` binaries.)*

## Build & Verification
- **Overlay + manifests (public release mode disables Lobby):**
  ```powershell
  pwsh -NoProfile -ExecutionPolicy Bypass -File .\build.ps1 -NoLobby
  ```
- **Installer + unit/integration self-tests:**
  ```powershell
  pwsh -NoProfile -ExecutionPolicy Bypass -File .\build-setup.ps1 -Test
  ```
- **Full release package (`.zip` + setup `.exe`):**
  ```powershell
  pwsh -NoProfile -ExecutionPolicy Bypass -File .\package.ps1 -NoLobby
  ```
- **UFC 4 local prototype:** run `pwsh -NoProfile -ExecutionPolicy Bypass -File .\ufc4\build.ps1`, then `python -m unittest discover -s ufc4\tests -p 'test_*.py'`. Its build outputs and game-derived research files remain ignored under `ufc4/dist/` and `ufc4/.build/`.

## Key Invariants
- **Dual-engine installer (`installer/Setup.cs`):** Combined Install / Update / Repair / Remove checks both `bin` (`Original / Parsec`) and `novo` (`PS5 Pro / DLSS`) before modifying either.
- **Foreign-mod safety (`installer/SelfTests.cs`):** `Manager.Apply(folder, false)` without `repair: true` must continue to reject unrecognized files (`Conflicts.Count != 0`).
- **Output folders:** Support reports, save backups, and fight reports use `Documents\UFC 6 Parsectagon Overlay` (with fallback reads for older fight reports). The update cache uses `%LOCALAPPDATA%\UFC 6 Parsectagon Overlay\Updates`.
- **UFC 4 cheats:** the first session-only stamina hook crashed at fight start; the installed core was rolled back. The revised hook is disabled in default builds and must not be installed until the rollback test is confirmed. Do not write PORTHEON's startup cheat config from this menu. The experimental damage pack also needs in-game validation.
