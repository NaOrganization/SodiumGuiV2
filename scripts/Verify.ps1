param(
	[string]$Configuration = "Debug",
	[string]$Platform = "x64",
	[switch]$SkipExamples
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
	throw "vswhere.exe was not found. Install Visual Studio Build Tools with MSBuild and MSVC."
}

$vsInstall = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild -property installationPath
if (-not $vsInstall) {
	throw "No Visual Studio installation with MSBuild was found."
}

$msbuild = Join-Path $vsInstall "MSBuild\Current\Bin\MSBuild.exe"
if (-not (Test-Path $msbuild)) {
	$msbuild = (& $vswhere -latest -products * -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1)
}
if (-not (Test-Path $msbuild)) {
	throw "MSBuild.exe was not found under the selected Visual Studio installation."
}

$vcvars = Join-Path $vsInstall "VC\Auxiliary\Build\vcvars64.bat"
if (-not (Test-Path $vcvars)) {
	throw "vcvars64.bat was not found under the selected Visual Studio installation."
}

$testOutDir = Join-Path $root "tests\bin\$Platform\$Configuration"
$testObjDir = Join-Path $root "tests\obj\$Platform\$Configuration"
New-Item -ItemType Directory -Force -Path $testOutDir, $testObjDir | Out-Null

$testExe = Join-Path $testOutDir "SdCoreTests.exe"
$sources = @(
	"tests\CoreTests.cpp",
	"Input\SdInput.cpp",
	"Render\SdRenderList.cpp",
	"Render\SdRenderListPath.cpp",
	"Render\SdRenderListPrimitives.cpp",
	"Render\SdRenderListText.cpp",
	"Render\SdRenderStats.cpp"
) | ForEach-Object { Join-Path $root $_ }

$quotedSources = ($sources | ForEach-Object { "`"$_`"" }) -join " "
$compileCommand = "call `"$vcvars`" >nul && pushd `"$testObjDir`" && cl /nologo /std:c++20 /EHsc /W3 /utf-8 /I`"$root`" /Fe:`"$testExe`" $quotedSources"

Write-Host "Building core tests..."
cmd /c $compileCommand
if ($LASTEXITCODE -ne 0) {
	exit $LASTEXITCODE
}

Write-Host "Running core tests..."
& $testExe
if ($LASTEXITCODE -ne 0) {
	exit $LASTEXITCODE
}

if (-not $SkipExamples) {
	$solution = Join-Path $root "examples\examples.sln"
	Write-Host "Building examples solution ($Configuration|$Platform)..."
	& $msbuild $solution /m /p:Configuration=$Configuration /p:Platform=$Platform /v:minimal
	if ($LASTEXITCODE -ne 0) {
		exit $LASTEXITCODE
	}
}

Write-Host "Verification completed."
