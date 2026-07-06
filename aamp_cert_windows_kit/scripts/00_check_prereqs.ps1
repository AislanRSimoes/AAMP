# 00_check_prereqs.ps1 - verifica o ambiente da maquina-alvo Windows.
. "$PSScriptRoot\lib_common.ps1"
Write-Step "Verificacao de pre-requisitos (RC-Windows-1)"

# Detecta presenca no PATH SEM executar a ferramenta.
# NAO usar 'cl 2>&1': o cl escreve o banner em stderr e, com $ErrorActionPreference='Stop',
# isso viraria erro terminante e geraria um falso "NAO ENCONTRADO".
function Test-Tool([string]$name) {
    $cmd = Get-Command $name -ErrorAction SilentlyContinue
    if ($cmd) {
        $loc = if ($cmd.Source) { $cmd.Source } else { $cmd.Name }
        Write-Host ("[ok]  {0}: {1}" -f $name, $loc) -ForegroundColor Green
        return $true
    }
    Write-Host ("[!!]  {0}: NAO ENCONTRADO no PATH" -f $name) -ForegroundColor Red
    return $false
}

$okCmake = Test-Tool "cmake"
$okCl    = Test-Tool "cl"
$okCtest = Test-Tool "ctest"

# Versoes (cmake/ctest escrevem em stdout; seguro). Nao executamos 'cl' so para versao.
if ($okCmake) { Write-Host ("      " + (cmake --version | Select-Object -First 1)) }
if ($okCtest) { Write-Host ("      " + (ctest --version | Select-Object -First 1)) }

Write-Host ""
if (-not $okCl) {
    Write-Host "cl ausente do PATH: abra o 'x64 Native Tools Command Prompt for VS 2022'" -ForegroundColor Yellow
    Write-Host "(ou rode vcvars64.bat) ANTES de iniciar o PowerShell, e re-execute." -ForegroundColor Yellow
}
Write-Host "Confirme manualmente:" -ForegroundColor Yellow
Write-Host "  - Visual Studio 2022 (MSVC v143) com workload C++ Desktop"
Write-Host "  - JUCE disponivel conforme CMakePresets.json do repositorio"
Write-Host "  - Interface de audio real com driver ASIO (preferencial) ou WASAPI"
Write-Host "  - Esta na maquina de REFERENCIA para a medicao de SLAs (Passo 5)"
Initialize-Evidence
Write-Host ""
if ($okCmake -and $okCl -and $okCtest) {
    Write-Host "Ambiente OK. Siga o RUNBOOK_RC_WINDOWS.md a partir do Passo 1." -ForegroundColor Green
} else {
    Write-Host "Resolva os itens [!!] antes do Passo 1." -ForegroundColor Yellow
}
