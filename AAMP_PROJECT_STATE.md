# AAMP_PROJECT_STATE.md — Fonte única de verdade para continuação

> Documento de handoff. Objetivo: permitir que qualquer modelo/IA continue a investigação **exatamente do ponto
> atual**, sem reconstruir semanas de trabalho. Branch de trabalho: `claude/aamp-migration-stage-0-fzuot8`.
> Última atualização: 2026-07 (após F1 implementado e 1ª validação em hardware da Helix LT).

---

## 0. Orientação essencial (leia antes de tudo)
- **AAMP é um sistema de preservação de evidências de caracterização**, não "ferramenta de gráficos". Núcleo = o
  Evidence Package (`.aamp`). Tudo o mais é derivação.
- **"HardwareController" = objeto JavaScript do FRONTEND** (`resources/aamp_laboratory.html`), NÃO uma classe C++.
  Toda a "Hardware Refactoring" (Sprints 2–6, P2-01, B1, U1, F1) aconteceu **no frontend**. O backend só recebeu
  ajustes aditivos (Sprint 1).
- **JUCE/WebView2 NÃO compila/roda no ambiente Linux de desenvolvimento.** Toda validação automatizada é
  **mock + Node/Playwright**. A validação em hardware real é responsabilidade do usuário (Windows/Vostro/Helix).
- **Fase atual = Product Development.** Implementar → Validar → Corrigir → Próximo item. Não reabrir arquitetura por
  bug; não produzir documentação conceitual nova; não reabrir decisões fechadas.

---

## 1. Estado atual da arquitetura

### Backend (C++, `src/`)
Arquitetura de **dois motores** com o Evidence Package como contrato entre eles:
- **Acquisition Engine** — produz **evidência bruta** (formas de onda), não ciência.
  - `StimulusGenerator` (`src/stimulus/`) — 16 estímulos ST-01..ST-16 (determinísticos, seed=1).
  - `CaptureEngine` (`src/capture/`) + `JuceCaptureDevice` (`src/hardware/JuceCaptureDevice.h`) — play+capture real (ASIO/WASAPI via JUCE AudioDeviceManager).
  - `MeasurementOrchestrator` (`src/orchestration/`, "M-08") — `runSuite()` conduz a suíte por estímulo (gera→captura→valida→emite), FSM interna, degradação graciosa.
  - `EvidenceRecorder` + `EvidenceSerializer` (`src/evidence/`) — observam (estímulo, captura) e serializam o `.aamp` de **evidência** (WAVs fonte + captura + metadados).
- **Laboratory / Analysis Engine** — **deriva** a ciência a partir da evidência.
  - `AnalysisEngine` (`src/analysis/AnalysisEngine.cpp`, ~1049 linhas) — MS-01..MS-15 (backend, máquina-alvo).
  - No frontend, o `LabEngine` (JS) reimplementa a derivação para reabrir `.aamp` (`fromEvidence`).
  - `ExportEngine` (`src/export/`, "M-07") — **derivações**: PDF/CSV/WAV(IR)/PNG a partir do `ResultBundle`.
- **Ponte:** `AampWebBridge` (`src/bridge/AampWebBridge.cpp`) — WebView2; comandos `enumerate/prepare/start/cancel/export/getSettings/saveSettings/setLanguage/closeConfirm/diag`. `HardwareManager` (`src/hardware/`) enumera devices.
- **Contrato central:** `ResultBundle` (`src/common/ResultBundle.h`) — MS-01..15 (scalars+curves) + meta.
- **Constantes normativas:** `src/common/Constants.h` (fs 48 kHz, FFT 65536, banda 20–20k, THD H2–H10, Tikhonov λ=1e-6·(1+N/S), roll 500/1000 ms, SNR mín 20 dB, etc.).

### Frontend (`resources/aamp_laboratory.html`, ~3300 linhas, IIFE `const AAMP=(function(){…})()`)
- **Modelos de domínio do hardware (JS, dentro da IIFE):** `HardwareProfile` (Desired, persistido) · `DeviceCapabilities` (Capabilities, volátil) · `PreparedConfiguration`/`preparedConfig` (Applied, efêmero) · `HardwareSession` (Session: requestId, pendingCallback, state/FSM).
- **`HardwareController` (JS, stateless):** único intérprete das intenções; dono de TODA transição da FSM.
- **LabEngine (JS):** deriva MS-01..15 de um `.aamp` de evidência (`fromEvidence`).
- **Instrumentos/UI:** rail, palco, `showDomain`, painéis, LabChart.

