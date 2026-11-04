param()
$ErrorActionPreference = 'Stop'
$taskDist = Join-Path $PSScriptRoot 'dist'
New-Item -ItemType Directory -Path $taskDist -Force | Out-Null
$taskFiles = @('Core.lua','Options.lua','Setups.lua','SetupUI.lua','z_Profiles.toc','README.md','LICENSE','Install.ps1','Launch.ps1','docs') | ForEach-Object { Join-Path $PSScriptRoot $_ }
$taskStage = Join-Path $taskDist ('package-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $taskStage 'z_Profiles') -Force | Out-Null
foreach ($taskFile in $taskFiles) { Copy-Item -LiteralPath $taskFile -Destination (Join-Path $taskStage 'z_Profiles') -Recurse -Force }
Compress-Archive -LiteralPath (Join-Path $taskStage 'z_Profiles') -DestinationPath (Join-Path $taskDist 'zProfiles-0.3.1.zip') -Force
Write-Output 'Packaged zProfiles 0.3.1. Install zAPI.dll separately: https://github.com/zabuu/zAPI#installation'
