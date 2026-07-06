# Decisao de Toolchain - RC-Windows-1 (registro para o pacote §9)

A baseline (DOC-00..13, README, planos) cita "MSVC v143 / VS 2022" no texto e "JUCE" SEM versao.
As escolhas abaixo sao decisoes de maquina-alvo, registradas para reprodutibilidade.
Sao DESVIOS/decisoes a documentar - nao requisitos implicitos da baseline.

| Item | Baseline | Usado no RC-Windows-1 | Natureza |
|---|---|---|---|
| Compilador | VS 2022 / MSVC v143 | VS 18 Insiders / MSVC 19.51 (14.51.36231) | Desvio (preview) |
| Gerador CMake | "Visual Studio 17 2022" (preset) | Ninja (via CMakeUserPresets, override local) | Adaptacao |
| JUCE (versao) | "JUCE" (sem versao) | JUCE 8.0.12, tag git | Decisao (baseline omissa) |
| Provisionamento JUCE | find_package(JUCE) (sem metodo) | Install local do fonte -> CMAKE_PREFIX_PATH | Decisao |

Observacoes:
- JUCE 8.0.12 inclui o ASIO SDK sob GPLv3; avaliar implicacao de licenca para distribuicao
  comercial (DOC-08 ja exige avaliacao de licenca). WASAPI nao tem essa implicacao.
- O scaffolding JUCE nao havia sido compilado antes; ajustes de API encontrados ficam aqui.
- Repositorio nao modificado: o JUCE entra so via CMakeUserPresets.json (override local).
