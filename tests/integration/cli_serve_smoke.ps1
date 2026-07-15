param(
    [Parameter(Mandatory = $true)]
    [string]$BeamDropExe
)

$ErrorActionPreference = 'Stop'
$baseDir = Join-Path ([IO.Path]::GetTempPath()) ("beamdrop-cli-serve-" + [guid]::NewGuid())
$process = $null

try {
    New-Item -ItemType Directory -Path $baseDir | Out-Null

    $listener = [System.Net.Sockets.TcpListener]::new([System.Net.IPAddress]::Loopback, 0)
    $listener.Start()
    $port = ([System.Net.IPEndPoint]$listener.LocalEndpoint).Port
    $listener.Stop()

    $process = Start-Process -FilePath $BeamDropExe -ArgumentList @(
        'serve', '--host', '127.0.0.1', '--port', $port,
        '--save-dir', (Join-Path $baseDir 'received'),
        '--log-file', (Join-Path $baseDir 'serve.log')
    ) -PassThru -RedirectStandardOutput (Join-Path $baseDir 'stdout.txt') `
      -RedirectStandardError (Join-Path $baseDir 'stderr.txt')

    $deadline = [DateTime]::UtcNow.AddSeconds(3)
    $isListening = $false
    while ([DateTime]::UtcNow -lt $deadline) {
        $process.Refresh()
        $isListening = $null -ne (Get-NetTCPConnection -LocalAddress 127.0.0.1 -LocalPort $port `
            -State Listen -ErrorAction SilentlyContinue)
        if (-not $process.HasExited -and $isListening) {
            break
        }
        Start-Sleep -Milliseconds 50
    }

    if ($process.HasExited -or -not $isListening) {
        $stderr = Get-Content (Join-Path $baseDir 'stderr.txt') -Raw -ErrorAction SilentlyContinue
        throw "beamdrop serve did not remain listening on 127.0.0.1:$port. stderr: $stderr"
    }
} finally {
    if ($null -ne $process -and -not $process.HasExited) {
        Stop-Process -Id $process.Id -Force
        $process.WaitForExit()
    }
    Remove-Item -LiteralPath $baseDir -Recurse -Force -ErrorAction SilentlyContinue
}