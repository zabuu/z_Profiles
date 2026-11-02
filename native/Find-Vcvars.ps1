$taskVswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (!(Test-Path -LiteralPath $taskVswhere)) { throw 'Install Visual Studio Build Tools with Desktop development with C++ (x86/x64).' }
$taskVsPath = & $taskVswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$taskVsPath) { throw 'Visual Studio C++ x86/x64 tools were not found.' }
$taskVcvars = Join-Path $taskVsPath 'VC\Auxiliary\Build\vcvarsall.bat'
if (!(Test-Path -LiteralPath $taskVcvars)) { throw 'The Visual Studio x86 environment script was not found.' }
$taskVcvars
