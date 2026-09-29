# Convenient PowerShell script to build the project
$cmakePath = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if (-not (Test-Path $cmakePath)) {
    $cmakePath = "cmake"
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host " Building FileTransferSystem with CMake " -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

& $cmakePath -B build -S .
if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}

& $cmakePath --build build --config Debug
if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed!" -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host ""
Write-Host " Build succeeded!" -ForegroundColor Green
Write-Host "  -> Server: .\build\Debug\server.exe" -ForegroundColor Yellow
Write-Host "  -> Client: .\build\Debug\client.exe" -ForegroundColor Yellow
