param(
    [string]$BuildDir = ".\\build-nonhalf\\Release",
    [string]$OutputRoot = ".\\dist",
    [string]$PackageName = "haorender-windows-portable",
    [switch]$Zip
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$resolvedBuildDir = (Resolve-Path (Join-Path $repoRoot $BuildDir)).Path
$resolvedOutputRoot = Join-Path $repoRoot $OutputRoot
$packageDir = Join-Path $resolvedOutputRoot $PackageName
$resourcesDir = Join-Path $repoRoot "Resources"

if (-not (Test-Path (Join-Path $resolvedBuildDir "myrender.exe"))) {
    throw "Cannot find myrender.exe in build directory: $resolvedBuildDir"
}

if (-not (Test-Path $resourcesDir)) {
    throw "Cannot find Resources directory: $resourcesDir"
}

New-Item -ItemType Directory -Path $resolvedOutputRoot -Force | Out-Null
if (Test-Path $packageDir) {
    Remove-Item -LiteralPath $packageDir -Recurse -Force
}
New-Item -ItemType Directory -Path $packageDir -Force | Out-Null

$excludeNames = @(
    "*.pdb",
    "*.ilk",
    "*.lib",
    "*.exp",
    "opencv_world*d.dll"
)

Get-ChildItem -LiteralPath $resolvedBuildDir -Force | ForEach-Object {
    $skip = $false
    foreach ($pattern in $excludeNames) {
        if ($_.Name -like $pattern) {
            $skip = $true
            break
        }
    }
    if (-not $skip) {
        Copy-Item -LiteralPath $_.FullName -Destination $packageDir -Recurse -Force
    }
}

Copy-Item -LiteralPath $resourcesDir -Destination $packageDir -Recurse -Force

foreach ($docName in @("README.md", "README.zh-CN.md", "README.ja.md", "LICENSE", "NOTICE")) {
    $docPath = Join-Path $repoRoot $docName
    if (Test-Path $docPath) {
        Copy-Item -LiteralPath $docPath -Destination $packageDir -Force
    }
}

$zipPath = Join-Path $resolvedOutputRoot ($PackageName + ".zip")
if ($Zip) {
    if (Test-Path $zipPath) {
        Remove-Item -LiteralPath $zipPath -Force
    }
    Compress-Archive -Path (Join-Path $packageDir "*") -DestinationPath $zipPath
    Write-Output "Portable package created: $zipPath"
}
else {
    Write-Output "Portable package directory created: $packageDir"
}
