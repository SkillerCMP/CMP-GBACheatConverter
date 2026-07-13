param(
    [switch]$KeepBuild
)

$ErrorActionPreference = 'Stop'
$root = $PSScriptRoot
$build = Join-Path $root 'build-windows-x64'
$dist = Join-Path $root 'dist-windows-x64'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$log = Join-Path $root "build-$stamp-warnings-errors.log"

function Invoke-Logged {
    param(
        [Parameter(Mandatory = $true)][string]$FilePath,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    "> $FilePath $($Arguments -join ' ')" | Tee-Object -FilePath $log -Append
    & $FilePath @Arguments 2>&1 | Tee-Object -FilePath $log -Append
    if ($LASTEXITCODE -ne 0) {
        throw "$FilePath failed with exit code $LASTEXITCODE"
    }
}

if (-not $KeepBuild -and (Test-Path $build)) {
    Remove-Item $build -Recurse -Force
}

Invoke-Logged cmake @(
    '-S', $root,
    '-B', $build,
    '-A', 'x64',
    '-DCMAKE_SUPPRESS_REGENERATION=ON',
    '-DGBA_WARNINGS_AS_ERRORS=ON'
)
Invoke-Logged cmake @('--build', $build, '--config', 'Release', '--parallel')
Invoke-Logged ctest @('--test-dir', $build, '-C', 'Release', '--output-on-failure')

if (Test-Path $dist) {
    Remove-Item $dist -Recurse -Force
}
New-Item -ItemType Directory -Path $dist | Out-Null

$exe = Join-Path $build 'Release\GbaCheatConverter.exe'
if (-not (Test-Path $exe)) {
    throw "Windows executable was not created: $exe"
}
Copy-Item $exe $dist
Copy-Item (Join-Path $root 'README.md') $dist
Copy-Item (Join-Path $root 'LICENSE') $dist
Copy-Item (Join-Path $root 'NOTICE.md') $dist

$hash = Get-FileHash (Join-Path $dist 'GbaCheatConverter.exe') -Algorithm SHA256
"SHA-256 $($hash.Hash)  GbaCheatConverter.exe" |
    Set-Content (Join-Path $dist 'SHA256.txt') -Encoding ASCII

"Build passed. One dual-mode executable was copied to: $dist" |
    Tee-Object -FilePath $log -Append
"Log: $log"
