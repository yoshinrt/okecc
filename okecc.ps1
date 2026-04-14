param (
	[Parameter(Mandatory=$true)]
	[string]$Target
)

if (-not (Test-Path Env:VisualStudioVersion)) {
	# 1. vswhere を使って Visual Studio のインストールパスを取得
	$installPath = & "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe" -latest -property installationPath
	
	if (-not $installPath) {
		Write-Error "Visual Studio not found"
		return
	}
	
	# 2. vcvarsall.bat のパスを構成 (アーキテクチャは x64 をデフォルトに設定)
	$vcvarsbat = Join-Path $installPath "VC\Auxiliary\Build\vcvarsall.bat"
	$arch = "x64"
	
	# 3. バッチを実行して環境変数を書き出し、PowerShell で読み込む
	# cmd /c で実行し、'set' コマンドの結果を解析する
	$envVars = cmd /c "`"$vcvarsbat`" $arch && set"
	
	foreach ($line in $envVars) {
		if ($line -match "^([^=]+)=(.*)$") {
			$name = $matches[1]
			$value = $matches[2]
			# 既存の環境変数を上書き更新
			Set-Item -Path "Env:\$name" -Value $value
		}
	}
	
	Write-Host "MSVC $arch environment variables was sucsessfully imported." -ForegroundColor Cyan
}

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
	cmake -S okecc -B $BuildDir -A x64
	
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

	# 4. okecc.svg のリネーム処理
	if (Test-Path "okecc.svg") {
		$NewName = "$Target.svg"
		Move-Item -Path "okecc.svg" -Destination $NewName -Force
	}
} else {
	Write-Host "Error: Executable not found at $ExePath" -ForegroundColor Red
}
