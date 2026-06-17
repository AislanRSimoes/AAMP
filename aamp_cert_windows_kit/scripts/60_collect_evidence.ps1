# 60_collect_evidence.ps1 — Passo 6 — monta rc_evidence/ a partir das saidas REAIS (sem inventar).
. "$PSScriptRoot\lib_common.ps1"
Write-Step "Passo 6 — Montagem do Pacote de Evidencias (§9)"

# 1) Copiar os templates (se rc_evidence ainda nao os tiver)
$tpl = Join-Path $RepoRoot "aamp_cert_windows_kit\rc_evidence_template"
if (Test-Path $tpl) {
  Get-ChildItem $tpl -Recurse -File | ForEach-Object {
    $rel = $_.FullName.Substring($tpl.Length).TrimStart('\','/')
    $dst = Join-Path $EvidenceDir $rel
    New-Item -ItemType Directory -Force -Path (Split-Path $dst) | Out-Null
    if (-not (Test-Path $dst)) { Copy-Item $_.FullName $dst }
  }
}

# 2) Concatenar os logs brutos por build/execucao em test_output_windows.log
$master = Join-Path $EvidenceDir "test_output_windows.log"
"" | Set-Content $master
foreach ($l in @("test_output_windows_unit.log","test_output_windows_e2e.log","hw_suite_run.log","slas_run.log","build_msvc_release.log")) {
  $p = Join-Path $EvidenceDir $l
  if (Test-Path $p) {
    Add-Content $master "===== $l ====="
    Get-Content $p | Add-Content $master
  }
}

Write-Host "Estrutura atual de rc_evidence/:"
Get-ChildItem $EvidenceDir -Recurse | Select-Object FullName

Write-Host ""
Write-Host "PENDENTE (preenchimento humano, com evidencia real):" -ForegroundColor Yellow
Write-Host "  - coverage/: gerar com OpenCppCoverage/lcov (>=80% Analysis/Stimulus/Export)."
Write-Host "  - test_list.csv: marcar PASS/FAIL/SKIP por TC conforme os logs reais."
Write-Host "  - tc_fr_ac_map.csv: preencher coluna Evidencia (ponteiro para o log)."
Write-Host "  - acceptance_checklist.md: status real de AC-* e P-01..P-09."
Write-Host "  - release_preconditions.md: atualizar para o estado Windows (e OBS-1)."
Write-Host ""
Write-Host "So apos tudo verde, siga a Trava de Emissao (RUNBOOK secao 8)."
