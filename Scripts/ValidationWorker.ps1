# Installation metadata only. Runtime trust comes from the DLL's compiled hash.
function Get-SoggfyValidationWorker([string]$PackageRoot) {
    $descriptor = Join-Path $PackageRoot 'VALIDATION_WORKER.txt'
    $item = Get-Item -LiteralPath $descriptor -Force
    if ($item.PSIsContainer -or $item.Length -gt 66 -or
        ($item.Attributes -band [IO.FileAttributes]::ReparsePoint)) { throw 'Invalid validation helper descriptor' }
    $hash = [IO.File]::ReadAllText($descriptor).Trim()
    if ($hash -cnotmatch '^[a-f0-9]{64}$') { throw 'Invalid validation helper digest' }
    $name = 'SoggfyValidation-' + $hash + '.exe'
    $path = Join-Path $PackageRoot $name
    Assert-SoggfyValidationWorker $path $hash
    return [pscustomobject]@{ Hash = $hash; Name = $name; Path = $path }
}

function Assert-SoggfyValidationWorker([string]$Path, [string]$Hash) {
    $item = Get-Item -LiteralPath $Path -Force
    if ($item.PSIsContainer -or $item.Length -eq 0 -or $item.Length -gt 64MB -or
        ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -or
        (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash -ine $Hash) {
        throw "Validation helper does not match this package; retained: $Path"
    }
}

function Install-SoggfyValidationWorker($Worker, [string]$Root) {
    $target = Join-Path $Root $Worker.Name
    if (Test-Path -LiteralPath $target) {
        Assert-SoggfyValidationWorker $target $Worker.Hash
        return
    }
    $source = $null; $destination = $null
    try {
        $source = [IO.File]::Open($Worker.Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
        Assert-SoggfyValidationWorker $Worker.Path $Worker.Hash
        $destination = [IO.File]::Open($target, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
        $source.CopyTo($destination)
        $destination.Dispose(); $destination = $null
        Assert-SoggfyValidationWorker $target $Worker.Hash
    } finally {
        if ($null -ne $destination) { $destination.Dispose() }
        if ($null -ne $source) { $source.Dispose() }
    }
}
