param(
    [Parameter(Mandatory=$true)][string]$BuildDirectory,
    [Parameter(Mandatory=$true)][string]$OutputDirectory
)
$ErrorActionPreference = 'Stop'
$taskBuild = (Resolve-Path -LiteralPath $BuildDirectory).Path
$taskOutput = [IO.Path]::GetFullPath($OutputDirectory)
$taskStage = Join-Path $taskOutput 'ReadyForLaunch-0.1.0-win-x64'
$taskZip = Join-Path $taskOutput 'ReadyForLaunch-0.1.0-win-x64.zip'
if (Test-Path -LiteralPath $taskStage) { throw "Destination already exists: $taskStage. Choose a new output folder." }
New-Item -ItemType Directory -Force -Path $taskOutput | Out-Null
cmake --install $taskBuild --config Release --prefix $taskStage
if ($LASTEXITCODE -ne 0) { throw 'Install staging failed' }
Compress-Archive -LiteralPath $taskStage -DestinationPath $taskZip
$taskHash = (Get-FileHash -LiteralPath $taskZip -Algorithm SHA256).Hash.ToLowerInvariant()
Set-Content -LiteralPath (Join-Path $taskOutput 'SHA256SUMS.txt') -Value "$taskHash  $([IO.Path]::GetFileName($taskZip))" -Encoding ascii
Write-Output $taskZip
