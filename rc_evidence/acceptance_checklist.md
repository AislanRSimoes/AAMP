# AAMP — Checklist de Critérios de Aceite (DOC-12 §2)

Estado: **núcleo headless determinístico (Fases 1–4 — Blocos A, B, C, D, G-headless, E-lógica)**.
Legenda: ✅ verificado (TC verde, executado de fato) · ⏳ pendente (bloco/máquina-alvo).

## M-03 Stimulus Generator
- ✅ AC-M03-01 — 16 estímulos — TC-010
- ✅ AC-M03-02 — Determinismo — TC-011
- ✅ AC-M03-03 — Compostos ≤ 0 dBFS (−1 dBFS) — TC-012
- ✅ AC-M03-04 — Parâmetros conforme DOC-03 — TC-013

## M-05 Analysis Engine
- ✅ AC-M05-01 — 15 famílias (ou N/A justificado) — TC-030, TC-031, TC-036, TC-037
- ✅ AC-M05-02 — THD seno puro < 0.001% — TC-032
- ✅ AC-M05-03 — SNR ±0.5 dB — TC-033
- ✅ AC-M05-04 — Determinismo — TC-034
- ✅ AC-M05-05 — Sem NaN/Inf na deconvolução — TC-035

## M-04 Capture Engine  (Bloco D — loopback virtual)
- ✅ AC-M04-01 — Sincronização ≤ 1 sample — TC-020
- ✅ AC-M04-02 — Clipping → repetição → aborto após limite — TC-021
- ✅ AC-M04-03 — Sinal baixo → aviso não bloqueante — TC-022
- ✅ AC-M04-04 — Captura parcial não processada — TC-023
- ✅ AC-M04-05 — Averaging coerente — TC-024

## M-08 Orchestrator  (Bloco C — loopback + mocks)
- ✅ AC-M08-01 — Fluxo 100% automático — TC-060
- ✅ AC-M08-02 — Confirmação de descarte — TC-061
- ✅ AC-M08-03 — Cancelamento seguro — TC-062
- ✅ AC-M08-04 — Degradação graciosa — TC-063

## M-01 Hardware Manager  (Bloco E — lógica headless verificada; OS real na máquina-alvo)
- 🟡 AC-M01-01 — Só dispositivos conectados; hot-plug: **enumeração verificada (mock)** TC-001; hot-plug (TC-002) E2E
- ✅ AC-M01-02 — Configuração inválida rejeitada (ER-003) — TC-003
- ✅ AC-M01-03 — Fallback de nome de canal — TC-004
- ⏳ AC-M01-04 — Desconexão tratada (TC-005) — E2E (hardware real)

## M-02 Routing Manager  (Bloco E — lógica headless verificada)
- ✅ AC-M02-01 — Roteamento mono/estéreo por contagem de canais — verificado (resolveRouting)
- ✅ AC-M02-02 — Verificação bloqueia início sem sinal (ER-022) — verificado (verifyRouting)
- ✅ AC-M02-03 — Canal parcialmente funcional detectado (ER-025) — verificado (detectChannelFaults)
  Backends reais ASIO/WASAPI/CoreAudio: `JuceDeviceBackend.h` (máquina-alvo).

## M-06 Dashboard  (Bloco F — máquina-alvo, display/GUI)
- ⏳ AC-M06-01..03 — render de 6 seções, interações, estados
- 🟡 AC-M06-04 — Progresso sem detalhes técnicos: **modelo verificado** (TC-044 parte unit);
  render na máquina-alvo

## M-07 Export Engine  (Bloco G — CSV/WAV headless; PDF/PNG render na máquina-alvo)
- ✅ AC-M07-01 — CSV conforme DOC-10 (UTF-8, ponto decimal, N/A, sufixo `*`) — TC-050, TC-051
- ✅ AC-M07-02 — WAV IR 32-bit float, SR da medição — TC-052
- 🟡 AC-M07-03 — PDF com 6 seções: **modelo verificado** (TC-053, `IPdfWriter`); render na máquina-alvo
- ⏳ AC-M07-04 — PNG ≥1920×1080 com marcadores — render (máquina-alvo, `IPngWriter`)
