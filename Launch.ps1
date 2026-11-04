param([string]$GamePath = (Join-Path $PSScriptRoot '..\..\..'), [string]$ZApiPath = (Join-Path $PSScriptRoot '..\zAPI'))
$ErrorActionPreference = 'Stop'
$taskGame = (Resolve-Path -LiteralPath $GamePath).Path.TrimEnd('\')
$taskConfig = Join-Path $taskGame 'dlls.txt'
$taskDll = Join-Path $taskGame 'zAPI.dll'
$taskLauncher = Join-Path $taskGame 'VanillaFixes.exe'
if (!(Test-Path -LiteralPath $taskConfig) -or !(Test-Path -LiteralPath $taskDll) -or !(Test-Path -LiteralPath $taskLauncher)) { throw 'Run Install.ps1 before using this launcher.' }
if ((Get-Content -LiteralPath $taskConfig -TotalCount 1).Trim() -ine 'zAPI.dll') { throw 'zAPI.dll must be the first entry in dlls.txt. Run Install.ps1 again.' }
if (@(Get-Content -LiteralPath $taskConfig | Where-Object { $_.Trim() -ieq 'zProfilesNative.dll' }).Count) { throw 'Remove the old zProfilesNative.dll entry from dlls.txt before launching with merged zAPI.' }
$taskManifest = Join-Path $ZApiPath 'bin\SHA256SUMS'
if (!(Test-Path -LiteralPath $taskManifest)) { throw 'The validated launch helper requires the external zAPI checkout and its bin/SHA256SUMS. See https://github.com/zabuu/zAPI#installation, or launch manually through VanillaFixes after following that guide.' }
$taskExpected = ((Get-Content -LiteralPath $taskManifest -Raw).Trim() -split '\s+')[0]
if ($taskExpected -notmatch '^[a-fA-F0-9]{64}$' -or (Get-FileHash -LiteralPath $taskDll -Algorithm SHA256).Hash -ine $taskExpected) { throw 'Installed zAPI.dll differs from the external dependency manifest. Update it with WoW closed before launching.' }
$taskSupported = @('D8FC8B43006118B9E51572581DAD00DB498ACEF100B45A75D01D0E3791EB63E7', '3D019E9EE7F220B5FD90BA33E538130A19774159E4CDA0CBFD65563618E388D5')
if ($taskSupported -notcontains (Get-FileHash -LiteralPath (Join-Path $taskGame 'WoW.exe') -Algorithm SHA256).Hash) { throw 'The game executable has changed and needs a compatibility audit.' }
# This launches the interactive game the user needs to see and control.
Start-Process -FilePath $taskLauncher -WorkingDirectory $taskGame -WindowStyle Normal
