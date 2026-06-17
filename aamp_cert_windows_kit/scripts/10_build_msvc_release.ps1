# 10_build_msvc_release.ps1 — Passo 1 — P-07: build MSVC Release /W4 /WX.
. "$PSScriptRoot\lib_common.ps1"
Write-Step "Passo 1 — Build MSVC Release /W4 /WX (P-07)"

Push-Location $RepoRoot
try {
  # AJUSTE: nome do preset se diferir de 'windows-release'.
  & cmake --preset windows-release -DAAMP_WITH_JUCE=ON
  $code = Invoke-Logged "build_msvc_release.log" "cmake" @("--build","build/windows","--config","Release")

  $warn = Get-SrcWarningCount "build_msvc_release.log"
  Write-Host ""
  Write-Host "MEDIDO: exit_code=$code ; warnings_em_src=$warn"
  if ($code -eq 0 -and $warn -eq 0) {
    Write-Host "Sinais objetivos COMPATIVEIS com P-07 (build limpo, 0 warnings em src/)." -ForegroundColor Green
  } else {
    Write-Host "Sinais objetivos INCOMPATIVEIS com P-07. Corrija e re-execute." -ForegroundColor Red
  }
  Note-OperatorDecision "P-07" "build Release MSVC /W4 /WX, 0 warnings em src/ (RUNBOOK 1)"
}
finally { Pop-Location }
