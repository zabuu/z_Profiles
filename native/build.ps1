param([switch]$Tests)
$ErrorActionPreference = 'Stop'
$taskRoot = $PSScriptRoot
$taskOutput = Join-Path $taskRoot 'build'
$taskMinhook = Join-Path $taskRoot 'vendor\minhook'
$taskVcvars = & (Join-Path $taskRoot 'Find-Vcvars.ps1')
New-Item -ItemType Directory -Path $taskOutput -Force | Out-Null
$taskSources = @((Join-Path $taskRoot 'zProfilesNative.cpp'), (Join-Path $taskMinhook 'src\buffer.c'), (Join-Path $taskMinhook 'src\hook.c'), (Join-Path $taskMinhook 'src\trampoline.c'), (Join-Path $taskMinhook 'src\hde\hde32.c'))
$taskInputs = ($taskSources | ForEach-Object { '"' + $_ + '"' }) -join ' '
$taskOptions = '/nologo /O2 /MT /EHsc /std:c++17 /W4 /DUNICODE /D_UNICODE /I"' + (Join-Path $taskMinhook 'include') + '" /I"' + (Join-Path $taskMinhook 'src') + '" '
$taskCommand = 'call "' + $taskVcvars + '" x86 >nul && cl /LD ' + $taskOptions + $taskInputs + ' /link /DEF:"' + (Join-Path $taskRoot 'zProfilesNative.def') + '" /OUT:"' + (Join-Path $taskOutput 'zProfilesNative.dll') + '" user32.lib kernel32.lib'
Push-Location -LiteralPath $taskOutput
try {
  & cmd.exe /d /s /c $taskCommand
  if ($LASTEXITCODE -ne 0) { throw "Native build failed: $LASTEXITCODE" }
  if ($Tests) {
    $taskTestSources = @((Join-Path $taskRoot '..\tests\native_tests.cpp')) + $taskSources[1..4]
    $taskTestInputs = ($taskTestSources | ForEach-Object { '"' + $_ + '"' }) -join ' '
    $taskTestCommand = 'call "' + $taskVcvars + '" x86 >nul && cl ' + $taskOptions + $taskTestInputs + ' /link /OUT:"' + (Join-Path $taskOutput 'native_tests.exe') + '" kernel32.lib user32.lib'
    & cmd.exe /d /s /c $taskTestCommand
    if ($LASTEXITCODE -ne 0) { throw "Native test build failed: $LASTEXITCODE" }
    & (Join-Path $taskOutput 'native_tests.exe')
    if ($LASTEXITCODE -ne 0) { throw "Native tests failed: $LASTEXITCODE" }
  }
} finally { Pop-Location }
Get-FileHash -LiteralPath (Join-Path $taskOutput 'zProfilesNative.dll') -Algorithm SHA256
