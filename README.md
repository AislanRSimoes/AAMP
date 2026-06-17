# AAMP — Núcleo Determinístico (Fase 1) + Pacote de Evidências

**Suíte de referência:** AAMP v2.0.0 (DOC-00 a DOC-13, DOC-IDX, DOC-AUD, DOC-CR)
**Autoridade:** DOC-00 prevalece sobre qualquer item em conflito.
**Derivação:** todo o código deriva exclusivamente da suíte aprovada. Nenhum comportamento
não documentado foi introduzido; nenhum requisito implícito foi assumido; nenhum algoritmo
de DOC-03 foi substituído.

---

## 1. O que esta entrega É — e o que NÃO é

Esta entrega é o **núcleo headless determinístico (Fases 1–4 — Blocos A, B, C, D, G-headless, E-lógica)**:
M-03, M-05, M-04, M-08, **M-07 (CSV/WAV)** e a **lógica de M-01/M-02 (Hardware/Routing)**, com o DSP de suporte, o contrato `ResultBundle`,
as fronteiras `IAudioDevice`/`IDeviceEnumerator`/`IPdfWriter`/`IPngWriter` e o harness de loopback. Os testes
foram **executados de fato** (286 checks / 34 casos, 0 falhas) e a cobertura **medida com gcov**.

Esta entrega **NÃO é um Release Candidate**. Pela definição da própria suíte
(DOC-12 §3, Plano de Testes §7/§9, Plano de Implementação §11), o RC só existe quando
**P-01..P-09 estiverem cumpridas no ambiente que a suíte exige**: Windows 10/11 + macOS 12+,
hardware de áudio real, build Release sem warnings em MSVC **e** AppleClang, e testes
E2E + SLAs na máquina de referência. Esses portões (P-03, P-07, E2E, SLAs) são, por
construção da suíte, da **máquina-alvo** — não deste ambiente headless Linux. Apresentá-los
como concluídos aqui violaria a regra de honestidade do próprio plano.

## 2. Resultado verificável (executado neste ambiente)

- **314 checks, 0 falhas / 41 casos** — `rc_evidence/test_output_ci_headless.log`.
- Verdes: **TC-010..013** (M-03), **TC-030..037** (M-05), **TC-020..024** (M-04),
  **TC-060..063** (M-08), **TC-050..055** (M-07 CSV/WAV), **TC-001/003/004** (M-01) + routing/ER-022/ER-025 (M-02) + base DSP.
- **Cobertura ≥ 80%** nas áreas-meta Analysis e Stimulus (agregado ~97%); M-07 97%, M-01 80%, M-02 93%, M-04 100%, M-08 81% — `rc_evidence/coverage/`.
- **Zero warnings** em `src/` com `-Wall -Wextra -Wpedantic` — `rc_evidence/scope_audit.log`.
- **Auditoria P-08/P-09**: 0 ocorrências fora de escopo — `rc_evidence/scope_audit.log`.

## 3. Conformidade com DOC-00 e a filosofia operacional

- **Modelo automático único**: `generateSuite()` produz as 16 excitações na ordem A→B→C;
  `computeAllMetrics()` calcula as 15 famílias. Nenhuma seleção de estímulo/métrica/modo/método.
- **Hardware agnóstico**: o núcleo não referencia fabricante/modelo; descoberta de dispositivo
  pertence a M-01 (camada de plataforma, máquina-alvo).
- **Sem persistência/histórico/comparação**: o núcleo apenas calcula e entrega `ResultBundle`.
- **Determinismo**: seeds fixos (MT/Voss-McCartney = 1), double-precision, métodos exatos de
  DOC-03 (deconvolução, janela Blackman-Harris 4-termos, envelope Hilbert + média móvel 1 ms).
- **Enquadramento**: as métricas descrevem o sistema sob teste — sem reconhecimento/inferência.

## 4. Estrutura (DOC-09 §3)

```
src/common/      ResultBundle, Constants (sem números mágicos)
src/analysis/    Fft, Windows, Hilbert, Deconvolution, AnalysisEngine + metrics/
src/stimulus/    StimulusGenerator (16 estímulos) + Prng
tests/           TestRunner + casos TC-010..013, TC-030..037, janelas + LoopbackHarness
rc_evidence/     Pacote de evidências (Plano de Testes §9)
CMakeLists.txt   Build turnkey (núcleo headless + ganchos de máquina-alvo)
CMakePresets.json
build_and_test.sh  Reproduz o build/execução headless
```

## 5. Como reproduzir

```
./build_and_test.sh        # compila o núcleo, roda os testes, imprime a cobertura
# ou, com CMake:
cmake --preset linux-headless-cov && cmake --build build/linux && ctest --preset headless
```

## 6. O que falta para o RC (mapa de blocos do Plano de Implementação)

| Bloco | Módulo | Onde conclui | Estado |
|---|---|---|---|
| A/B | M-03, M-05 (+DSP) | CI headless | ✅ entregue e testado |
| C | M-08 Orchestrator (estados, progresso, recuperação 3/1/1) | CI (mocks) | ✅ entregue e testado |
| D | M-04 Capture (sinc, clipping, averaging) | loopback | ✅ entregue e testado |
| E | M-01/M-02 Hardware/Routing | CI (lógica) + máquina-alvo (OS) | ✅ lógica testada; OS real (hot-plug/desconexão) na máquina-alvo |
| F | M-06 Dashboard (6 seções, gráficos) | máquina-alvo (GUI) | ⏳ skeleton turnkey (`DashboardComponent.h`) |
| G | M-07 Export (CSV/WAV headless; PDF/PNG render) | CI + máquina-alvo | ✅ CSV/WAV testados; PDF/PNG na máquina-alvo |
| 5 | Integração + RC (P-03/P-07/E2E/SLAs) | máquina-alvo Win+macOS+HW | ⏳ |

> **Scaffolding turnkey de máquina-alvo** (não compilado aqui; requer JUCE): `src/hardware/JuceDeviceBackend.h`,
> `src/export/render/JuceRenderWriters.h`, `src/dashboard/DashboardComponent.h`, `.github/workflows/ci.yml`.
> Detalhes em `rc_evidence/TARGET_MACHINE.md`.

## 7. Decisões reservadas registradas (DOC-08 §5 / Plano §2)

- **RD-01 FFT**: núcleo usa Cooley-Tukey radix-2 double-precision próprio (DOC-03 §4.1),
  atrás da fronteira `Fft`; substituível por JUCE DSP FFT na máquina-alvo sem alterar chamadas.
- **RD-02 WAV**: I/O via JUCE AudioFormat na camada de export (máquina-alvo).
- **RD-03 PDF**: atrás de interface `IPdfWriter` (biblioteca permissiva a registrar).
- **RD-04 Teste**: harness mínimo estilo Catch2; Catch2/GTest na máquina-alvo (CTest).

*DOC-00 é a autoridade. Em qualquer conflito, DOC-00 prevalece.*
