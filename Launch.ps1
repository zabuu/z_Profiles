param([string]$GamePath = (Join-Path $PSScriptRoot '..\..\..'))
$ErrorActionPreference = 'Stop'
$taskGame = (Resolve-Path -LiteralPath $GamePath).Path.TrimEnd('\')
$taskConfig = Join-Path $taskGame 'dlls.txt'
$taskDll = Join-Path $taskGame 'zProfilesNative.dll'
$taskLauncher = Join-Path $taskGame 'VanillaFixes.exe'
if (!(Test-Path -LiteralPath $taskConfig) -or !(Test-Path -LiteralPath $taskDll) -or !(Test-Path -LiteralPath $taskLauncher)) { throw 'Run Install.ps1 before using this launcher.' }
if ((Get-Content -LiteralPath $taskConfig -TotalCount 1).Trim() -ine 'zProfilesNative.dll') { throw 'zProfilesNative.dll must be the first entry in dlls.txt. Run Install.ps1 again.' }
$taskExpected = ((Get-Content -LiteralPath (Join-Path $PSScriptRoot 'bin\SHA256SUMS') -Raw).Trim() -split '\s+')[0]
if ((Get-FileHash -LiteralPath $taskDll -Algorithm SHA256).Hash -ine $taskExpected) { throw 'Installed native binary differs from this release. Run Install.ps1 again.' }
if ((Get-FileHash -LiteralPath (Join-Path $taskGame 'WoW.exe') -Algorithm SHA256).Hash -ne 'D8FC8B43006118B9E51572581DAD00DB498ACEF100B45A75D01D0E3791EB63E7') { throw 'The game executable has changed and needs a compatibility audit.' }
# This launches the interactive game the user needs to see and control.
Start-Process -FilePath $taskLauncher -WorkingDirectory $taskGame -WindowStyle Normal
