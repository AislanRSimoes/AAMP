# RUNBOOK — Certificação Windows do AAMP (emissão do RC-Windows-1)

**Autoridade:** DOC-00. **Não altera escopo, requisitos ou algoritmos.** Apenas executa,
na máquina-alvo Windows, os portões que a suíte define como de máquina-alvo, e coleta a
evidência real exigida pelo Plano de Testes §9.

**Ambiente exigido:** Windows 10/11 x64 · Visual Studio 2022 (MSVC v143) · CMake ≥ 3.21 ·
JUCE (conforme o repositório) · uma interface de áudio real com driver **ASIO** (preferencial)
ou **WASAPI** · máquina de referência para SLAs (DOC-09 §6/§7).

**Layout assumido:** este kit fica na raiz do repositório AAMP (com `src/`, `tests/`,
`CMakePresets.json`). Builds em `build/windows`. Saídas de evidência em `rc_evidence/`.

---

## 0. Sequência de fechamento dos portões (visão geral)

Execute os passos **em ordem**. Cada passo só inicia com o anterior aprovado, porque os
artefatos encadeiam (o build alimenta E2E, render, hardware e SLAs; tudo alimenta o §9).

| # | Passo | Script | Portão(ões) | TCs cobertos |
|---|---|---|---|---|
| 1 | Build MSVC Release `/W4 /WX` | `10_build_msvc_release.ps1` | **P-07** | — (build limpo) |
| 2 | Re-verificação do núcleo no Windows | `20_run_unit_integration.ps1` | P-01 (base) | TC-010..013, 020..024, 030..037, 050..052/055, 060..063 |
| 3 | E2E de GUI + render | `30_run_e2e_windows.ps1` | **P-04**, **P-05** | TC-040..045, TC-053(render), TC-054 |
| 4 | Suíte automática em hardware real | `40_run_hardware_suite.ps1` | **P-03** | TC-001, TC-002, TC-005, TC-060 (HW) |
| 5 | Medição dos SLAs | `50_measure_slas.ps1` | RC (DOC-09 §6/§7) | — (DSP/captura/memória) |
| 6 | Montagem do pacote de evidências | `60_collect_evidence.ps1` | **§9** | consolida 1–5 |
| 7 | Sign-off e emissão | manual (§7 deste runbook) | todos | trava de emissão |

> P-06, P-08 e P-09 já estão CONCLUÍDOS na baseline (independentes de plataforma) e são
> apenas reconfirmados no checklist, sem reexecução.

---

## 1. Passo 1 — Build MSVC Release `/W4 /WX`  → fecha P-07

**Objetivo:** o build Release deve compilar **sem warnings em `src/`** sob `/W4 /WX` (MSVC).

```powershell
# Configurar (gera build/windows com backends JUCE)
cmake --preset windows-release -DAAMP_WITH_JUCE=ON          # AJUSTE: nome do preset

# Confirmar que /W4 /WX estão ativos para MSVC em src/ (definidos no CMakeLists — HANDOFF item 2).
# NÃO editar algoritmos. Se os flags não estiverem presentes, apenas garanta a flag de compilação.

# Compilar Release com log bruto capturado
cmake --build build/windows --config Release 2>&1 | Tee-Object rc_evidence/build_msvc_release.log
```

**Evidência gerada:** `rc_evidence/build_msvc_release.log`.

**Critério de aprovação (P-07):**
- Exit code do build = 0; **e**
- Zero linhas de `warning` referentes a arquivos sob `src/` no log (com `/WX`, qualquer
  warning vira erro e quebra o build — logo, build verde já implica zero warnings).

**Reprovação:** qualquer warning/erro em `src/`. Corrigir apenas o que o compilador apontar,
sem alterar comportamento documentado (DOC-03). Re-rodar.

> **Escopo Windows-1:** AppleClang/macOS fica **fora** do RC-Windows-1 (ver RPT-RC-WIN-1, OBS-2).
> P-07 pleno da suíte ainda exigirá AppleClang no marco de duas plataformas.

---

## 2. Passo 2 — Re-verificação do núcleo no Windows  → sustenta P-01

**Objetivo:** confirmar no Windows os casos já verdes em headless (regressão de plataforma).

```powershell
# Build Debug com sanitizers para a varredura (ASan/UBSan quando suportado pelo toolset)
cmake --preset windows-debug -DAAMP_WITH_JUCE=ON            # AJUSTE
cmake --build build/windows-debug --config Debug

# Rodar a suíte unit+integração (loopback virtual), saída bruta capturada
ctest --test-dir build/windows-debug -C Debug --output-on-failure 2>&1 `
  | Tee-Object rc_evidence/test_output_windows_unit.log
