param(
  [Parameter(Position=0)][string]$Map = 'gm_construct',
  [string]$Settings = '',
  [string]$ProbeReport = '',
  [int]$Port = 8765
)
$ErrorActionPreference = 'Stop'
$repository = Split-Path $PSScriptRoot -Parent
if (!$Settings) { $Settings = Join-Path $repository '_work/testing-install.json' }
$install = Get-Content -LiteralPath $Settings -Raw | ConvertFrom-Json
$bsp = if (Test-Path -LiteralPath $Map -PathType Leaf) { (Resolve-Path -LiteralPath $Map).Path }
       else { Join-Path $install.source "garrysmod/maps/$Map.bsp" }
if (!(Test-Path -LiteralPath $bsp -PathType Leaf)) { throw "BSP not found: $bsp" }
$name = [IO.Path]::GetFileNameWithoutExtension($bsp)
$document = Join-Path $repository "_work/cubemaps/$name.json"
$runtime = Join-Path $install.target "garrysmod/data/openremix/cubemaps/$name.cubemaps.bin"
$editorArguments = @('-m', 'probe_editor', $bsp, '--output', $document, '--runtime-output', $runtime, '--port', $Port)
if ($ProbeReport) { $editorArguments += @('--probe-report', (Resolve-Path -LiteralPath $ProbeReport).Path) }
Push-Location $PSScriptRoot
try { & python @editorArguments; exit $LASTEXITCODE }
finally { Pop-Location }
