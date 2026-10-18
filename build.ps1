param([ValidateSet('Debug','Release')][string]$Configuration='Release')
$ErrorActionPreference='Stop'
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation=& $vswhere -latest -products '*' -version '[17.0,18.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$installation){$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath}
if(!$installation){throw 'Visual Studio C++ tools are required (v143 / Windows 10 or 11 SDK).'}
$builder=Join-Path $installation 'MSBuild\Current\Bin\MSBuild.exe'
& $builder (Join-Path $PSScriptRoot 'Chalkboard.sln') /m "/p:Configuration=$Configuration" /p:Platform=x64 /v:minimal /nologo
if($LASTEXITCODE -ne 0){throw 'Build failed.'}
Write-Output (Join-Path $PSScriptRoot "bin\x64\$Configuration\Chalkboard.exe")
