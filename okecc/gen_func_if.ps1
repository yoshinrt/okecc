param(
	[Parameter(Mandatory = $true)]
	[string]$FilePath
)

# ファイル全体を1つの文字列として読み込む
$t = Get-Content -Path $FilePath -Raw

$t = $t `
	-replace '^[\S\s]*?OKE_CHIP_DEF_BEGIN', '' `
	-replace 'OKE_CHIP_DEF_END[\S\s]*', '' `
	-replace '//.*', '' `
	-replace "[\r\n \t]+", ' ' `
	-replace '/\*.*?\*/', ' '

$matches = [regex]::Matches($t, '\b\w+ *\([^)]*\bLastLocationArg\b.*?\)')

foreach ($m in $matches) {
	$p = $m.Value `
		-replace '[, ]*LastLocationArg.*', '' `
		-replace " *([,\(\)]) *", '$1' `
		-replace '[\(\),]+$', '' `
		-split '[\(\),]+'
	
	$name = $p[0]
	$params = ''
	
	for($i = 1; $i -lt $p.Length; ++$i){
		Write-Host ("<" + $p[$i] + ">")
		if($p[$i] -cmatch '^CChipVar'){
			$params += "ARGT_VAR, "
		}else{
			$params += "ARGT_INT, "
		}
	}
	$params = "{$params}"
	
	Write-Host $name $params
}

# 確認用（必要なければ削除）
#Write-Output $t
