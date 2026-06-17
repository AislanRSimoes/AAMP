# 20_run_unit_integration.ps1 — Passo 2 — re-verificacao do nucleo no Windows (sustenta P-01).
. "$PSScriptRoot\lib_common.ps1"
Write-Step "Passo 2 — Unit + Integracao no Windows (P-01 base)"

Push-Location $RepoRoot
try {
  & cmake --preset windows-debug -DAAMP_WITH_JUCE=ON         # AJUSTE
  & cmake --build build/windows-debug --config Debug
  $code = Invoke-Logged "test_output_windows_unit.log" "ctest" `
            @("--test-dir","build/windows-debug","-C","Debug","--output-on-failure")
  Write-Host ""
  Write-Host "MEDIDO: ctest exit_code=$code (0 = todos passaram)"
  Note-OperatorDecision "P-01 (base)" "100% unit/integracao PASS no Windows (RUNBOOK 2)"
}
finally { Pop-Location }
