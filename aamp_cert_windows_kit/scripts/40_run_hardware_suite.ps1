# 40_run_hardware_suite.ps1 - Passo 4 - suite automatica em hardware real (P-03).
. "$PSScriptRoot\lib_common.ps1"
Write-Step "Passo 4 - Suite automatica em hardware real (P-03)"

Write-Host "Pre-condicao: interface de audio real conectada e configurada em Settings." -ForegroundColor Yellow
Write-Host "Modelo automatico unico: um comando dispara a suite completa (sem selecao)." -ForegroundColor Yellow
$log = Join-Path $EvidenceDir "hw_suite_run.log"
# AJUSTE: flags reais do app.
& "$RepoRoot\build\windows\Release\AAMP.exe" --run-suite --log $log

Write-Host ""
Write-Host "Roteiro manual de hardware:" -ForegroundColor Yellow
Write-Host "  TC-001: confirmar que apenas dispositivos conectados sao enumerados."
Write-Host "  TC-002: conectar um dispositivo APOS o init; deve aparecer sem refresh."
Write-Host "  TC-005: desconectar durante a suite; esperar ER-002 + aborto seguro (sem crash)."
Note-OperatorDecision "P-03" "suite completa em HW real; TC-001/002/005/060(HW) (RUNBOOK 4/6)"