### Decisões arquiteturais FECHADAS (não reabrir)
1. **Evidence First** — todo derivado é reconstruível só da evidência primária; nada derivado é autoritativo.
2. **`.aamp` = Evidence Package** (Container de Evidências), artefato primário do produto.
3. **Cadeia canônica:** `Measurement → Evidence → (Analysis Specification ⨯ Analysis) → Results → Presentation`.
4. **Analysis Specification v1 — CONGELADA** (ciência normativa; mudança numérica ⇒ nova versão + DR).
5. **Single Writable Owner + Lifetime Separation** para os 4 modelos de hardware.
6. **Behavior Ownership** — 100% das transições da FSM dentro do `HardwareController` (prova: 12/12, 0 fora).
7. **HardwareController stateless** (Controller Purity: sem campos de estado).
8. Terminologia oficial: Measurement · Evidence Package · Evidence First · Analysis Specification · Results · Presentation.

### Fontes de verdade
- **Código:** `resources/aamp_laboratory.html` (frontend/FSM), `src/bridge/AampWebBridge.cpp`, `src/hardware/JuceCaptureDevice.h`, `src/analysis/AnalysisEngine.cpp`, `src/evidence/*`, `src/common/Constants.h`.
- **Este arquivo** (`AAMP_PROJECT_STATE.md`).
- **Referências internas (scratchpad, não versionadas):** `DR-01-evidence-model.md`, `DS-01-audio-export-strategy.md`, `E2-analysis-specification.md`, `product-backlog.md`, `b1-windows-validation.md`, e testes `.cjs`.
- **Kit Windows:** `aamp_cert_windows_kit/RUNBOOK_RC_WINDOWS.md`.

---

## 2. Linha do tempo da Hardware Refactoring

Todas no frontend salvo indicação. Cada Sprint teve RFC → aprovação → implementação → provas estáticas/funcionais → aceite.

| Item | Objetivo | Arquivos | Bugs corrigidos | Limitações conhecidas |
|---|---|---|---|---|
| **Sprint 1** | `PreparedConfiguration` (value object imutável) + `needsPrepare` (política pura) + `requestId` (correlação) | `aamp_laboratory.html`; **`AampWebBridge.cpp`** (evento `prepared` emite `applied`+`requestId`; `[HWSWITCH]` diag) | Base de decisão de reprepare | Correlação plena só no diálogo (protocolo não leva requestId a todos os eventos de execução) |
| **Sprint 2** | Funções puras `channelBases/selectClosest/validateAgainstCaps/adjustToCaps/clampDesiredToCaps`; `repopulateChannels` = render PURO | `aamp_laboratory.html` | **Mecanismo 1** (render zerava a base antes do 1º prepare) | — |
| **Sprint 3** | `HardwareProfile` — dono único do Desired (persistido) | `aamp_laboratory.html` | Estado Desired sem working-copy duplicada | — |
| **Sprint 4** | `DeviceCapabilities` — snapshot volátil das capacidades | `aamp_laboratory.html` | nIn/nOut sempre em par (Atomic Snapshot) | — |
| **Sprint 5** | `HardwareSession` — contexto de sessão (requestId, pendingCallback, state/FSM) | `aamp_laboratory.html` | Correlação antes de estado | requestId incremental é detalhe (não confiar) |
| **Sprint 6** | `HardwareController` stateless + FSM `Idle·Configuring·CfgPreparing·CfgReady·AcqPreparing·Running·Error` | `aamp_laboratory.html` | Elimina flags booleanas coordenadas à mão | — |
| **P2-01** (`eab0f1c`) | Behavior Consolidation — TODA transição da FSM movida para o `HardwareController`; handlers viram adaptadores finos | `aamp_laboratory.html` | Transições distribuídas em handlers | — |
| **B1** (`807e394`) | `setChannel` reprovisiona ao trocar canal (igual a `setMode`) | `aamp_laboratory.html` | **Mecanismo 2** (diálogo mostrava validade do canal anterior) | **Code Complete / Pending Hardware Validation** |
| **U1** (`8c3c973`) | Config de hardware acessível durante a sessão (⚙ persistente na barra) | `aamp_laboratory.html` | Config só no palco vazio | Grupo Caracterização colapsa em sessão (por design) |
| **F1** (`c2eabcd`) | Evidence Delivery & Derivation — ação Exportar no rail + painel Evidência/Derivações | `aamp_laboratory.html` | Export só no encerramento; evidência invisível | Derivações oficiais só p/ medição deste run (backend tem o bundle); importada mostra só o nome do arquivo |

