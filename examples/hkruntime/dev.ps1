# Champ local dev: serve the page with a model you made.
#
#   .\examples\bitnet\dev.ps1 -Model C:\Users\grego\2b-qwen.bnz
#   .\examples\bitnet\dev.ps1 -Model C:\path\to\model-folder
#   .\examples\bitnet\dev.ps1            # the stock 2B4T checkpoint
#
# -Model is a hodgemix output (OUT.bnz or OUT.bnz.ternary.safetensors) or a
# folder with model.safetensors. The page always loads /models/champ/; this
# server mounts the chosen folder there, so the model is the server's choice
# and nothing in the page's URL picks it.
#
#   .\examples\bitnet\dev.ps1 -Catalog C:\Users\grego\models\catalog.dev.json
#
# -Catalog serves several models for the page's model menu. The file names each
# one; nothing is looked up:
#   {"models": [
#     {"id": "champ", "name": "BitNet b1.58 2B4T", "dir": "C:\\Users\\grego\\models\\flow\\2b4t"},
#     {"id": "hodge-2b4t-1", "name": "Hodge 2B4T 1", "dir": "C:\\Users\\grego\\models\\dev\\hodge-2b4t-1"}
#   ]}
# Each dir holds the files the page reads: config.json, tokenizer.json, and
# model.bnz or model.safetensors (volume.safetensors for the map). "tier"
# (optional) puts a model in one of Auto's four engine slots (web/router.js):
# lite, balanced, thinking or pro, each at most once. The page gets
# /models/catalog.json with the ids, names and tiers only, and /models/<id>/.
param(
    [string]$Model = "$env:USERPROFILE\models\microsoft\bitnet-b1.58-2B-4T",
    [string]$Name = "",
    [string]$Base = "$env:USERPROFILE\models\microsoft\bitnet-b1.58-2B-4T",
    [string]$Catalog = "",
    [int]$Port = 8792,
    [switch]$NoServe  # link the model folder only (a server on -Port is already running)
)
$ErrorActionPreference = "Stop"
$Repo = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$Models = Join-Path $env:USERPROFILE "models"
$Small = "config.json", "generation_config.json", "tokenizer.json", "tokenizer_config.json", "special_tokens_map.json"

