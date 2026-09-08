param(
    [Parameter(Mandatory=$true)][string]$BuildDirectory,
    [Parameter(Mandatory=$true)][string]$OutputDirectory,
    [string]$InnoCompiler = (Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Inno Setup 6/ISCC.exe'),
    [switch]$SkipInstaller
)
$ErrorActionPreference = 'Stop'
$taskBuild = (Resolve-Path -LiteralPath $BuildDirectory).Path
$taskOutput = [IO.Path]::GetFullPath($OutputDirectory)
$taskHeader = Get-Content -LiteralPath (Join-Path $taskBuild 'generated/version.hpp') -Raw
if ($taskHeader -notmatch 'appVersion\[\]\s*=\s*"([0-9]+\.[0-9]+\.[0-9]+(?:-[0-9A-Za-z.-]+)?)"') { throw 'Could not read the configured build version' }
$taskVersion = $Matches[1]
$taskNumericVersion = ($taskVersion -split '-')[0]
$taskBase = "ReadyForLaunch-$taskVersion-win-x64"
$taskStage = Join-Path $taskOutput $taskBase
$taskZip = Join-Path $taskOutput "$taskBase.zip"
if ((Test-Path -LiteralPath $taskStage) -or (Test-Path -LiteralPath $taskZip)) { throw "Package destination already exists. Choose a fresh output folder: $taskOutput" }
if (-not $SkipInstaller -and -not (Test-Path -LiteralPath $InnoCompiler)) { throw 'Inno Setup 6.3+ is required, or use -SkipInstaller for a portable ZIP only.' }
New-Item -ItemType Directory -Force -Path $taskOutput | Out-Null
cmake --install $taskBuild --config Release --prefix $taskStage
if ($LASTEXITCODE -ne 0) { throw 'Install staging failed' }
Compress-Archive -LiteralPath $taskStage -DestinationPath $taskZip
$taskArtifacts = @($taskZip)
if (-not $SkipInstaller) {
    $taskScript = Join-Path $PSScriptRoot '../installer/ReadyForLaunch.iss'
    & $InnoCompiler "/DPackageDir=$taskStage" "/DOutputDir=$taskOutput" "/DAppVersion=$taskVersion" "/DAppNumericVersion=$taskNumericVersion" $taskScript
    if ($LASTEXITCODE -ne 0) { throw 'Installer compilation failed' }
    $taskSetup = Join-Path $taskOutput "ReadyForLaunch-$taskVersion-Setup.exe"
    if (-not (Test-Path -LiteralPath $taskSetup)) { throw 'Installer output is missing' }
    $taskArtifacts += $taskSetup
}
$taskSums = foreach ($taskArtifact in $taskArtifacts) {
    $taskHash = (Get-FileHash -LiteralPath $taskArtifact -Algorithm SHA256).Hash.ToLowerInvariant()
    "$taskHash  $([IO.Path]::GetFileName($taskArtifact))"
}
Set-Content -LiteralPath (Join-Path $taskOutput 'SHA256SUMS.txt') -Value $taskSums -Encoding ascii
$taskArtifacts | Write-Output
