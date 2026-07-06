# 10_build_msvc_release.ps1 - Passo 1 - P-07: build MSVC Release /W4 /WX.
. "$PSScriptRoot\lib_common.ps1"
Write-Step "Passo 1 - Build MSVC Release /W4 /WX (P-07)"

# Preset de configure/build. Default 'windows-release'. Para Ninja+cl (Caminho B):
#   $env:AAMP_CONFIGURE_PRESET = "windows-release-ninja"
#   $env:AAMP_BUILD_PRESET     = "windows-release-ninja"
$cfgPreset = if ($env:AAMP_CONFIGURE_PRESET) { $env:AAMP_CONFIGURE_PRESET } else { "windows-release" }
# JUCE ON por padrao. Para validar so o nucleo deterministico: $env:AAMP_WITH_JUCE = "OFF"
$withJuce  = if ($env:AAMP_WITH_JUCE) { $env:AAMP_WITH_JUCE } else { "ON" }

Push-Location $RepoRoot
try {
    Write-Host "Repo root: $RepoRoot"
    Write-Host "Configure preset: $cfgPreset"
    Write-Host "AAMP_WITH_JUCE: $withJuce"
    if (-not (Test-Path "CMakePresets.json")) {
        Write-Host "[ERRO] CMakePresets.json nao esta em $RepoRoot. Abortando Passo 1." -ForegroundColor Red
        Write-Host '       Rode a partir do repo, ou defina $env:AAMP_REPO_ROOT.' -ForegroundColor Yellow
        return
    }

    $cfg = Invoke-Logged "configure_windows_release.log" "cmake" @("--preset", $cfgPreset, "-DAAMP_WITH_JUCE=$withJuce")
    if ($cfg -ne 0) {
        Write-Host "[ERRO] Configure falhou (exit=$cfg). Veja rc_evidence/configure_windows_release.log" -ForegroundColor Red
        Write-Host "       Cheque: nome do preset (cmake --list-presets) e gerador/toolset." -ForegroundColor Yellow
        return
    }

    # Build: por build-preset (recomendado p/ Ninja) ou pelo binaryDir 'build/windows'.
    if ($env:AAMP_BUILD_PRESET) {
        $code = Invoke-Logged "build_msvc_release.log" "cmake" @("--build", "--preset", $env:AAMP_BUILD_PRESET)
    } else {
        $code = Invoke-Logged "build_msvc_release.log" "cmake" @("--build", "build/windows", "--config", "Release")
    }

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
finally {
    Pop-Location
}
