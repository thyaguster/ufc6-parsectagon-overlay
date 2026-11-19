# Agent Instructions (`ufc6-parsectagon-overlay`)

## Single Source of Truth
- **Always work in this Git repository** (`thyaguster/ufc6-parsectagon-overlay`, `main` branch).
- Do **not** edit legacy copies such as `.build/new-engine-port`; this repo is self-contained (`.tools/`, `.deps/`, `dist/`, `release/` are gitignored).

## Git & Sync Workflow (Codex & Antigravity)
1. Before starting any task, pull the latest `main`:
   ```powershell
   git pull --ff-only origin main
   ```
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

## Key Invariants
- **Dual-engine installer (`installer/Setup.cs`):** Combined Install / Update / Repair / Remove checks both `bin` (`Original / Parsec`) and `novo` (`PS5 Pro / DLSS`) before modifying either.
- **Foreign-mod safety (`installer/SelfTests.cs`):** `Manager.Apply(folder, false)` without `repair: true` must continue to reject unrecognized files (`Conflicts.Count != 0`).
- **Documents folder:** Support reports, save backups, fight reports, and update cache use `Documents\UFC 6 Parsectagon Overlay` (with fallback reads for older fight reports).
