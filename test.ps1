param([ValidateSet('Debug','Release')][string]$Configuration='Debug')
$ErrorActionPreference='Stop'
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$installation=& $vswhere -latest -products '*' -version '[17.0,18.0)' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(!$installation){$installation=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath}
if(!$installation){throw 'Visual Studio C++ tools are required.'}
$builder=Join-Path $installation 'MSBuild\Current\Bin\MSBuild.exe'
& $builder (Join-Path $PSScriptRoot 'ChalkboardTests.vcxproj') /m "/p:Configuration=$Configuration" /p:Platform=x64 /v:minimal /nologo
if($LASTEXITCODE -ne 0){throw 'Test build failed.'}
& (Join-Path $PSScriptRoot "Tests\bin\$Configuration\ChalkboardTests.exe") $PSScriptRoot
if($LASTEXITCODE -ne 0){throw 'Tests failed.'}
