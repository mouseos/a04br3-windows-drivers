# hive/端末に触れない共通入口。Windows mockもこの実関数を使用する。
function Assert-SignedRenderNativeAdmission {
    param([string]$PolicyPath,[string]$ExpectedSha256,[int]$Capture)
    $mode = & python -B (Join-Path $PSScriptRoot 'signed_render_native_admission.py') --policy $PolicyPath --sha256 $ExpectedSha256 --capture $Capture
    if($LASTEXITCODE -ne 0 -or @($mode).Count -ne 1 -or $mode -cnotin @('terminal-windows-held','unattended-collection')) {
        throw 'Exact signed-render terminal hold or unattended collection contract required'
    }
    return $mode
}
