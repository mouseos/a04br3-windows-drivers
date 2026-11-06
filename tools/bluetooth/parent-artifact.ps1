# Parent artifact provenance checks only. No compiler, driver or device calls.
function Get-A04ParentRuntimeBlobs {
    param([string]$Root,[string]$Commit='HEAD')
    $scope=@('drivers/bus/mt8167-shared','drivers/input/a04br3-buttons/pmic_read.c',
      'drivers/input/a04br3-buttons/pmic_read.h','drivers/usb/mt8167-usb20/gpio22.c',
      'drivers/usb/mt8167-usb20/gpio22.h','drivers/usb/mt8167-usb20/board_pins.h',
      'drivers/audio/mt8167-afe/diagnostic.h','tools/ci/build-msdc-parent-probe.ps1')
    $tree=@(& git -C $Root ls-tree -r $Commit -- @scope)
    if($LASTEXITCODE -ne 0 -or !$tree.Count){throw 'Cannot identify parent runtime blobs'}
    $blobs=@{}
    foreach($line in $tree){
        if($line -notmatch '^100(?:644|755) blob ([0-9a-f]{40})\t(.+)$'){throw 'Unexpected parent source tree entry'}
        $id=$Matches[1];$path=$Matches[2]
        if($path -match '\.[ch]$' -or $path -eq 'drivers/bus/mt8167-shared/prototype/shared-parent.inf' -or
           $path -eq 'tools/ci/build-msdc-parent-probe.ps1'){$blobs[$path]=$id}
    }
    if(!$blobs.ContainsKey('drivers/bus/mt8167-shared/wdf_parent_probe.c')){throw 'Parent runtime source missing'}
    return $blobs
}
function Assert-A04ParentBlobs {
    param($Expected,$Actual)
    if($Expected.Count -ne $Actual.Count){throw 'Parent runtime blob set changed'}
    foreach($path in $Expected.Keys){if($Expected[$path] -cne $Actual[$path]){throw "Parent runtime blob mismatch: $path"}}
}
function Get-A04ParentHash {
    param([string]$Path)
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}
function Get-A04ParentSourceCommit {
    param([string]$Root,[string]$Commit)
    if($Commit -notmatch '^[0-9a-fA-F]{7,40}$'){throw 'Parent source commit must be an explicit Git commit ID'}
    $resolved=(& git -C $Root rev-parse --verify "$Commit`^{commit}")
    if($LASTEXITCODE -ne 0 -or $resolved -notmatch '^[0-9a-f]{40}$'){throw 'Parent source commit is unavailable'}
    return $resolved
}
function Write-A04ParentArtifact {
    param([string]$Root,[string]$Directory,$Flags,[bool]$IntegratedUsb,[string]$ReuseDirectory)
    $commit=Get-A04ParentSourceCommit $Root (& git -C $Root rev-parse HEAD)
    $runtime=Get-A04ParentRuntimeBlobs $Root $commit
    # Refuse source edits hidden behind HEAD, even for a wrapper-only build.
    foreach($path in $runtime.Keys){
        $working=(& git -C $Root hash-object -- $path)
        if($LASTEXITCODE -ne 0 -or $working -cne $runtime[$path]){throw "Parent working source differs from commit: $path"}
    }
    $parentCommit=$commit;$reused=$false;$inputManifestHash=$null;$archive=$null
    if($ReuseDirectory){
        $source=[IO.Path]::GetFullPath($ReuseDirectory)
        if(!$source.StartsWith($Root+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase)){
            throw 'ReuseParentDirectory must be inside this checkout (preserve its build directory when updating source)'
        }
        $sameDirectory=$source.TrimEnd([IO.Path]::DirectorySeparatorChar) -eq $Directory.TrimEnd([IO.Path]::DirectorySeparatorChar)
        $inputManifest=Join-Path $source 'manifest.json'
        $m=Get-Content -LiteralPath $inputManifest -Raw|ConvertFrom-Json
        $inputManifestHash=Get-A04ParentHash $inputManifest
        if($null -eq $m.source_dirty -or $m.source_dirty -ne $false){throw 'Reused parent must attest a clean source checkout'}
        $parentCommit=Get-A04ParentSourceCommit $Root $(if($m.parent_source_commit){$m.parent_source_commit}else{$m.source_commit})
        foreach($flag in $Flags.Keys){
            if($null -eq $m.parent_flags.$flag -or [string]$m.parent_flags.$flag -cne [string]$Flags[$flag]){
                throw "Reused parent flag missing or different: $flag"
            }
        }
        Assert-A04ParentBlobs $runtime (Get-A04ParentRuntimeBlobs $Root $parentCommit)
        if($m.runtime_blobs){
            $recorded=@{};foreach($property in $m.runtime_blobs.PSObject.Properties){$recorded[$property.Name]=$property.Value}
            Assert-A04ParentBlobs $runtime $recorded
        }
        $expected=$m.sys_sha256
        if(!$expected){
            $entries=@($m.files|Where-Object{($_.path -replace '\\','/') -match '(^|/)mt8167-shared\.sys$'})
            if($entries.Count -ne 1){throw 'Reused parent SYS hash missing or ambiguous'}
            $expected=$entries[0].sha256
        }
        if($expected -notmatch '^[0-9a-fA-F]{64}$' -or (Get-A04ParentHash (Join-Path $source 'mt8167-shared.sys')) -ne $expected){throw 'Reused parent SYS hash mismatch'}
        $inf=Join-Path $Root 'drivers\bus\mt8167-shared\prototype\shared-parent.inf'
        if((Get-A04ParentHash (Join-Path $source 'shared-parent.inf')) -ne (Get-A04ParentHash $inf)){throw 'Reused parent INF differs from fixed source'}
        # All checks precede output. No fallback compile on an incomplete manifest.
        New-Item -ItemType Directory -Force $Directory|Out-Null
        $archive='source-manifest.'+$inputManifestHash+'.json'
        Copy-Item -LiteralPath $inputManifest -Destination (Join-Path $Directory $archive) -Force
        if((Get-A04ParentHash (Join-Path $Directory $archive)) -ne $inputManifestHash){throw 'Parent source manifest copy mismatch'}
        if(!$sameDirectory){
            Copy-Item -LiteralPath (Join-Path $source 'mt8167-shared.sys') -Destination $Directory -Force
            Copy-Item -LiteralPath (Join-Path $source 'shared-parent.inf') -Destination $Directory -Force
        }
        if((Get-A04ParentHash (Join-Path $Directory 'mt8167-shared.sys')) -ne $expected){throw 'Copied parent SYS hash mismatch'}
        $reused=$true
    }
    $sys=Join-Path $Directory 'mt8167-shared.sys';$inf=Join-Path $Directory 'shared-parent.inf'
    $dirty=@(& git -C $Root status --porcelain);if($LASTEXITCODE){throw 'Cannot inspect source state'}
    $result=@{format=1;source_commit=$commit;source_dirty=($dirty.Count -ne 0);parent_source_commit=$parentCommit;
      parent_only=$true;parent_reused=$reused;input_manifest_sha256=$inputManifestHash;input_manifest_archive=$archive;parent_flags=$Flags;
      runtime_blobs=$runtime;diagnostic_interface=11;integrated_usb=$IntegratedUsb;shared_parent_count=1;
      audio_built=$false;client_built=$false;usb_built=$false;hardware_verified=$false;
      sys_sha256=(Get-A04ParentHash $sys);inf_sha256=(Get-A04ParentHash $inf);
      files=@(@{path='mt8167-shared.sys';sha256=(Get-A04ParentHash $sys)},@{path='shared-parent.inf';sha256=(Get-A04ParentHash $inf)})}
    $result|ConvertTo-Json -Depth 6|Set-Content -Encoding UTF8 (Join-Path $Directory 'manifest.json')
    return $result
}
