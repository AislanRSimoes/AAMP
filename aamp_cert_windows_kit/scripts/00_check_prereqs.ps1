# 00_check_prereqs.ps1 — verifica o ambiente da máquina-alvo Windows.
. "$PSScriptRoot\lib_common.ps1"
Write-Step "Verificacao de pre-requisitos (RC-Windows-1)"

$checks = @(
  @{ Name = "cmake";  Cmd = { cmake --version } },
  @{ Name = "MSVC cl"; Cmd = { cl 2>&1 | Select-Object -First 1 } },
  @{ Name = "ctest";  Cmd = { ctest --version } }
)
foreach ($c in $checks) {
  try { $out = & $c.Cmd; Write-Host ("[ok]  {0}: {1}" -f $c.Name, ($out | Select-Object -First 1)) }
  catch { Write-Host ("[!!]  {0}: NAO ENCONTRADO" -f $c.Name) -ForegroundColor Red }
}

Write-Host ""
Write-Host "Confirme manualmente:" -ForegroundColor Yellow
Write-Host "  - Visual Studio 2022 (MSVC v143) com workload C++ Desktop"
Write-Host "  - JUCE disponivel conforme CMakePresets.json do repositorio"
Write-Host "  - Interface de audio real com driver ASIO (preferencial) ou WASAPI"
Write-Host "  - Esta na maquina de REFERENCIA para a medicao de SLAs (Passo 5)"
Initialize-Evidence
Write-Host ""
Write-Host "Pronto. Siga o RUNBOOK_RC_WINDOWS.md a partir do Passo 1."
