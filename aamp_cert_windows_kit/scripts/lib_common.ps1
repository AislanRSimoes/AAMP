# lib_common.ps1 — helpers para os scripts de certificação Windows do AAMP.
# Princípio: executar comandos reais e CAPTURAR a saída real. Nunca emitir PASS fabricado.

$ErrorActionPreference = "Stop"
$RepoRoot   = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$EvidenceDir = Join-Path $RepoRoot "rc_evidence"

function Initialize-Evidence {
    if (-not (Test-Path $EvidenceDir)) { New-Item -ItemType Directory -Path $EvidenceDir | Out-Null }
    New-Item -ItemType Directory -Force -Path (Join-Path $EvidenceDir "coverage") | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $EvidenceDir "render")   | Out-Null
}

function Write-Step([string]$msg) {
    Write-Host ""
    Write-Host "==== $msg ====" -ForegroundColor Cyan
}

# Executa um comando, ecoa e captura a saída bruta num log. Retorna o exit code real.
function Invoke-Logged([string]$logName, [string]$exe, [string[]]$args) {
    $logPath = Join-Path $EvidenceDir $logName
    Write-Host "> $exe $($args -join ' ')"
    & $exe @args 2>&1 | Tee-Object -FilePath $logPath
    $code = $LASTEXITCODE
    Add-Content $logPath "EXIT_CODE=$code"
    Write-Host "exit code: $code  (log: rc_evidence/$logName)"
    return $code
}

# Conta linhas de warning referentes a arquivos sob src/ num log de build (sinal objetivo).
function Get-SrcWarningCount([string]$logName) {
    $logPath = Join-Path $EvidenceDir $logName
    if (-not (Test-Path $logPath)) { return -1 }
    return (Select-String -Path $logPath -Pattern "warning" -SimpleMatch `
            | Where-Object { $_.Line -match "[\\/]src[\\/]" }).Count
}

# Lê dimensões de um PNG (sinal objetivo para TC-054).
function Get-PngSize([string]$path) {
    Add-Type -AssemblyName System.Drawing
    $img = [System.Drawing.Image]::FromFile((Resolve-Path $path))
    $size = [pscustomobject]@{ Width = $img.Width; Height = $img.Height }
    $img.Dispose()
    return $size
}

# Lembrete invariável: o script mede; a decisão de portão é humana, contra os critérios.
function Note-OperatorDecision([string]$gate, [string]$criterion) {
    Write-Host "[REGISTRAR] $gate — verificar contra critério: $criterion" -ForegroundColor Yellow
    Write-Host "            Anote o resultado em rc_evidence/acceptance_checklist.md" -ForegroundColor Yellow
}