**Provas de arquitetura (mock+node) todas verdes:** Ownership (1 escritor/campo), Behavior Ownership (12/12 dentro do Controller), Controller Purity, Lifetime, Snapshot Integrity, Correlation, regressão S1–S6.

---

## 3. Estado atual do HardwareController + ciclo

O `HardwareController` (JS, stateless) interpreta **intenções** e emite **comandos** para o backend via `send(...)`.
Mapeamento intenção → comando → backend:

| Fase | Frontend (HardwareController) | Comando | Backend |
|---|---|---|---|
| **Enumerate** | `openConfig`/`onDevices` | `send('enumerate')` | `HardwareManager::enumerateDevices` → evento `devices` |
| **Prepare** | `selectDevice`/`setMode`/`setChannel`/`beginAcquisition` | `send('prepare',{deviceId,mode,inBase,outBase,requestId})` | `doPrepare` abre o device, ativa canais (`outBase→outMask`, `inBase→inMask`), valida, emite `prepared{state,caps,applied,requestId}` |
| **Start** | `beginAcquisition`/`onPrepared` | `send('start')` | `doStart` → thread: `JuceCaptureDevice` + `MeasurementOrchestrator.runSuite` |
| **Capture** | (backend) | — | `JuceCaptureDevice::playAndCapture` por estímulo; emite `state/progress/metricReady` |
| **Stop/Finish** | `onRunFinished` | (fim natural) ou `send('cancel')` | `doStart` emite `runFinished{outcome,aamp,aampPath}`; `doCancel` aborta |
| **Dispose** | `teardown`→`reset` | — | AudioDeviceManager persistente reusado (sem dispose explícito por medição) |

**FSM (transições, todas no HardwareController):** `Idle→Configuring→CfgPreparing→CfgReady→(OK)Idle→(start)AcqPreparing→Running→Idle`; ramos de `Error`. Prova: 12 escritas `HardwareSession.set({state…})`, 100% dentro do Controller.

**Bugs comprovadamente resolvidos:**
- Mecanismo 1 (Sprint 2), Mecanismo 2 (B1, código), `[HWSWITCH]` confirma reconfiguração de canal correta (1 `doPrepare` por mudança efetiva; `outMask` reflete a seleção).
- FSM coerente; sem flags distribuídas; U1/F1 não tocam a FSM (regressão verde).

**Bugs/pendências que permanecem:**
- **Avaliação de Integridade (CONFIRMADO):** captura silenciosa classificada como "Medição confiável" (ver §6, Ponto 2). A corrigir.
- **B1** pendente de confirmação em hardware real.
- **E3 (latente):** `doPrepare` aceita/ecoa "Stereo" mas `doStart` força `ChannelMode::Mono` (`AampWebBridge.cpp:456,488`) → `applied.mode` pode declarar modo não honrado.

---

## 4. Build

- **Sistema:** CMake. `project(AAMP VERSION 2.0.0)`. `CMakePresets.json` na raiz.
- **Targets** (`CMakeLists.txt`):
  - `aamp_core` — **static lib** com todo o núcleo (stimulus/capture/analysis/orchestration/evidence/export/common). Base de tudo.
  - `aamp_app` — **GUI/hardware**, só com `-DAAMP_WITH_JUCE=ON`. `juce_add_gui_app(aamp_app PRODUCT_NAME "AAMP")` → **`AAMP.exe`**. Linka `aamp_core` + JUCE (audio_devices/gui/dsp) + WebView2 estático + ASIO. Fontes extra: `src/ui/Main.cpp`, `src/bridge/AampWebBridge.cpp`.
  - `aamp_tests` — **testes unitários** C++ (`tests/`), linka `aamp_core`. → `aamp_tests[.exe]`.
  - `aamp_validation_harness` — harness de validação científica (`validation/harness/validation_main.cpp`), linka `aamp_core`.
  - `wav_metric_harness` — ferramenta CLI (`tools/wav_metric_harness.cpp`) para métricas sobre WAV avulso.
