param (
    [Parameter(Mandatory=$true)]
    [string]$Target
)

$BuildDir = "build"
$ExePath = Join-Path (Get-Location) "$BuildDir\$Target.exe"
# 期待されるプロジェクトファイルのパス
$ProjPath = "$BuildDir\$Target.vcxproj"

# 1. そもそも build フォルダがない、または指定されたターゲットの vcxproj がない場合
if (-not (Test-Path "$BuildDir\CMakeCache.txt") -or -not (Test-Path $ProjPath)) {
    if (-not (Test-Path $ProjPath) -and (Test-Path "$BuildDir\CMakeCache.txt")) {
        Write-Host "--- New target '$Target' detected. Updating CMake cache ---" -ForegroundColor Yellow
    } else {
        Write-Host "--- Configuring CMake ---" -ForegroundColor Cyan
    }
    
    # CMake を再実行してファイル構成をスキャンし直す
    cmake -B $BuildDir
    
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error: CMake configuration failed. Check if $Target.cpp exists." -ForegroundColor Red
        exit $LASTEXITCODE
    }
}

# 2. ビルド実行 (Release構成)
# 確実に build ディレクトリを指すように引数の順序を整理
Write-Host "--- Building: $Target (Release) ---" -ForegroundColor Green
cmake --build $BuildDir --config Release --target $Target

if ($LASTEXITCODE -ne 0) { 
    Write-Host "Build Failed." -ForegroundColor Red
    exit $LASTEXITCODE 
}

# 3. 実行ファイルの実行
if (Test-Path $ExePath) {
    Write-Host "--- Running: $Target ---" -ForegroundColor Magenta
    & $ExePath

    # 4. chip.svg のリネーム処理
    if (Test-Path "chip.svg") {
        $NewName = "$Target.svg"
        Move-Item -Path "chip.svg" -Destination $NewName -Force
    }
} else {
    Write-Host "Error: Executable not found at $ExePath" -ForegroundColor Red
}
