# Windows team entry point. Resolves this checkout and this computer's engine.
# -CheckOnly performs read-only preflight; -BuildOnly compiles without launching.
# Examples: OpenProject.cmd -CheckOnly
#           OpenProject.cmd -EngineRoot "D:\Epic Games\UE_5.9"
[CmdletBinding()]
param([string]$EngineRoot, [switch]$CheckOnly, [switch]$BuildOnly, [switch]$AllowDifferentEngineBuild)
$ErrorActionPreference = 'Stop'
try {
    $projectDir = Split-Path -Parent $PSScriptRoot
    $project = Join-Path $projectDir 'Game_Jam_Clean_Vers.uproject'
    $descriptor = Get-Content -LiteralPath $project -Raw | ConvertFrom-Json
    $candidates = [System.Collections.Generic.List[string]]::new()
    if ($EngineRoot) { $candidates.Add($EngineRoot) }
    elseif ($env:UE_ENGINE_ROOT) { $candidates.Add($env:UE_ENGINE_ROOT) }
    else {
        # Custom/source installations, including a locally registered GUID.
        $custom = Get-ItemProperty 'HKCU:\Software\Epic Games\Unreal Engine\Builds' -ErrorAction SilentlyContinue
        if ($custom) {
            $associated = $custom.PSObject.Properties[$descriptor.EngineAssociation]
            if ($associated) { $candidates.Add([string]$associated.Value) }
            foreach ($property in $custom.PSObject.Properties) {
                if ($property.Name -notlike 'PS*') { $candidates.Add([string]$property.Value) }
            }
        }
        foreach ($key in @('HKLM:\SOFTWARE\EpicGames\Unreal Engine\5.9', 'HKLM:\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\5.9')) {
            $entry = Get-ItemProperty $key -ErrorAction SilentlyContinue
            if ($entry -and $entry.InstalledDirectory) { $candidates.Add($entry.InstalledDirectory) }
        }
        $manifest = Join-Path $env:ProgramData 'Epic\UnrealEngineLauncher\LauncherInstalled.dat'
        if (Test-Path -LiteralPath $manifest) {
            foreach ($entry in (Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json).InstallationList) {
                if ($entry.AppName -eq 'UE_5.9') { $candidates.Add($entry.InstallLocation) }
            }
        }
        $candidates.Add((Join-Path $env:ProgramFiles 'Epic Games\UE_5.9'))
    }
    $resolvedEngine = $null
    foreach ($candidate in ($candidates | Select-Object -Unique)) {
        $versionFile = Join-Path $candidate 'Engine\Build\Build.version'
        if (!(Test-Path -LiteralPath $versionFile)) { continue }
        $version = Get-Content -LiteralPath $versionFile -Raw | ConvertFrom-Json
        if ($version.MajorVersion -eq 5 -and $version.MinorVersion -eq 9 -and
            (Test-Path -LiteralPath (Join-Path $candidate 'Engine\Binaries\Win64\UnrealEditor.exe')) -and
            (Test-Path -LiteralPath (Join-Path $candidate 'Engine\Build\BatchFiles\Build.bat'))) {
            $resolvedEngine = (Resolve-Path -LiteralPath $candidate).Path
            break
        }
    }
    if (!$resolvedEngine) { throw 'UE 5.9 editor/build tools not found. Install the team engine, or pass -EngineRoot "<your UE_5.9 directory>".' }
    Write-Host "Project: $project"
    Write-Host "Engine: $resolvedEngine (5.9.$($version.PatchVersion), CL $($version.Changelist))"
    if ($version.PatchVersion -ne 0 -or $version.Changelist -ne 60351404) {
        if (!$AllowDifferentEngineBuild) { throw 'Team reference is UE 5.9.0 CL 60351404. Install the matching engine. Use -AllowDifferentEngineBuild only for an intentionally validated alternative.' }
        Write-Warning 'Engine differs from the team reference; asset compatibility is not verified.'
    }
    # Check enabled mandatory plugins without editing local plugin/MCP preferences.
    $pluginNames = @{}
    foreach ($pluginDir in @((Join-Path $resolvedEngine 'Engine\Plugins'), (Join-Path $projectDir 'Plugins'))) {
        if (Test-Path -LiteralPath $pluginDir) {
            Get-ChildItem -LiteralPath $pluginDir -Recurse -Filter '*.uplugin' -File | ForEach-Object { $pluginNames[$_.BaseName] = $true }
        }
    }
    foreach ($plugin in $descriptor.Plugins) {
        if ($plugin.Enabled -and !$plugin.Optional -and !$pluginNames.ContainsKey($plugin.Name)) {
            throw "Required plugin missing: $($plugin.Name). Use the matching team engine/plugin installation. No plugins were disabled."
        }
    }
    # Scan real content, including ZIP distributions, for unexpanded LFS pointers.
    $content = Join-Path $projectDir 'Content'
    $badFiles = [System.Collections.Generic.List[string]]::new()
    $assetCount = 0
    foreach ($file in (Get-ChildItem -LiteralPath $content -Recurse -File)) {
        if ($file.Extension -in @('.uasset', '.umap')) { $assetCount++ }
        if ($file.Length -eq 0 -and $file.Extension -in @('.uasset', '.umap')) { $badFiles.Add($file.FullName); continue }
        $stream = [System.IO.File]::OpenRead($file.FullName)
        try {
            $buffer = New-Object byte[] 128
            $count = $stream.Read($buffer, 0, $buffer.Length)
            if ([System.Text.Encoding]::ASCII.GetString($buffer, 0, $count).StartsWith('version https://git-lfs.github.com/spec/v1')) { $badFiles.Add($file.FullName) }
        } finally { $stream.Dispose() }
    }
    if ($badFiles.Count) { throw "Missing asset bytes / Git LFS pointers ($($badFiles.Count)): $($badFiles[0]). In a Git checkout run git lfs pull, then retry. For ZIP delivery obtain a complete Content folder." }
    foreach ($map in @('Maps\Lvl_Downtown_Day.umap', 'Maps\Downtown_Environment.umap', 'Maps\Downtown_Daylight.umap')) {
        if (!(Test-Path -LiteralPath (Join-Path $content $map))) { throw "Required map missing: $map. Obtain a complete project copy." }
    }
    # A Git checkout also lets us detect tracked files absent from the copy.
    $repo = Split-Path -Parent $projectDir
    if ((Test-Path -LiteralPath (Join-Path $repo '.git')) -and (Get-Command git -ErrorAction SilentlyContinue)) {
        $deleted = @(git -C $repo ls-files --deleted -- Game_Jam_Clean_Vers/Content Game_Jam_Clean_Vers/Source Game_Jam_Clean_Vers/Config)
        if ($LASTEXITCODE -ne 0) { throw 'Could not inspect tracked project files.' }
        if ($deleted.Count) { throw "Tracked project files are missing ($($deleted.Count)): $($deleted[0]). Review local deletions before opening." }
    }
    Write-Host "Preflight passed: $assetCount assets; required maps and enabled plugins present. This is not a full asset dependency or gameplay test."
    if ($CheckOnly) { exit 0 }
    # Never compile over an open editor, including a different checkout of this module.
    if (Get-Process UnrealEditor, UnrealEditor-Cmd -ErrorAction SilentlyContinue) {
        throw 'Close Unreal Editor normally before building. Save your work first; this script never terminates the editor.'
    }
    $build = Join-Path $resolvedEngine 'Engine\Build\BatchFiles\Build.bat'
    & $build Game_Jam_Clean_VersEditor Win64 Development "-Project=$project" -WaitMutex -NoHotReloadFromIDE
    if ($LASTEXITCODE -ne 0) { throw "Full build failed (exit $LASTEXITCODE). Editor was not launched. Check the compiler log and install the UE-compatible Visual Studio C++ toolchain/Windows SDK." }
    if ($BuildOnly) { exit 0 }
    $editor = Join-Path $resolvedEngine 'Engine\Binaries\Win64\UnrealEditor.exe'
    # Direct invocation deliberately bypasses machine-specific EngineAssociation GUIDs.
    Start-Process -FilePath $editor -ArgumentList ('"{0}" /Game/Maps/Lvl_Downtown_Day' -f $project)
    exit 0
} catch {
    Write-Host "ERROR: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
