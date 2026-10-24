param([string]$Mode = 'releasedbg')
$ErrorActionPreference = 'Stop'
$repoRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$dll = Join-Path $repoRoot "build\windows\x64\$Mode\AlchemyReworked.dll"
if (!(Test-Path -LiteralPath $dll)) { throw "Build the DLL first: $dll" }
$distribution = Join-Path $repoRoot 'contrib\Distribution\Paths of Empyrean'
$stage = Join-Path $repoRoot ('dist-release\stage-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path (Join-Path $stage 'SKSE\Plugins') -Force | Out-Null
Copy-Item -LiteralPath $dll -Destination (Join-Path $stage 'SKSE\Plugins\AlchemyReworked.dll')
Copy-Item -LiteralPath (Join-Path $distribution 'SKSE\Plugins\AlchemyReworked.yaml') -Destination (Join-Path $stage 'SKSE\Plugins\AlchemyReworked.yaml')
Copy-Item -LiteralPath (Join-Path $distribution 'Alchemy Reworked - Paths of Empyrean.esp') -Destination $stage
Copy-Item -LiteralPath (Join-Path $repoRoot 'LICENSE'), (Join-Path $repoRoot 'README.md') -Destination $stage
$archive = Join-Path $repoRoot 'dist-release\AlchemyReworked-1.3.0-PoE-RU.zip'
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $archive -Force
Get-FileHash -LiteralPath $archive -Algorithm SHA256
