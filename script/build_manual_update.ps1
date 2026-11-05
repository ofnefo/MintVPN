[CmdletBinding()]
param(
    [string]$Version = "1.0.0",
    [int]$VersionMajor = 1,
    [int]$VersionMinor = 0,
    [int]$VersionPatch = 0,
    [int]$VersionBuild = 0
)

$ErrorActionPreference = "Stop"
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$deploymentRoot = Join-Path $projectRoot "deployment\manual-update"
$payloadRoot = Join-Path $deploymentRoot "Quattro"
$runtimeRoot = Join-Path $projectRoot "deployment\windows-amd64"
$archivePath = Join-Path $deploymentRoot "Quattro.zip"
$makensis = "${env:ProgramFiles(x86)}\NSIS\makensis.exe"

# A prior staging step is not evidence that the runtime DLL is still the
# intended artifact. Validate it before deleting or creating any payload.
$cronetExpected = $env:CRONET_DLL_SHA256
if ($cronetExpected -notmatch '^[0-9a-fA-F]{64}$') {
    throw 'Set CRONET_DLL_SHA256 to the independently verified runtime DLL digest.'
}
$cronetPath = Join-Path $runtimeRoot 'libcronet.dll'
if (-not (Test-Path -LiteralPath $cronetPath -PathType Leaf)) {
    throw 'Staged Cronet runtime DLL is missing.'
}
if ((Get-FileHash -LiteralPath $cronetPath -Algorithm SHA256).Hash -ne $cronetExpected) {
    throw 'Staged Cronet runtime DLL digest does not match CRONET_DLL_SHA256.'
}

if (-not $deploymentRoot.StartsWith($projectRoot, [StringComparison]::OrdinalIgnoreCase)) {
    throw "Unexpected manual-update staging path: $deploymentRoot"
}

# Reject mislabeled packages before archiving: the GUI resource and wrapper must agree.
$guiVersion = (Get-Item -LiteralPath (Join-Path $runtimeRoot "MintVPN.exe")).VersionInfo.ProductVersion
if ($guiVersion -notmatch "^$([regex]::Escape($Version))(?:\.0)?$") {
    throw "Staged MintVPN.exe version '$guiVersion' does not match requested '$Version'. Rebuild the GUI first."
}

if (Test-Path -LiteralPath $deploymentRoot) {
    Remove-Item -LiteralPath $deploymentRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $payloadRoot -Force | Out-Null

$runtimeFiles = @("MintVPN.exe", "MintVPNCore.exe", "MintVPNUpdater.exe", "libcronet.dll")
foreach ($name in $runtimeFiles) {
    $source = Join-Path $runtimeRoot $name
    if (-not (Test-Path -LiteralPath $source)) {
        throw "Missing staged runtime file: $source"
    }
    Copy-Item -LiteralPath $source -Destination (Join-Path $payloadRoot $name)
}

# Preserve available licences in the update archive too. Missing notices fail the build.
$licenseFiles = @{
    "MintVPN-GPL-3.0.txt" = "LICENSE"
    "Upstream-Notice.md" = "THRONE_NOTICE.md"
    "QHotkey-BSD-3-Clause.txt" = "3rdparty/QHotkey/LICENSE"
    "SQLiteCpp-LICENSE.txt" = "3rdparty/SQLiteCpp/LICENSE.txt"
    "simple-protobuf-LICENSE.txt" = "3rdparty/simple-protobuf/LICENSE"
}
$licenseRoot = Join-Path $payloadRoot "licenses"
New-Item -ItemType Directory -Path $licenseRoot -Force | Out-Null
foreach ($item in $licenseFiles.GetEnumerator()) {
    Copy-Item -LiteralPath (Join-Path $projectRoot $item.Value) -Destination (Join-Path $licenseRoot $item.Key) -ErrorAction Stop
}
Copy-Item -LiteralPath (Join-Path $projectRoot '3rdparty/go-core-notices') -Destination (Join-Path $licenseRoot 'go-core') -Recurse -ErrorAction Stop
Copy-Item -LiteralPath (Join-Path $projectRoot '3rdparty/qt-6.11.1-sbom') -Destination (Join-Path $licenseRoot 'qt-6.11.1-sbom') -Recurse -ErrorAction Stop

Compress-Archive -LiteralPath $payloadRoot -DestinationPath $archivePath -CompressionLevel Optimal

if (-not (Test-Path -LiteralPath $makensis)) {
    throw "NSIS was not found: $makensis"
}

Push-Location $projectRoot
try {
    & $makensis /NOCD /V3 /INPUTCHARSET UTF8 `
        "/DAPP_VERSION=$Version" `
        "/DAPP_VERSION_MAJOR=$VersionMajor" `
        "/DAPP_VERSION_MINOR=$VersionMinor" `
        "/DAPP_VERSION_PATCH=$VersionPatch" `
        "/DAPP_VERSION_BUILD=$VersionBuild" `
        script\windows_manual_update.nsi
    if ($LASTEXITCODE -ne 0) {
        throw "NSIS failed with exit code $LASTEXITCODE"
    }
}
finally {
    Pop-Location
}

Get-Item -LiteralPath (Join-Path $projectRoot "MintVPNUpdate-$Version.exe")
