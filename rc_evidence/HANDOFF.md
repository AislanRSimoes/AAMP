# AAMP — Documento de Consolidação e Handoff (Fases 1–4)

**Autoridade:** DOC-00. **Escopo:** exclusivamente medição e caracterização (DOC-01 §3).
**Status global:** núcleo headless determinístico **verificado de fato**; portões de GUI/hardware/
duas-plataformas **turnkey** para a máquina-alvo. **Isto não é um Release Candidate** (definição
da própria suíte: P-01..P-09 só fecham no ambiente exigido — DOC-12 §3, Plano de Testes §7).

## 1. O que está verificado de fato (executado aqui)
- **347 checks / 45 casos, 0 falhas** (gcov; `-Wall -Wextra -Wpedantic` zero warnings).
- Módulos com lógica/serialização completas e testadas: **M-03, M-05, M-04, M-08, M-07(CSV/WAV)**,
  **lógica de M-01/M-02**, **view-model de M-06**.
- Trio-meta de cobertura (DOC-11 §4): Analysis, Stimulus, Export **todos ≥ 80%**.
- Auditoria de escopo (P-08/P-09): 0 violações efetivas.

## 2. Mapa de TCs por estado
| Estado | TCs |
|---|---|
| VERIFICADO (CI headless) | TC-010,011,012,013, TC-030..037, TC-020..024, TC-060..063, TC-050,051,052,055, TC-003,004, TC-040,042 |
| MODELO verificado / render na máquina-alvo | TC-053 (relatório 6 seções), TC-044 (progresso) |
| PARCIAL (lógica aqui / OS real na máquina-alvo) | TC-001 (enumeração) |
| PENDENTE — máquina-alvo (hardware/GUI/2 plataformas) | TC-002, TC-005 (E2E HW), TC-041, TC-043, TC-045 (GUI), TC-054 (PNG) |

## 3. Checklist ordenado para fechar o RC na máquina-alvo (turnkey)
1. `cmake --preset windows-release -DAAMP_WITH_JUCE=ON` e `macos-release` — compila app + backends.
2. **P-07**: build Release deve sair com `/W4 /WX` (MSVC) e `-Werror` (AppleClang) **sem warnings em src/**.
3. **M-01/M-04**: `JuceDeviceBackend.h` provê enumeração/stream reais; rodar TC-002 (hot-plug) e TC-005 (desconexão).
4. **M-06**: `Main.cpp` + `DashboardComponent` renderizam o `DashboardModel` (já testado); rodar TC-040..045.
5. **M-07 render**: `JuceRenderWriters.h` (PDF A4 paisagem ≥150 DPI; PNG ≥1920×1080); rodar TC-053(render)/054.
6. **P-03**: suíte automática completa em hardware real, Windows **e** macOS (CI `.github/workflows/ci.yml`).
7. **SLAs** (DOC-09 §6/§7): medir DSP ≤ 15 s, captura ≤ 110 s, memória ≤ 2 GB na máquina de referência.
8. **P-04/P-05**: confirmar 15 famílias exibidas e exportação válida nos 4 formatos com N/A correto.
9. Atualizar DOC-13 e este pacote; só então declarar RC com o Pacote de Evidências completo.

## 4. Conformidade com DOC-00 (reafirmação)
- Modelo automático único (`runSuite()`); usuário não escolhe estímulo/métrica/modo/método.
- Hardware agnostic: descoberta só via OS; sem catálogo/marca; ASIO/WASAPI/CoreAudio são backends, não dependências de escopo.
- Sem profiling, modelagem/captura neural, classificação, reconhecimento, inferência, DB, histórico, DAW ou editor.
- M-06/M-07 apenas renderizam/serializam o `ResultBundle` de M-05; nenhum recálculo fora de M-05.

## 5. Scaffolding turnkey presente no pacote (não verificado headless)
- `src/ui/Main.cpp` — app JUCE (M-01→M-08→M-06).
- `src/dashboard/DashboardComponent.h` + `DashboardSections.h` — render das 6 seções (consome o view-model testado).
- `src/hardware/JuceDeviceBackend.h` — backends reais ASIO/WASAPI/CoreAudio.
- `src/export/render/JuceRenderWriters.h` — PDF/PNG.
- `tests/e2e/E2E_Specs.cpp` — TC-002/005/041/043/045/053(render)/054 (guardados por `AAMP_E2E`).
- `.github/workflows/ci.yml` — matriz Linux/Windows/macOS.

> Estes arquivos **não alteram a contagem verificada (347/45)**; compilam e rodam na máquina-alvo.
