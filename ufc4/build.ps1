$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$zig = Join-Path $projectRoot '.tools/ziglang/zig.exe'
$vulkan = Join-Path $projectRoot 'vendor/vulkan-headers/include'
if (!(Test-Path -LiteralPath $zig)) { throw 'Zig compiler missing' }
Push-Location -LiteralPath $PSScriptRoot
try {
    New-Item -ItemType Directory -Path dist -Force | Out-Null
    & $zig cc -target x86_64-windows-gnu -O2 -Wall -Wextra -shared -Isrc src/ufc4_core.c src/ufc4_cheats_config.c src/screenshot_folder.c -lgdi32 -luser32 -lkernel32 -lshell32 -lole32 -o dist/ufc4_actions_core.dll
    if ($LASTEXITCODE) { throw 'Core build failed' }
    & $zig cc -target x86_64-windows-gnu -O2 -Wall -Wextra -shared -DUFC4_TEST -Isrc src/ufc4_core.c src/ufc4_cheats_config.c src/screenshot_folder.c -lgdi32 -luser32 -lkernel32 -lshell32 -lole32 -o dist/ufc4_actions_core_test.dll
    if ($LASTEXITCODE) { throw 'Core test build failed' }
    & $zig cc -target x86_64-windows-gnu -O2 -Wall -Wextra -shared "-I$vulkan" -Isrc src/vulkan_menu_layer.c src/screenshot_png.c src/screenshot_folder.c src/vulkan_layer.def -lkernel32 -lole32 -lwindowscodecs -luuid -lshell32 -o dist/ufc4_actions_layer.dll
    if ($LASTEXITCODE) { throw 'Layer build failed' }
    & $zig cc -target x86_64-windows-gnu -O2 -Wall -Wextra -shared src/ufc4_winmm.c src/ufc4_winmm.def -lkernel32 -o dist/winmm.dll
    if ($LASTEXITCODE) { throw 'Bridge build failed' }
    & $zig cc -target x86_64-windows-gnu -O2 -Wall -Wextra -municode src/ufc4_restart.c -lkernel32 -lshell32 '-Wl,--subsystem,windows' -o dist/ufc4_actions_restart.exe
    if ($LASTEXITCODE) { throw 'Restart helper build failed' }
    Copy-Item -LiteralPath src/ufc4_actions_layer.json -Destination dist/ufc4_actions_layer.json -Force
    $font=Join-Path $PSScriptRoot 'src/ufc4_keyboard/keyboard-art/font/BarlowCondensed-Bold.ttf'
    $tail=Join-Path $PSScriptRoot 'src/ufc4_buttonart_xbox_tail.bin'
    & python -m PyInstaller --onefile --noconfirm --clean --exclude-module numpy --name ufc4_keycap_sync --paths src --paths src/ufc4_keyboard "--add-data=$font;ufc4_keyboard/keyboard-art/font" "--add-data=$tail;." --distpath dist --workpath (Join-Path $PSScriptRoot '.build/ufc4-keycap') --specpath (Join-Path $PSScriptRoot '.build') src/ufc4_keycap_sync.py
    if ($LASTEXITCODE) { throw 'UFC 4 keyboard sync helper build failed' }
    Write-Host 'Built UFC 4 overlay prototype in dist.'
} finally {
    Pop-Location
}
