[CmdletBinding()]
param(
    [string]$Port = "COM4",
    [int]$BaudRate = 115200,
    [string]$CliPath = "arduino-cli",
    [switch]$Clean,
    [switch]$SkipUpload,
    [switch]$SkipMonitor
)

$ErrorActionPreference = "Stop"
$repo = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$sketch = Join-Path $repo "firmware\arduino\esp32shell"
$build = Join-Path $repo ".build\arduino"
$partitions = Join-Path $sketch "partitions.csv"

$cli = Get-Command $CliPath -ErrorAction SilentlyContinue
if (-not $cli) {
    throw "Arduino CLI was not found. Install it and ensure '$CliPath' is on PATH."
}
if (-not (Test-Path -LiteralPath $partitions)) {
    throw "Partition table not found: $partitions"
}

if ($Clean -and (Test-Path -LiteralPath $build)) {
    Remove-Item -LiteralPath $build -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $build | Out-Null

& $cli.Source compile `
    --fqbn esp32:esp32:esp32s3 `
    --board-options "FlashSize=32M,PSRAM=opi" `
    --build-path $build `
    --warnings all `
    $sketch
if ($LASTEXITCODE -ne 0) { throw "Arduino CLI compile failed with exit code $LASTEXITCODE." }

$flashArgsPath = Join-Path $build "flash_args"
if (-not (Test-Path -LiteralPath $flashArgsPath)) {
    throw "Build verification failed: flash_args was not produced."
}
$flashArgs = Get-Content -LiteralPath $flashArgsPath -Raw
if ($flashArgs -notmatch "--flash-size\s+32MB") {
    throw "Build verification failed: flash_args does not specify --flash-size 32MB."
}
$partitionBinary = Get-ChildItem -LiteralPath $build -Filter "*.partitions.bin" -File | Select-Object -First 1
$firmwareBinary = Get-ChildItem -LiteralPath $build -Filter "*.ino.bin" -File | Select-Object -First 1
if (-not $partitionBinary -or -not $firmwareBinary) {
    throw "Build verification failed: firmware or partition binary is missing."
}

Write-Host "Build verified: $($firmwareBinary.Name), $($partitionBinary.Name), flash size 32MB"

if (-not $SkipUpload) {
    & $cli.Source upload `
        --fqbn esp32:esp32:esp32s3 `
        --port $Port `
        --input-dir $build
    if ($LASTEXITCODE -ne 0) { throw "Arduino CLI upload failed with exit code $LASTEXITCODE." }
}

if (-not $SkipMonitor) {
    Write-Host "Opening UART shell on $Port at $BaudRate baud. Press Ctrl-C to exit the monitor."
    & $cli.Source monitor --port $Port --config "baudrate=$BaudRate"
    if ($LASTEXITCODE -ne 0) { throw "Arduino CLI monitor exited with code $LASTEXITCODE." }
}