```

**Critério:** 100% dos casos unit/integração PASS no Windows; contagem final idêntica em
número de casos à baseline headless. (Referência da baseline: 347 checks / 45 casos / 0 falhas
— ver OBS-1 no RPT-RC-WIN-1; reconciliar a narrativa numérica antes do §9.)

---

## 3. Passo 3 — E2E de GUI e render  → fecha P-04 e P-05

**Objetivo:** renderizar o dashboard (M-06) e os exportadores PDF/PNG (M-07) e verificar os
casos de Sistema. Compilar com os specs E2E habilitados.

```powershell
cmake --preset windows-release -DAAMP_WITH_JUCE=ON -DAAMP_E2E=ON   # AJUSTE
cmake --build build/windows --config Release

# Executar os E2E guardados por AAMP_E2E (TC-040..045, TC-053 render, TC-054)
ctest --test-dir build/windows -C Release -R "E2E" --output-on-failure 2>&1 `
  | Tee-Object rc_evidence/test_output_windows_e2e.log

# Gerar artefatos de render para inspeção/anexo
# AJUSTE: caminho/flag do app para exportar a partir de um ResultBundle de loopback
.\build\windows\Release\AAMP.exe --export-sample --out rc_evidence\render
```

**Evidência gerada:** `test_output_windows_e2e.log`, mais os arquivos em `rc_evidence/render/`
(PDF e PNG de amostra) e capturas de tela das 6 seções, se aplicável.

**Critérios (resumo; detalhe na seção 5):**
- **P-04:** 6 seções renderizadas (TC-040) e as 15 famílias exibidas; interações (TC-041),
  estados vazio/carregando (TC-042/043), tooltip multi-curva (TC-045), progresso sem
  detalhes técnicos (TC-044).
- **P-05:** PDF com 6 seções, A4 paisagem, ≥150 DPI (TC-053); PNG ≥1920×1080 com marcadores
  (TC-054); CSV/WAV e N/A já verdes na baseline são reconfirmados.

---

## 4. Passo 4 — Suíte automática em hardware real  → fecha P-03

**Objetivo:** rodar **um único comando** (modelo automático único) com hardware de áudio real,
sem qualquer seleção de estímulo/métrica/modo pelo usuário.

```powershell
# Configurar a interface de áudio em Settings (única tela de configuração).
# Iniciar a suíte automática completa em hardware real e capturar o console:
.\build\windows\Release\AAMP.exe --run-suite --log rc_evidence\hw_suite_run.log   # AJUSTE

# TC-002 (hot-plug): conectar um dispositivo APÓS o init e confirmar que aparece sem refresh.
# TC-005 (desconexão): desconectar durante a suíte e confirmar ER-002 + aborto seguro (sem crash).
```

**Evidência gerada:** `rc_evidence/hw_suite_run.log` + anotação de TC-001/002/005 no checklist.

**Critério (P-03):** a suíte completa executa de ponta a ponta em hardware real; TC-001
(enumeração só de conectados), TC-002 (hot-plug), TC-005 (desconexão segura) aprovados.

---

## 5. Passo 5 — Medição dos SLAs (DOC-09 §6/§7)

**Objetivo:** confirmar os limites de desempenho na **máquina de referência**.

```powershell
.\build\windows\Release\AAMP.exe --run-suite --report-timing `
  --log rc_evidence\slas_run.log    # AJUSTE: flag de timing/memória
