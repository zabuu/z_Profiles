param()
$ErrorActionPreference = 'Stop'
$taskBinary = Join-Path $PSScriptRoot 'native\build\zProfilesNative.dll'
if (!(Test-Path -LiteralPath $taskBinary)) { throw 'Run native\build.ps1 -Tests first.' }
$taskBin = Join-Path $PSScriptRoot 'bin'
New-Item -ItemType Directory -Path $taskBin -Force | Out-Null
Copy-Item -LiteralPath $taskBinary -Destination (Join-Path $taskBin 'zProfilesNative.dll') -Force
$taskHash = (Get-FileHash -LiteralPath (Join-Path $taskBin 'zProfilesNative.dll') -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText((Join-Path $taskBin 'SHA256SUMS'), "$taskHash  zProfilesNative.dll`n", [Text.UTF8Encoding]::new($false))
Write-Output "Packaged zProfilesNative.dll ($taskHash)"
