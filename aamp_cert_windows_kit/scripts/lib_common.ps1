# lib_common.ps1 - helpers para os scripts de certificacao Windows do AAMP.
# Principio: executar comandos reais e CAPTURAR a saida real. Nunca emitir PASS fabricado.

$ErrorActionPreference = "Stop"

# Repo root = DOIS niveis acima de scripts/  (scripts -> aamp_cert_windows_kit -> RAIZ DO REPO).
# Sobrescrevivel com $env:AAMP_REPO_ROOT se o kit estiver em outro lugar.
if ($env:AAMP_REPO_ROOT) {
    $RepoRoot = (Resolve-Path $env:AAMP_REPO_ROOT).Path
} else {
    $RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
}
$EvidenceDir = Join-Path $RepoRoot "rc_evidence"

# Aviso claro se o RepoRoot nao parece ser a raiz do repo.
if (-not (Test-Path (Join-Path $RepoRoot "CMakePresets.json"))) {
    Write-Host ("[AVISO] Nao encontrei CMakePresets.json em '{0}'." -f $RepoRoot) -ForegroundColor Yellow
    Write-Host '        Coloque o kit em <repo>\aamp_cert_windows_kit\ ou defina $env:AAMP_REPO_ROOT.' -ForegroundColor Yellow
}

function Initialize-Evidence {
    if (-not (Test-Path $EvidenceDir)) { New-Item -ItemType Directory -Path $EvidenceDir | Out-Null }
    New-Item -ItemType Directory -Force -Path (Join-Path $EvidenceDir "coverage") | Out-Null
    New-Item -ItemType Directory -Force -Path (Join-Path $EvidenceDir "render")   | Out-Null
}

function Write-Step([string]$msg) {
    Write-Host ""
    Write-Host "==== $msg ====" -ForegroundColor Cyan
}

# Executa um comando, ecoa e captura a saida bruta (stdout+stderr) num log; retorna o exit code real.
# IMPORTANTE: cmake/cl escrevem em stderr mesmo em SUCESSO. Por isso, neste bloco,
# $ErrorActionPreference vira 'Continue' para que stderr nao aborte o script. A decisao de
# sucesso/falha fica por conta do EXIT CODE retornado, avaliado pelo chamador.
function Invoke-Logged([string]$logName, [string]$exe, [string[]]$cmdArgs) {
    $logPath = Join-Path $EvidenceDir $logName
    Write-Host "> $exe $($cmdArgs -join ' ')"
    $prev = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $out  = & $exe @cmdArgs 2>&1
        $code = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $prev
    }
    if ($null -eq $code) { $code = -1 }
    # Grava o log e ecoa no console SEM emitir os objetos para o stream de saida
    # (senao a saida do comando "vazaria" para o valor de retorno da funcao).
    $lines = $out | ForEach-Object { $_.ToString() }
    $lines | Set-Content -Path $logPath -Encoding UTF8
    Add-Content -Path $logPath -Value "EXIT_CODE=$code"
    $lines | Out-Host
    Write-Host "exit code: $code  (log: rc_evidence/$logName)"
    return $code
}

function Get-SrcWarningCount([string]$logName) {
    $logPath = Join-Path $EvidenceDir $logName
    if (-not (Test-Path $logPath)) { return -1 }
    $hits = Select-String -Path $logPath -Pattern "warning" -SimpleMatch | Where-Object { $_.Line -match "[\\/]src[\\/]" }
    return $hits.Count
}

function Get-PngSize([string]$path) {
    Add-Type -AssemblyName System.Drawing
    $img = [System.Drawing.Image]::FromFile((Resolve-Path $path))
    $size = [pscustomobject]@{ Width = $img.Width; Height = $img.Height }
    $img.Dispose()
    return $size
}

function Note-OperatorDecision([string]$gate, [string]$criterion) {
    Write-Host "[REGISTRAR] $gate - verificar contra criterio: $criterion" -ForegroundColor Yellow
    Write-Host "            Anote o resultado em rc_evidence\acceptance_checklist.md" -ForegroundColor Yellow
}