- **Para que serve cada executável:**
  - `AAMP.exe` — o **produto** (GUI + WebView2 + ASIO). É o que valida no hardware.
  - `aamp_tests[.exe]` — regressão do núcleo científico/lógico (não precisa de hardware/JUCE-GUI).
  - `aamp_validation_harness` — valida a suíte científica contra datasets (`validation/dataset`).
  - `wav_metric_harness` — computa métricas de um WAV isolado (diagnóstico/DSP).
- **Etapa PÓS-BUILD do JUCE (importante):** `add_custom_command(TARGET aamp_app POST_BUILD …)` **copia** `resources/aamp_laboratory.html` para `<dir do AAMP.exe>/resources/aamp_laboratory.html`. **A fonte permanece em `resources/`**; o artefato ao lado do exe é uma cópia. Se editar o frontend, rebuild (ou recopiar) para o exe enxergar.
- **Pré-requisitos Windows:** WebView2 SDK em `C:/webview2` (NuGet extraído; `-DAAMP_WEBVIEW2_SDK=<path>` para ajustar) e ASIO SDK em `C:/asiosdk/common`. Sem `JUCE_USE_WIN_WEBVIEW2=1` o frontend ES6 quebra ("Erro de Script").

---

## 5. Processo correto de compilação

**Windows (produto — o que importa para a validação Helix):**
```powershell
cmake --preset windows-release -DAAMP_WITH_JUCE=ON
cmake --build build/win --config Release
# Executável: build/win/…/Release/AAMP.exe   (JUCE coloca em pasta de artefatos do target)
```
- **Preset:** `windows-release` (generator "Visual Studio 17 2022", `binaryDir=${sourceDir}/build/win`, `CMAKE_BUILD_TYPE=Release`).
- **ATENÇÃO à discrepância de diretório:** o preset usa `build/win`; o `RUNBOOK_RC_WINDOWS.md` referencia `build/windows` e `.\build\windows\Release\AAMP.exe` (rótulos "AJUSTE"). **Confirme o caminho real do exe** após o build (JUCE gera em `…/aamp_app_artefacts/Release/AAMP.exe` ou similar).
- **Não usar Ninja para `aamp_app`** com este preset (é gerador VS). Ninja/Makefiles servem ao preset Linux headless.

**Linux (headless — testes de núcleo, SEM GUI/JUCE):**
```bash
cmake --preset linux-headless-cov     # Unix Makefiles, build/linux, coverage
cmake --build build/linux
ctest --preset headless
```

**Frontend (validação automatizada neste ambiente):** Node 22 + Playwright/Chromium headless carregando `file://…/resources/aamp_laboratory.html` com o MockEngine embutido. (Ver testes `.cjs` no scratchpad.)

**Comandos INCORRETOS / erros comuns já vistos:**
- Compilar `aamp_app` **sem** `-DAAMP_WITH_JUCE=ON` → não gera GUI (só núcleo).
- Faltar WebView2/ASIO SDK → `FATAL_ERROR` no configure.
- Esquecer que o frontend é **copiado** no pós-build → editar `resources/…html` e rodar o exe antigo (frontend desatualizado).
- Rodar JUCE/GUI no Linux → **não compila** (headless).

---

## 6. Estado atual da investigação Helix

### Sintomas (1ª validação em hardware, Vostro + Helix LT, ASIO)
- Build OK; app inicia; WebView2 carrega; ASIO abre; `prepare()`/`start()` OK.
- Todos os 16 estímulos: **`capPk≈0`, `capRMS≈0`, `onsetIdx=-1`**; buffers coerentes (`rawSize`/`capturadoEfetivo`).
- UI: RMS ≈ −96 dBFS, Silêncio 100%, quase tudo "sinal baixo" — **porém veredito "Medição confiável".**
- **Evidência-chave do operador:** o **sweep era audível** saindo da Helix, **mas sem atividade visível na cadeia de processamento** (blocos/medidores) no display.

