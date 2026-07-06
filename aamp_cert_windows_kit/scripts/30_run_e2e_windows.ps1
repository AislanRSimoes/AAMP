# 30_run_e2e_windows.ps1 - Passo 3 - E2E de GUI e render (P-04, P-05).
. "$PSScriptRoot\lib_common.ps1"
Write-Step "Passo 3 - E2E GUI + render PDF/PNG (P-04, P-05)"

Push-Location $RepoRoot
try {
    & cmake --preset windows-release -DAAMP_WITH_JUCE=ON -DAAMP_E2E=ON   # AJUSTE
    & cmake --build build/windows --config Release
    Invoke-Logged "test_output_windows_e2e.log" "ctest" @("--test-dir", "build/windows", "-C", "Release", "-R", "E2E", "--output-on-failure") | Out-Null

    # AJUSTE: comando do app para exportar amostra a partir de um ResultBundle de loopback.
    $renderDir = Join-Path $EvidenceDir "render"
    & "$RepoRoot\build\windows\Release\AAMP.exe" --export-sample --out $renderDir

    # Sinal objetivo para TC-054 (PNG >= 1920x1080)
    $png = Get-ChildItem $renderDir -Filter *.png -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($png) {
        $sz = Get-PngSize $png.FullName
        Write-Host ("MEDIDO: PNG {0} = {1}x{2}" -f $png.Name, $sz.Width, $sz.Height)
        if ($sz.Width -ge 1920 -and $sz.Height -ge 1080) {
            Write-Host "  >= 1920x1080 OK (TC-054 dimensao)" -ForegroundColor Green
        } else {
            Write-Host "  ABAIXO de 1920x1080 (TC-054 reprova)" -ForegroundColor Red
        }
    } else {
        Write-Host "Nenhum PNG gerado em rc_evidence/render - verificar exportacao." -ForegroundColor Yellow
    }

    Note-OperatorDecision "P-04" "6 secoes + 15 familias exibidas; TC-040..045 (RUNBOOK 6)"
    Note-OperatorDecision "P-05" "PDF 6 secoes A4 paisagem >=150 DPI (TC-053); PNG marcadores (TC-054)"
}
finally {
    Pop-Location
}