if ($Catalog) {
    if (-not [IO.Path]::IsPathRooted($Catalog)) { throw "-Catalog takes a full path, not $Catalog" }
    $entries = @((Get-Content -Raw -LiteralPath $Catalog | ConvertFrom-Json).models)
    if (-not $entries.Count) { throw "$Catalog lists no models" }
    $mounts = @("ex=examples")
    $listed = @()
    $ids = @{}
    $tiers = @{}
    foreach ($m in $entries) {
        $id = [string]$m.id
        $dir = [string]$m.dir
        if ($id -cnotmatch '^[a-z0-9][a-z0-9-]{0,31}$') { throw "$Catalog`: id '$id' is not 1-32 of a-z, 0-9 and -" }
        if ($ids.ContainsKey($id)) { throw "$Catalog`: id $id is listed twice" }
        $ids[$id] = $true
        if (-not [IO.Path]::IsPathRooted($dir) -or $dir.Contains("..") -or [IO.Path]::GetFullPath($dir) -ne $dir.TrimEnd('\')) {
            throw "$Catalog`: $id's dir must be a full, clean path, not '$dir'"
        }
        foreach ($f in "config.json", "tokenizer.json") {
            if (-not (Test-Path -LiteralPath (Join-Path $dir $f) -PathType Leaf)) { throw "$Catalog`: $id has no $dir\$f" }
        }
        if (-not (Test-Path -LiteralPath (Join-Path $dir "model.bnz") -PathType Leaf) -and
            -not (Test-Path -LiteralPath (Join-Path $dir "model.safetensors") -PathType Leaf)) {
            throw "$Catalog`: $id has neither $dir\model.bnz nor $dir\model.safetensors"
        }
        $name = if ([string]$m.name) { [string]$m.name } else { $id }
        $entry = [ordered]@{ id = $id; name = $name }
        $tier = [string]$m.tier
        if ($tier) {
            if ($tier -cnotin "lite", "balanced", "thinking", "pro") { throw "$Catalog`: $id's tier '$tier' is not lite, balanced, thinking or pro" }
            if ($tiers.ContainsKey($tier)) { throw "$Catalog`: tier $tier is given to $($tiers[$tier]) and $id" }
            $tiers[$tier] = $id
            $entry.tier = $tier
        }
        $listed += $entry
        $mounts += "models/$id=$dir"
        Write-Host ("  {0,-16} {1}  ({2})" -f $id, $dir, $name) -ForegroundColor Cyan
    }
    # Only catalog.json is in this folder, so /models/ shows nothing else.
    $stage = Join-Path $Models "dev\_catalog"
    New-Item -ItemType Directory -Force $stage | Out-Null
    Get-ChildItem -LiteralPath $stage | Remove-Item -Force
    [IO.File]::WriteAllText((Join-Path $stage "catalog.json"), (ConvertTo-Json -Depth 4 -InputObject @{ models = @($listed) }))
    $mounts = @("models=$stage") + $mounts
    $url = "http://127.0.0.1:$Port/ex/hkruntime/web/index.html"
    Write-Host "  open  : $url" -ForegroundColor Green
    if ($NoServe) { return }
    Write-Host "  Ctrl+C stops the server." -ForegroundColor DarkGray
    $env:PORT = "$Port"
    Set-Location $Repo
    $nodeArgs = @("WASMJsLoader\serve.mjs")
    foreach ($m in $mounts) { $nodeArgs += "--mount"; $nodeArgs += $m }
    & node @nodeArgs
    return
}

$Model = (Resolve-Path $Model).Path
if (Test-Path $Model -PathType Container) {
    $weights = Join-Path $Model "model.safetensors"
    $srcDir = $Model
    # A hodgemix output folder: its newest OUT.bnz.ternary.safetensors, named after OUT.
    if (-not (Test-Path $weights)) {
        $newest = Get-ChildItem $Model -Filter "*.ternary.safetensors" | Sort-Object LastWriteTime -Descending | Select-Object -First 1
        if ($newest) {
            $weights = $newest.FullName
            if (-not $Name) { $Name = $newest.Name -replace '\.bnz\.ternary\.safetensors$|\.ternary\.safetensors$', '' }
        }
    }
} else {
    $weights = $Model
    if ($weights -like "*.bnz") { $weights = "$weights.ternary.safetensors" }
    $srcDir = Split-Path -Parent $Model
}
if (-not (Test-Path $weights)) { throw "no weights at $weights (hodgemix writes OUT.bnz.ternary.safetensors)" }

if ($Model.StartsWith($Models + "\", [StringComparison]::OrdinalIgnoreCase) -and (Test-Path (Join-Path $Model "model.safetensors"))) {
    $dir = $Model
} else {
    if (-not $Name) { $Name = ([IO.Path]::GetFileName($weights) -replace '\.bnz\.ternary\.safetensors$|\.safetensors$', '') }
    $dir = Join-Path $Models "dev\$Name"
    New-Item -ItemType Directory -Force $dir | Out-Null
    $link = Join-Path $dir "model.safetensors"
    if (Test-Path $link) { Remove-Item $link -Force }
    try {
        New-Item -ItemType HardLink -Path $link -Target $weights | Out-Null
    } catch {
        Write-Host "hardlink failed ($($_.Exception.Message)); copying" -ForegroundColor Yellow
        Copy-Item $weights $link
    }
    foreach ($f in $Small) {
        $from = Join-Path $srcDir $f
        if (-not (Test-Path $from)) { $from = Join-Path $Base $f }
        if (Test-Path $from) { Copy-Item -Force $from (Join-Path $dir $f) }
    }
    # The page tokenizes with tokenizer.json: one with a different vocabulary
    # than the embeddings loads but reads every token id as another word.
    $cfgNow = Get-Content -Raw (Join-Path $dir "config.json") | ConvertFrom-Json
    $tok = Get-Content -Raw (Join-Path $dir "tokenizer.json") | ConvertFrom-Json -AsHashtable
    $ids = @($tok["model"]["vocab"].Values) + @($tok["added_tokens"] | ForEach-Object { $_["id"] })
    $nTok = ($ids | Measure-Object -Maximum).Maximum + 1
    if ($nTok -gt $cfgNow.vocab_size -or $cfgNow.vocab_size - $nTok -gt 2048) {
        Write-Host "  WARNING tokenizer.json has $nTok tokens, the model's embeddings $($cfgNow.vocab_size): put the model's own tokenizer.json next to it" -ForegroundColor Yellow
    }
    foreach ($f in "config.json", "tokenizer.json") {
        if (-not (Test-Path (Join-Path $dir $f))) { throw "$dir has no $f (none next to the model or in -Base)" }
    }
}

$url = "http://127.0.0.1:$Port/ex/hkruntime/web/index.html"
Write-Host ""
Write-Host "  model : $dir  (served as /models/champ/)" -ForegroundColor Cyan
Write-Host "  open  : $url" -ForegroundColor Green
if ($NoServe) {
    Write-Host "  (no server started: restart this script without -NoServe to serve this model)" -ForegroundColor DarkGray
    return
}
Write-Host "  Ctrl+C stops the server." -ForegroundColor DarkGray
Write-Host ""
$env:PORT = "$Port"
Set-Location $Repo
node WASMJsLoader\serve.mjs --mount "ex=examples" --mount "models/champ=$dir"
