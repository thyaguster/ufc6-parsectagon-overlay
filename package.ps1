param([switch]$NoLobby)
$ErrorActionPreference = 'Stop'
Set-Location -LiteralPath $PSScriptRoot
& (Join-Path $PSScriptRoot 'build.ps1') -NoLobby:$NoLobby
$version = (Get-Content -LiteralPath 'dist\release.json' -Raw | ConvertFrom-Json).version
& (Join-Path $PSScriptRoot 'build-setup.ps1')
$package = Join-Path $PSScriptRoot "release\UFC6-Parsectagon-Overlay-$version"
$releaseRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot 'release')).TrimEnd('\') + '\'
$packagePath = [IO.Path]::GetFullPath($package)
if (-not $packagePath.StartsWith($releaseRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Package path is outside release directory.' }
if (Test-Path -LiteralPath $packagePath) { Remove-Item -LiteralPath $packagePath -Recurse -Force }
New-Item -ItemType Directory -Force -Path $package, "$package\dist" | Out-Null
foreach ($file in @('README.md','LICENSE','THIRD_PARTY_NOTICES.md','RELEASE-NOTES.md','build.ps1','build-setup.ps1','build-icons.ps1','build-updater.ps1','package.ps1','manage_menu.py','attach_menu.py')) {
    Copy-Item -LiteralPath $file -Destination $package
}
Copy-Item -LiteralPath 'src' -Destination $package -Recurse -Force
Copy-Item -LiteralPath 'installer' -Destination $package -Recurse -Force
if (Test-Path -LiteralPath 'ufc4') {
    New-Item -ItemType Directory -Force -Path "$package\ufc4" | Out-Null
    foreach ($u4Dir in @('src','tools','tests','docs','port_bridge')) {
        if (Test-Path -LiteralPath (Join-Path 'ufc4' $u4Dir)) {
            Copy-Item -LiteralPath (Join-Path 'ufc4' $u4Dir) -Destination "$package\ufc4" -Recurse -Force
        }
    }
    foreach ($u4File in @('README.md','UFC4-CHEATS.md','UFC4-FIGHT-REPORT.md','UFC4-NEXT-CHAT.md','UFC4-STARTUP.md','UFC4-XBOX-ICONS.md','build.ps1','install-test.ps1')) {
        if (Test-Path -LiteralPath (Join-Path 'ufc4' $u4File)) {
            Copy-Item -LiteralPath (Join-Path 'ufc4' $u4File) -Destination "$package\ufc4" -Force
        }
    }
    if (Test-Path -LiteralPath 'ufc4\dist') {
        New-Item -ItemType Directory -Force -Path "$package\ufc4\dist" | Out-Null
        foreach ($u4Dist in @('winmm.dll','ufc4_actions_core.dll','ufc4_actions_layer.dll','ufc4_actions_layer.json','ufc4_actions_restart.exe','ufc4_fight_report_watcher.exe','ufc4_keycap_sync.exe')) {
            if (Test-Path -LiteralPath (Join-Path 'ufc4\dist' $u4Dist)) {
                Copy-Item -LiteralPath (Join-Path 'ufc4\dist' $u4Dist) -Destination "$package\ufc4\dist" -Force
            }
        }
    }
}
New-Item -ItemType Directory -Force -Path "$package\docs", "$package\docs\screenshots" | Out-Null
foreach ($name in @('PHOTO-MODE.md','SCREENSHOTS.md','PERFORMANCE.md','STARTUP.md','FIGHT-REPORT-EXPORT.md','LIVE-FIGHT-UPLOAD.md','ROADMAP.md','theme-config.example.cfg')) { Copy-Item -LiteralPath (Join-Path 'docs' $name) -Destination "$package\docs" }
Get-ChildItem -LiteralPath 'docs/screenshots' -File | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination "$package\docs\screenshots" }
if (Test-Path -LiteralPath 'scripts') {
    $scriptPackage = Join-Path $package 'scripts'
    New-Item -ItemType Directory -Force -Path $scriptPackage | Out-Null
    foreach ($name in @('fight_report_export.py','decode_fight_stats_blob.py','export_complete_fight_report.py','read_postfight_capture.py','lobby_account.py','upload_fight_report.py','generate_photo_blend_shaders.py','generate_photo_effects_shaders.py','new_engine_probe.ps1')) {
        Copy-Item -LiteralPath (Join-Path 'scripts' $name) -Destination $scriptPackage -Force
    }
}
if (Test-Path -LiteralPath '.github') { Copy-Item -LiteralPath '.github' -Destination $package -Recurse -Force }
Copy-Item -LiteralPath 'dist\UFC6ParsectagonOverlaySetup.exe' -Destination (Join-Path $package 'UFC6ParsectagonOverlaySetup.exe')
New-Item -ItemType Directory -Force -Path "$package\tests" | Out-Null
Get-ChildItem -LiteralPath 'tests' -File | ForEach-Object {
    if ($_.Name -ne 'fight_stats_probe_test.py') { Copy-Item -LiteralPath $_.FullName -Destination "$package\tests" }
}
Copy-Item -LiteralPath 'dist\libwinpthread-1.dll','dist\winmm.dll','dist\version.dll','dist\release.json','dist\diagnostics-components.json','dist\ufc6_menu_layer.dll','dist\ufc6_menu_layer.json','dist\ufc6_menu_restart.exe','dist\ufc6_menu_icons.exe','dist\ufc6_menu_config.exe' -Destination "$package\dist"
New-Item -ItemType Directory -Force -Path "$package\vendor\vulkan-headers" | Out-Null
Copy-Item -LiteralPath 'vendor\vulkan-headers\include','vendor\vulkan-headers\LICENSES' -Destination "$package\vendor\vulkan-headers" -Recurse -Force
Copy-Item -LiteralPath 'vendor\vulkan-headers\LICENSE.md' -Destination "$package\vendor\vulkan-headers"
New-Item -ItemType Directory -Force -Path "$package\vendor\xbox-icons\tests" | Out-Null
Get-ChildItem -LiteralPath 'vendor/xbox-icons' -File | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination "$package\vendor\xbox-icons" }
Copy-Item -LiteralPath 'vendor/xbox-icons/profiles','vendor/xbox-icons/runtime-licenses','vendor/xbox-icons/keyboard-art' -Destination "$package\vendor\xbox-icons" -Recurse -Force
Get-ChildItem -LiteralPath 'vendor/xbox-icons/tests' -File | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination "$package\vendor\xbox-icons\tests" }
# Exclude Python bytecode produced by the helper tests or a prior local build.
$packageRoot = [IO.Path]::GetFullPath($package).TrimEnd('\') + '\'
Get-ChildItem -LiteralPath $package -Directory -Recurse -Filter '__pycache__' | ForEach-Object {
    $cachePath = [IO.Path]::GetFullPath($_.FullName)
    if (-not $cachePath.StartsWith($packageRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Cache path is outside the release package.' }
    Remove-Item -LiteralPath $cachePath -Recurse -Force
}
$archive = Join-Path $PSScriptRoot "release\UFC6-Parsectagon-Overlay-$version.zip"
Compress-Archive -LiteralPath $package -DestinationPath $archive -Force
Get-Item -LiteralPath $archive | Select-Object FullName,Length | Format-List
