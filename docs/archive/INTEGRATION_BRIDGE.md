# AAMP — Ponte HTML ↔ Measurement Engine (integração)

Objetivo: fazer o `aamp_laboratory.html` deixar de simular e passar a **controlar o Engine C++
existente**, entregando a primeira execução completa `HTML → MeasurementOrchestrator → Session →
Laboratório` **sem arquivo intermediário**.

## O que está PRONTO e VERIFICADO neste ambiente (lado frontend)
- `fromEngineSession(payload)` — adaptador de origem ao vivo. Round-trip contra a fixture normativa:
  a Session produzida é **idêntica** à de `fromAamp(.aamp)` (device, MS-01 N=256, 14 métricas,
  15 escalares, 3 capturas, primary). `bootSession()` intacto.
- Casca `AAMP` — comandos/eventos, estados Preparação→Medição→Processamento→Confiança, grupo
  Caracterização no menu, config de hardware, Session única. Todo o fluxo roda hoje sobre o
  transporte simulado.
- Transporte nativo — o JS já reconhece **JUCE 8** (`window.__JUCE__`) e a ponte genérica
  (`window.aampNative` + `window.aampEvent`). Sem host nativo, cai no mock. Trocar mock↔nativo
  não altera a casca.

## O que este pacote ENTREGA para o build (lado C++, scaffolding de máquina-alvo)
> Mesma convenção de `JuceDeviceBackend.h`: código real contra as APIs reais, **não compilado/
> verificado aqui**. Compilar e verificar no build Windows/target.

- `AampWebBridge.h` — host único (`juce::WebBrowserComponent`) que hospeda o HTML e liga
  `HardwareManager` + `MeasurementOrchestrator` ao frontend, in-process (executável único, offline,
  sem IPC). `runSuite` em thread de trabalho; eventos empacotados para a thread de mensagens.
- `SessionSerializer.h` — `ResultBundle → Session JSON` na forma exata que `fromEngineSession`
  consome (contrato provado por round-trip no frontend).

## Contrato da ponte (1:1 com o que o mock já exercita)
| Comando (JS→C++) | API real |
|---|---|
| `enumerate` | `HardwareManager::enumerateDevices()` |
| `prepare{deviceId}` | constrói `IAudioDevice` + `MeasurementOrchestrator::prepare()` |
| `start` | `MeasurementOrchestrator::runSuite()` (bloqueante → worker thread) |
| `cancel` | `MeasurementOrchestrator::requestCancel()` |
| `export{format}` | `ExportEngine` (M-07, DOC-10) |

| Evento (C++→JS) | Origem |
|---|---|
| `devices` | `enumerateDevices()` |
| `state` | derivado de `StateMachine::state()` nos callbacks |
| `progress` | `setProgressCallback(Progress)` |
| `stimulusIntegrity` | `setStimulusCallback(StimulusReport)` |
| `metricReady` | `setResultCallback(MetricResult,…)` |
| `runFinished` | retorno de `runSuite` (`RunResult`) + `serializeSessionVar(bundle)` |

## Wiring no build (CMake/JUCE 8)
1. Adicione `src/bridge/AampWebBridge.h`, `src/bridge/AampWebBridge.cpp` e
   `src/bridge/SessionSerializer.h`.
2. Habilite o módulo do WebView: `juce_gui_extra` com `JUCE_WEB_BROWSER=1`. No Windows,
   o backend é o **WebView2** (runtime da Microsoft) — declarar a dependência/deploy.
3. **Origem do HTML — DEV (temporária): arquivo EXTERNO, sem `BinaryData`.**
   O `AampWebBridge` carrega `aamp_laboratory.html` de disco, permitindo editar HTML/CSS/JS sem
   recompilar. Coloque o arquivo em uma destas localizações (resolução automática, nesta ordem):
   - `<dir do executável>/resources/aamp_laboratory.html`  (relativo — preferido)
   - `C:\aamp\resources\aamp_laboratory.html`               (fallback fixo de dev)
   Nada de `withResourceProvider` / `juce_add_binary_data` nesta fase.
   > **PRODUTO FINAL:** reintroduzir `BinaryData` — `juce_add_binary_data(aamp_laboratory.html)`,
   > `Options{}.withResourceProvider(...)` e `goToURL(getResourceProviderRoot())` — para o
   > executável único, offline, sem dependência de arquivos externos. Só a origem do HTML muda;
   > toda a ponte permanece igual.
4. Instancie o `AampWebBridge` como conteúdo da janela principal, **no lugar do
   `DashboardComponent`** de `Main.cpp` (a apresentação passa a ser o WebView; a GUI JUCE do
   dashboard sai de cena). O restante de `Main.cpp` (device backend, orchestrator) é reutilizado.

> Verificação de dev: com WebView2 carregando `file://`, confirme que a integração nativa injeta
> `window.__JUCE__` na página externa (é o esperado — a integração é do componente, não da URL).
> O JS já é resiliente: reconhece `__JUCE__`, a ponte genérica `window.aampNative` e, sem host,
> cai no mock.

## O que ainda depende da sua máquina (honesto)
1. **Compilar e executar** — não é possível neste ambiente headless (sem JUCE). A "primeira
   execução completa" acontece no build Windows.
2. **`JuceAudioDevice::playAndCapture` está STUB** no projeto (`return {}`), marcado como
   "implementação completa pertence ao build da máquina-alvo". **Sem ela não há captura real** —
   o `runSuite` não obtém resposta. É pré-requisito da aquisição real (não da investigação).
3. **Capturas ao vivo (A2)** — o `ResultBundle` não carrega áudio bruto (as capturas vivem no
   `CaptureEngine`). O serializador manda `captures:[]`; os instrumentos que usam áudio bruto
   degradam graciosamente (como já fazem sem `M.captures`). Para alimentá-los ao vivo, exponha os
   buffers do `CaptureEngine` e preencha `captures` no `SessionSerializer`. Métricas/curvas
   (Confiança, Frequência e a maioria) **não** dependem disso.

## Checklist de verificação (no target)
- [ ] `enumerate` lista os dispositivos reais do OS (sem catálogo estático — DOC-00 §5).
- [ ] `prepare` transita para `Ready`; hardware inválido → `state:Error` sem travar o Laboratório.
- [ ] `start` → progresso amigável (sem ST-xx) → `runFinished(Completed)`.
- [ ] Frontend chama `fromEngineSession(payload)` → `bootSession()` → aterrissa em **Confiança**.
- [ ] Abrir `.aamp` continua funcionando (origem secundária) e **não** depende de hardware.
- [ ] Nenhum arquivo intermediário entre Engine e Laboratório.

Ao final, uma caracterização iniciada no HTML, executada pelo C++ e aberta no Laboratório fecha a
transição de "dois sistemas" para "um produto integrado".