### Comportamento observado / sequência
- `[HWSWITCH] prepare … ASIO Helix … nIn=8 nOut=8 outMask=1→11 inMask=1→11 ready=1`; `start … outMask=11 inMask=11`.
- `outMask=11` (binário) = canais 0,1 = **USB out 1/2**; `inMask=11` = **USB in 1/2**.
- Baseline residual ≈ −95…−97 dBFS (piso de ruído o tempo todo). `Offset medido` do M04 pula (0/2272/67573) — só ruído.

### Ponto 1 — Aquisição (o que o CÓDIGO faz — certo)
- **TX:** `JuceCaptureDevice.h:135-140` escreve o estímulo em **TODOS os canais de saída ativos** (idênticos) → sai em USB out 1 **e** 2.
- **RX:** `JuceCaptureDevice.h:143-154` captura **apenas o 1º canal de entrada ativo** (`inCh`) → lê só USB record 1.
- **Máscaras:** `AampWebBridge.cpp:315-319` — `outBase→outputChannels.setBit`, `inBase→inputChannels.setBit` (+1 em estéreo). `outBase`/`inBase` são **independentes e selecionáveis**.
- **Modelo implícito do AAMP = loopback direto** (sai no canal N, volta no canal N).
- **Helix (arquitetura USB Line 6):** USB 1/2 playback = **monitoramento** (audível) que **não entra na cadeia DSP** sem um bloco de entrada "USB 1/2" no preset; USB 3/4 = **re-amp** (injeta na cadeia). USB record 1/2 = saída principal. → Explica **exatamente**: sweep audível (monitoramento), cadeia sem atividade (não injetado), captura silenciosa.
- **Conclusão do Ponto 1:** **NÃO é bug do software de aquisição.** É **configuração/roteamento USB da Helix** (ponto de injeção/retorno) + expõe uma **limitação** do modelo loopback do AAMP. **Ainda hipótese a confirmar no hardware.**

### Ponto 2 — Avaliação da Integridade (bug de produto CONFIRMADO no código)
- Veredito global `M._trust` numa linha (`aamp_laboratory.html:2226`):
  `trust = (clip||peakDb>-1)?'bad' : ((peakDb>-6 || M.metrics.lowConf)?'warn' : (q?'ok':'idle'))`.
  `'ok'` → "Medição confiável" (`:2379`).
- **Defeito A:** a escala só protege o lado **ALTO** (clipping/peak). **Não há piso inferior** — captura no piso de ruído (peak ≈ −91 dBFS) cai em `'ok'`.
- **Defeito B:** o termo salva-vidas `M.metrics.lowConf` **nunca é populado no caminho da evidência** — o `LabEngine` (JS) **hardcoda `quality:'Normal'`** em todas as métricas (30× 'Normal', 0× 'LowConfidence' entre linhas 1330–1520); a regra da Analysis Spec (SNR<20 dB → LowConfidence) **não está implementada** aí. Logo `lowConf=0`.
- **Inconsistência que o operador notou:** o por-estímulo `acquisitionFindings()` (`:2204`) marca "sinal baixo" (peak<−40) mas **não alimenta** o veredito global.
- **Conclusão do Ponto 2:** **bug de produto** na avaliação de integridade, localizado e **não-arquitetural**. Correção prevista: gate de "sinal insuficiente" (usar `q.peakDb`/`q.rmsDb` vs piso) e/ou aplicar LowConfidence por SNR. **Aguardando autorização** (o usuário quer fechar o Ponto 1 antes).

### "primeira caracterização falha / segunda funciona"
Padrão **relatado em campo anteriormente** (intermitência), **NÃO reproduzido de forma consistente**: na última validação as **duas** rodadas Helix ficaram silenciosas (ambas em USB 1/2). Permanece **aberto/não explicado**; pode ser efeito de estado do preset/roteamento entre tentativas. Precisa reprodução controlada.

---

