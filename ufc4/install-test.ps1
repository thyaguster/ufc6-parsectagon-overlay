param([ValidateSet('Install','Remove')][string]$Action='Install')
$ErrorActionPreference='Stop'
$game='E:\Jogos\PORTHEON\games\UFC4\bin'
$archive='E:\Jogos\UFC 4 v28.00\CUSA14209\databigs\inxp_ui_40.big'
$files=@('winmm.dll','ufc4_actions_core.dll','ufc4_actions_layer.dll','ufc4_actions_layer.json','ufc4_actions_restart.exe','ufc4_keycap_sync.exe','ufc4_fight_report_watcher.exe','ufc4_buttonart_ps.bin','ufc4_buttonart_xbox.bin','ufc4_buttonart_keyboard.bin','ufc4_buttonart_archive.txt','ufc4_direct_icons_map.json','ufc4_direct_icons_ps.bin','ufc4_direct_icons_xbox.bin','ufc4_direct_icons_keyboard.bin','ufc4_startup_flow.bin')
if (!(Test-Path -LiteralPath (Join-Path $game 'sacredfire-game.exe'))) { throw 'UFC 4 native executable not found' }
if (Get-Process -Name 'sacredfire-game' -ErrorAction SilentlyContinue) { throw 'Close UFC 4 before changing overlay files' }
$backup=Join-Path $game 'ufc4-overlay-backup'
$startupBackup=Join-Path $backup 'ufc4-startup-originals.zip'
$marker=Join-Path $backup 'installed.marker'
if ($Action -eq 'Install') {
    New-Item -ItemType Directory -Path $backup -Force | Out-Null
    & python (Join-Path $PSScriptRoot 'src\ufc4_startup_patch.py') prepare --archive $archive --backup $startupBackup
    if ($LASTEXITCODE) { throw 'UFC 4 startup asset preparation failed' }
    & python (Join-Path $PSScriptRoot 'src\ufc4_startup_patch.py') bundle --backup $startupBackup --output (Join-Path $PSScriptRoot 'dist\ufc4_startup_flow.bin')
    if ($LASTEXITCODE) { throw 'UFC 4 startup bundle preparation failed' }
    & python (Join-Path $PSScriptRoot 'src\ufc4_icon_art.py') prepare --archive $archive --output (Join-Path $PSScriptRoot 'dist') --bindings (Join-Path $env:LOCALAPPDATA 'UFC4Native\bbport.ini')
    if ($LASTEXITCODE) { throw 'UFC 4 button artwork preparation failed' }
    & python -c "import sys,pathlib;sys.path.insert(0,sys.argv[1]);from ufc4_direct_icons import prepare;print(prepare(*map(pathlib.Path,sys.argv[2:])))" (Join-Path $PSScriptRoot 'src') $archive (Join-Path (Split-Path $archive) '00index.csv') (Join-Path $PSScriptRoot 'dist') (Join-Path $env:LOCALAPPDATA 'UFC4Native\bbport.ini')
    if ($LASTEXITCODE) { throw 'UFC 4 direct icon preparation failed' }
    New-Item -ItemType Directory -Path $backup -Force | Out-Null
    foreach ($file in $files) {
        $target=Join-Path $game $file
        $saved=Join-Path $backup $file
        if (!(Test-Path -LiteralPath $marker) -and (Test-Path -LiteralPath $target) -and !(Test-Path -LiteralPath $saved)) {
            Copy-Item -LiteralPath $target -Destination $saved
        }
        Copy-Item -LiteralPath (Join-Path $PSScriptRoot "dist\$file") -Destination $target -Force
    }
    $overlayIni=Join-Path $game 'ufc4-quit-menu.ini'
    $sacredfireIni=Join-Path $env:LOCALAPPDATA 'UFC4Native\sacredfire.ini'
    if (!(Test-Path -LiteralPath $overlayIni) -and (Test-Path -LiteralPath (Join-Path $game 'ufc4_actions.ini'))) {
        Copy-Item -LiteralPath (Join-Path $game 'ufc4_actions.ini') -Destination $overlayIni
    }
    if (!(Test-Path -LiteralPath $overlayIni) -or !(Select-String -LiteralPath $overlayIni -Pattern '^\s*SkipStartupSplashes\s*=' -Quiet)) {
        if (Test-Path -LiteralPath $sacredfireIni) {
            $movieSetting=Select-String -LiteralPath $sacredfireIni -Pattern '^skip_movies=([01])\s*$' | Select-Object -Last 1
            if ($movieSetting) {
                $value=$movieSetting.Matches[0].Groups[1].Value
                & python -c 'import ctypes,sys; w=ctypes.windll.kernel32.WritePrivateProfileStringW; w.argtypes=[ctypes.c_wchar_p]*4; raise SystemExit(0 if w("Menu","SkipStartupSplashes",sys.argv[2],sys.argv[1]) else 1)' $overlayIni $value
                if ($LASTEXITCODE) { throw 'Could not initialize UFC 4 intro-video preference' }
            }
        }
    }
    $style='ps'
    $ini=$overlayIni
    if (Test-Path -LiteralPath $ini) {
        $setting=Select-String -LiteralPath $ini -Pattern '^\s*GameButtonStyle\s*=\s*([012])\s*$' | Select-Object -Last 1
        if ($setting) { $style=@('xbox','ps','keyboard')[[int]$setting.Matches[0].Groups[1].Value] }
    }
    & (Join-Path $game 'ufc4_keycap_sync.exe') $game $style
    if ($LASTEXITCODE) { throw "Could not apply the currently selected $style game prompts" }
    Set-Content -LiteralPath $marker -Value 'UFC 4 Actions Overlay prototype installed' -Encoding Ascii
    Write-Host 'UFC 4 overlay prototype installed.'
} else {
    if (Test-Path -LiteralPath $startupBackup) {
        & python (Join-Path $PSScriptRoot 'src\ufc4_startup_patch.py') restore --archive $archive --backup $startupBackup
        if ($LASTEXITCODE) { throw 'Could not restore UFC 4 startup assets' }
    }
    if ((Test-Path -LiteralPath (Join-Path $game 'ufc4_buttonart_ps.bin')) -and (Test-Path -LiteralPath $archive)) {
        & python (Join-Path $PSScriptRoot 'src\ufc4_icon_art.py') set --archive $archive --output $game --style ps
        if ($LASTEXITCODE) { throw 'Could not restore PlayStation game button icons' }
    }
    if ((Test-Path -LiteralPath (Join-Path $game 'ufc4_direct_icons_map.json')) -and (Test-Path -LiteralPath $archive)) {
        & python -c "import sys,pathlib;sys.path.insert(0,sys.argv[1]);from ufc4_direct_icons import set_style;set_style(pathlib.Path(sys.argv[2]),pathlib.Path(sys.argv[3]),'ps')" (Join-Path $PSScriptRoot 'src') $archive $game
        if ($LASTEXITCODE) { throw 'Could not restore PlayStation fight HUD and tutorial icons' }
    }
    foreach ($file in $files) {
        $target=Join-Path $game $file
        $saved=Join-Path $backup $file
        if (Test-Path -LiteralPath $saved) { Copy-Item -LiteralPath $saved -Destination $target -Force }
        elseif (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target -Force }
    }
    if (Test-Path -LiteralPath $marker) { Remove-Item -LiteralPath $marker -Force }
    Write-Host 'UFC 4 overlay prototype removed; prior files restored.'
}
