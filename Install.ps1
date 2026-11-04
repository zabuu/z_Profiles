param([string]$GamePath = (Join-Path $PSScriptRoot '..\..\..'), [string]$ZApiPath = (Join-Path $PSScriptRoot '..\zAPI'))
$ErrorActionPreference = 'Stop'
$taskInstaller = Join-Path $ZApiPath 'Install.ps1'
if (!(Test-Path -LiteralPath $taskInstaller)) { throw 'Install the shared zAPI dependency first: https://github.com/zabuu/zAPI#installation. The manual instructions include zAPI.dll and dlls.txt.' }
& $taskInstaller -GamePath $GamePath