## 7. Hipóteses DESCARTADAS (com motivo)
- **Git / branch errada / merge errado** — código compila e roda; B1/U1/F1 presentes no HEAD; árvore limpa/sincronizada.
- **Compilação / inicialização** — build OK, app inicia, WebView2 carrega, ASIO abre, `prepare`/`start` OK.
- **`outMask`/reconfiguração de canal** — `[HWSWITCH]` confirma device aberto correto, máscaras refletem a seleção, **1 `doPrepare` por mudança efetiva** (B1 funciona).
- **Callbacks / fiação do HardwareController** — `prepare`/`start` executam; FSM coerente; regressão verde.
- **Mecanismo 1** (render zerava base) — corrigido na Sprint 2 (render puro + clamp explícito).
- **Mecanismo 2 / B1** (setChannel não reprovisiona) — corrigido em código (807e394).
- **F1/U1 terem quebrado a FSM** — Behavior Ownership 12/12 intacto; regressão S1–S6 verde.

## 8. Hipóteses ainda ABERTAS
1. **Roteamento USB da Helix (injeção/retorno)** — o estímulo em USB 1/2 vai ao monitoramento, não à cadeia DSP; caracterizar um processador exige injetar via re-amp (ex.: USB 3/4 + bloco de entrada no preset) e capturar o canal de record que carrega o processado. **Confirmar no hardware** (testes discriminantes em §11).
2. **Limitação do modelo de aquisição do AAMP** — loopback direto; broadcast em todas as saídas; lê só a 1ª entrada. Pode exigir seleção de canais TX≠RX casada ao mapeamento da Helix (não é bug; é limitação).
3. **Intermitência 1ª/2ª rodada** — não explicada; precisa reprodução.
4. **Avaliação de Integridade** — não é hipótese, é **bug confirmado** a corrigir (Ponto 2).

---

## 9. Estado atual do frontend e interação

- **Laboratório HTML** (`resources/aamp_laboratory.html`) — SPA de uma página; IIFE `AAMP`. Contém: modelos de hardware, `HardwareController` (FSM), `LabEngine` (deriva ciência do `.aamp`), instrumentos (rail/palco/LabChart), i18n pt-BR/en, MockEngine (transporte simulado quando não há host nativo).
- **WebBridge** (`AampWebBridge.cpp`) — WebView2 in-process; expõe `send/on/emit`. Comandos ↔ eventos: `prepare→prepared`, `start→state/progress/metricReady/runFinished`, `export→exported`, `enumerate→devices`, `getSettings→settings`.
- **Measurement Engine (C++)** — orquestra a suíte, captura real, produz o `.aamp` de evidência (`kDeliverEvidence=true`), retém `ResultBundle` (`m_lastBundle`) para o `ExportEngine`.
- **HardwareController (JS)** — traduz intenções do usuário/UI em comandos; dono da FSM; não tem estado próprio.
- **Interação:** usuário → HardwareController → `send(cmd)` → WebBridge/Measurement Engine → eventos → handlers finos → HardwareController atualiza modelos/FSM → View. Reabrir `.aamp`: `ingest→fromAamp/fromEvidence→bootSession→showDomain`. Fresco: `runFinished{aamp,aampPath}` → mesmo caminho de `ingest`.
- **Escopo/encapsulamento (ARMADILHA):** `showDomain`/`bootSession`/`buildRail` vivem **FORA** da IIFE; `HardwareSession`/`HardwareController`/`syncHwGear` vivem **DENTRO**. Código de fora só acessa o interno via API pública (`AAMP.fsmState()` etc.). Referenciar `HardwareSession` de dentro de `showDomain` gera `ReferenceError` (aconteceu no U1 e foi corrigido).

---