```

**Critérios objetivos:**
- Tempo de cálculo DSP ≤ **15 s**.
- Tempo de captura ≤ **110 s**.
- Pico de memória ≤ **2 GB**.

Registrar os três números medidos em `rc_evidence/slas_run.log` e no checklist.

---

## 6. Critérios objetivos de aprovação/reprovação por TC pendente

Aplicam-se aos casos que **só fecham no Windows**. Os demais já estão verdes em headless
(baseline) e são reconfirmados no Passo 2.

| TC | Portão | APROVA se… | REPROVA se… |
|---|---|---|---|
| TC-001 | P-03/P-01 | lista apenas dispositivos conectados ao OS; nomes do driver; fallback "Canal {n}" | há catálogo estático ou dispositivo desconectado listado |
| TC-002 | P-03 | dispositivo conectado após o init aparece **sem** refresh manual | exige refresh ou não aparece |
| TC-005 | P-03 | desconexão na suíte dispara ER-002 e aborta com segurança (sem crash/estado inconsistente) | crash, trava ou estado inconsistente |
| TC-040 | P-04 | as 6 seções (Summary/Technical/Dynamic/Musical/Noise/Stereo) renderizam | qualquer seção ausente |
| TC-041 | P-04 | zoom, pan, crosshair, tooltip, ocultar/exibir curva e exportar funcionam | qualquer interação ausente/quebrada |
| TC-042 | P-04 | estado vazio: eixos sem curva + mensagem | curva fantasma ou ausência de mensagem |
| TC-043 | P-04 | estado carregando: skeleton animado | tela congelada/sem feedback |
| TC-044 | P-04 | progresso sem timestamps/logs/nomes de módulo | vaza detalhe técnico |
| TC-045 | P-04 | tooltip exibe os valores de **todas** as curvas na posição | tooltip parcial/ausente |
| TC-053 | P-05 | PDF com as 6 seções, A4 paisagem, ≥150 DPI | seção ausente ou DPI/orientação fora |
| TC-054 | P-05 | PNG ≥1920×1080 **e** com marcadores incluídos | resolução menor ou sem marcadores |
| TC-060 (HW) | P-03 | 1 comando dispara a suíte completa em hardware real | exige seleção manual ou não completa |
| TC-061 | P-04 | nova medição pede confirmação antes de descartar | descarta sem diálogo |
| TC-062 | P-04 | fechar durante MEASURING abre diálogo e aborta com segurança | fecha sem aviso/aborto |

---

## 7. Roteiro de coleta de evidências (§9)

Rodar `scripts/60_collect_evidence.ps1` **após** os Passos 1–5. Ele apenas **copia/agrega** as
saídas reais já geradas para a estrutura `rc_evidence/` (não inventa nada). Itens a garantir:

1. **Cobertura** em `rc_evidence/coverage/` (HTML lcov/OpenCppCoverage + resumo). Meta: ≥ 80%
   em Analysis, Stimulus, Export.
2. **`test_list.csv`** — todos os TC-001..063 com nível, ambiente (CI headless / loopback /
   Windows HW / Windows GUI) e status PASS/FAIL/SKIP (com justificativa).
3. **`test_output_windows.log`** — concatenação dos logs brutos por build (Debug ASan/UBSan;
   Release) e por execução (unit, e2e, hw, slas).
4. **`tc_fr_ac_map.csv`** — cada TC → FR → AC → estímulos/métricas → ponteiro de evidência.
5. **`acceptance_checklist.md`** — AC-M01-01…AC-M08-04 e P-01…P-09, cada um com status,
   ambiente e artefato comprobatório.
6. **`release_preconditions.md`** — atualizado para o resultado Windows.

### Formato esperado dos logs

Runner de unidade/integração (mesmo formato da baseline) — **EXEMPLO DE FORMATO**, não evidência:

```
[TC] TC-010 Suite gera 16 estimulos em ordem A->B->C
[TC] TC-030 15 familias calculadas (ou N/A), nenhuma Error
...
==== <N> checks passed, <M> failed ====
```

`build_msvc_release.log` — esperado terminar sem `warning`/`error` em `src/`:

```
[X  /YY] Building CXX object src/...obj
...
Build succeeded.    (0 Warning(s)  0 Error(s))   ← EXEMPLO DE FORMATO
```

`slas_run.log` — três medidas explícitas:

```
DSP_COMPUTE_SECONDS=<medido>      (SLA ≤ 15)
CAPTURE_SECONDS=<medido>          (SLA ≤ 110)
PEAK_MEMORY_MB=<medido>           (SLA ≤ 2048)
```

---

## 8. Procedimento de emissão do RC-Windows-1

A emissão só ocorre quando **todas** as travas abaixo estiverem verdes com evidência real
anexada em `rc_evidence/`. Esta é a regra do Plano de Testes §9: sem o pacote, é status de
progresso, não RC.

**Trava de emissão (todas obrigatórias):**

- [ ] P-07 — build MSVC Release `/W4 /WX` sem warnings em `src/` (`build_msvc_release.log`).
- [ ] P-03 — suíte automática completa em hardware real; TC-001/002/005/060(HW) aprovados.
- [ ] P-04 — 15 famílias exibidas; TC-040..045 aprovados.
- [ ] P-05 — PDF (TC-053) e PNG (TC-054) válidos; CSV/WAV reconfirmados.
- [ ] SLAs — DSP ≤15 s, captura ≤110 s, memória ≤2 GB medidos na referência.
- [ ] §9 — `rc_evidence/` completo: coverage, test_list, test_output, tc_fr_ac_map,
      acceptance_checklist, release_preconditions.
- [ ] P-01 — reconfirmado (MUST verificados, agora incl. E2E Windows).
- [ ] P-02 — sem CAT-1/3/4 em aberto após E2E (ER-001/004 cobertos).
- [ ] P-06 / P-08 / P-09 — reconfirmados (já CONCLUÍDOS na baseline).

**Passos de emissão:**
1. Preencher `acceptance_checklist.md` com os status reais e ponteiros de evidência.
2. Atualizar `release_preconditions.md` (e a narrativa numérica — OBS-1) para o estado Windows.
3. Anexar `rc_evidence/` completo.
4. Só então declarar **RC-Windows-1** — com a ressalva (OBS-2) de que o RC **pleno** da suíte
   (Plano §7) ainda requer macOS (AppleClang + E2E macOS).

> **Não** declarar RC com qualquer trava em aberto. **Não** preencher status sem evidência.
> Em conflito, **DOC-00 prevalece**.

*Fim — RUNBOOK RC-Windows-1 · derivado de AAMP v2.0.0 · DOC-00 é autoridade.*
