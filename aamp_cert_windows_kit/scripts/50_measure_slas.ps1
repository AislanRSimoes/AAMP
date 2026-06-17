# 50_measure_slas.ps1 — Passo 5 — SLAs DOC-09 §6/§7 na maquina de referencia.
. "$PSScriptRoot\lib_common.ps1"
Write-Step "Passo 5 — Medicao de SLAs (maquina de referencia)"

$log = Join-Path $EvidenceDir "slas_run.log"
# AJUSTE: flag de timing/memoria do app.
& "$RepoRoot\build\windows\Release\AAMP.exe" --run-suite --report-timing --log $log

Write-Host ""
Write-Host "Limites objetivos a confirmar no log:" -ForegroundColor Yellow
Write-Host "  DSP_COMPUTE_SECONDS <= 15"
Write-Host "  CAPTURE_SECONDS     <= 110"
Write-Host "  PEAK_MEMORY_MB      <= 2048"
Note-OperatorDecision "SLAs" "DSP<=15s, captura<=110s, memoria<=2GB (RUNBOOK 5)"