## 10. O que um modelo que continua o projeto TENDE A ESQUECER / reabrir sem necessidade
1. **"HardwareController" é FRONTEND (JS), não C++.** Não procurar/criar uma classe C++ com esse nome.
2. **Fronteira IIFE:** `showDomain`/`bootSession`/`buildRail` são outer-scope; não podem referenciar `HardwareSession`/internos — usar `AAMP.fsmState()`. (Fonte de `ReferenceError` engolido por try/catch → aparece como modal, não pageerror.)
3. **Evidence First:** o `.aamp` é a verdade; PDF/CSV/PNG/IR são **derivações**. Não tratar relatório como fonte.
4. **Analysis Specification v1 está CONGELADA** — não alterar silenciosamente a matemática das métricas; mudança numérica exige nova versão + DR.
5. **Behavior Ownership (12/12)** e **Controller Purity** são invariantes — não redistribuir transições de FSM nos handlers nem dar estado ao Controller.
6. **Mock ≠ backend real:** o `.aamp` do MockEngine é formato **análise** (sem WAVs, sem `applied`); o backend real entrega **evidência** (WAVs). Por isso `preparedConfig` fica null no mock (não dá para testar "não repreparar no start" no mock).
7. **JUCE/WebView2 não roda no Linux** — validação = mock+node; hardware = usuário. Não "rodar o app" aqui.
8. **Product Development Phase:** não reabrir arquitetura por bug; não gerar documentação conceitual; não reavaliar decisões fechadas (§1).
9. **Convenção de canais AAMP:** rótulos cruzados — "Entrada" ↔ `outBase` (saída ASIO), "Saída" ↔ `inBase` (entrada ASIO). Confuso; conferir sempre no código.
10. **Aquisição é Mono por construção:** `doStart` usa `ChannelMode::Mono` e `hw.channels=Mono` hardcoded, embora a UI ofereça Stereo (E3, bug latente). O canal secundário é descartado no serializer.
11. **Dois formatos de `.aamp`** (evidência vs análise) e o flag `kDeliverEvidence` (hoje true).
12. **`M.metrics.lowConf` nunca é setado no LabEngine (JS)** — quality hardcoded 'Normal'. Central para o bug do Ponto 2.
13. **Branch única:** `claude/aamp-migration-stage-0-fzuot8`. Não commitar o identificador do modelo em artefatos do repositório.
14. **Post-build copia o frontend** ao lado do exe — editar `resources/…html` exige rebuild/recopiar.
15. **O silêncio da Helix ≠ bug de aquisição** — é roteamento/config USB (hipótese a confirmar) + limitação do modelo loopback; e é **independente** do bug de integridade.

---

## 11. Estado operacional (resumo de uma página)

**Onde o projeto está hoje**
- Arquitetura do subsistema de hardware **consolidada** (Sprints 1–6 + P2-01). Modelo conceitual do núcleo **fechado** (DR-01 + E2 congelada). Em **Product Development Phase**.
- **B1** (código), **U1**, **F1** implementados, testados (mock+node) e enviados. Branch limpa e sincronizada.
  Commits: `807e394` (B1), `8c3c973` (U1), `c2eabcd` (F1); antecedentes `eab0f1c` (P2-01), `6483777` (Sprint 6).

**O que está funcionando**
- Build Windows (JUCE+WebView2+ASIO), inicialização, `prepare`/`start`, reconfiguração de canal (`[HWSWITCH]`), FSM, U1 (config em sessão), F1 (Exportar/painel) — nos testes mock+node e na inicialização real.

**O que falta validar / corrigir**
- **Helix — Ponto 1 (aberto):** confirmar no hardware se o silêncio é roteamento/injeção-retorno USB (monitoramento vs re-amp) — testes discriminantes abaixo.
- **Integridade — Ponto 2 (bug confirmado):** corrigir o veredito (gate de sinal insuficiente + LowConfidence por SNR). Aguardando autorização.
- **B1**, **U1**, **F1**: conferência de comportamento no app real (rodada de validação).
- Emergentes de backlog: **E1** (canal secundário/estéreo), **E3** (prepare aceita Stereo × doStart força Mono), **E4** (multicanal >2), **F2** (Comparison between Evidence Packages).

**Próximo passo (imediato)**
1. **Confirmar o Ponto 1 no hardware** (não assumir "1/2 vs 3/4" como conclusão):
   - No preset da Helix, inserir um **bloco de entrada USB** no início da cadeia e rotear a saída da cadeia a um canal de **USB record** conhecido; confirmar atividade dos blocos com o sweep.
   - No AAMP, definir **Saída (TX)** = par que alimenta esse bloco (ex.: USB 3/4 → `outBase=2`) e **Entrada (RX)** = canal de record com o processado; rodar e observar `capPk>0`, `onsetIdx≥0`.
   - Verificar Global Settings de USB Audio da Helix (destino de USB 1/2; fonte dos canais de record).
2. Com o Ponto 1 fechado (config vs aquisição), **autorizar e implementar a correção do Ponto 2** (bug de integridade), com testes de regressão.
3. Seguir o ciclo Implementar → Validar → Corrigir → próximo item do backlog.
