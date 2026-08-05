# Pilar 1 — Plano Mestre de Convergência da Implementação

> **Objetivo:** tornar a implementação **100% conforme** às especificações normativas **aprovadas**.
> Não adiciona funcionalidades. Toda alteração futura deverá ser **rastreável** a uma decisão registrada.
> **Este documento é apenas o plano** — nenhum código foi modificado, nenhuma implementação foi iniciada.
>
> **Escopo (metricas normativamente encerradas):** **MS-08** (Noise Floor), **MS-09** (Dynamic Range) e
> **MS-10** (SNR). MS-10 é incluído por ser especificação **aprovada** (Registro de Decisão D1–D5) e por
> **AR-01** sequenciá-lo com o MS-08; seu item `pn≈0` (D4) foi explicitamente diferido ao Pilar 1.
>
> **Fora de escopo deste plano** (não são divergências de especificação aprovada): V3 (semântica da
> Consistência) e V4 (Reproducibility) — pertencem a métricas ainda **não** revisadas; V5 (faixas
> normativas) e MS-10/D5 (apresentação) — **Pilar 2**; V7 (escalares DBG de MS-04/05/06/07) — métricas não
> encerradas.

---

## Etapa 1 — Inventário de Convergência

| ID | Origem | Ref. normativa | Tipo | Risco | Situação atual | Situação esperada | Dependências |
|----|--------|----------------|------|:----:|----------------|-------------------|--------------|
| ~~**P1-001**~~ ✅ | MS-08 | N1-R1 / **AR-01** | algoritmo | Moderado | ~~Normalização por **ganho coerente** (calibrada para tom); ruído de banda larga **enviesado** por offset ~constante da janela (Apêndice B)~~ | **CONCLUÍDO** — normalização energeticamente correta para ruído (ENBW/Parseval) implementada em C++ e JS, preservando a grandeza normativa do Noise Floor | **AR-01** → **SATISFEITA** |
| **P1-002** | MS-08 | N4 / D4 (princípio) | validação | Baixo | Só `Normal`/`Error`; silêncio vazio/curto → piso degenerado reportado como `Normal`; silêncio "sujo" não sinalizado | Estados de qualidade para silêncio degenerado (não caracterizável / baixa confiança) | P1-001 |
| **P1-003** | MS-09 | **D2** | algoritmo | Moderado | Teto = RMS **banda larga no tempo** (inclui DC/fora de banda) — Apêndice C, MS09-A2 | Teto medido na **mesma banda** do Noise Floor (20–20 k) | AR-01, P1-001 |
| **P1-004** | MS-09 | **D3** | validação / trat. de erro | Baixo | `teto ≤ piso`: DR **negativo** reportado como `Normal`; só "sem degrau válido" → `NotApplicable` — Apêndice C, MS09-A4 | `teto ≤ piso` → **não caracterizável** (nunca valor positivo espúrio nem negativo) | P1-003 |
| **P1-005** | MS-10 | **D1** | algoritmo | Moderado | `pn` do SNR = `meanSquare` no tempo (banda larga), separado do MS-08 — Apêndice B.5 | `pn` = **Noise Floor band-limited** (reúso da definição MS-08); checagem fora-de-banda fica na Integridade | AR-01, P1-001 |
| **P1-006** | MS-10 | **D2/V8** | algoritmo | Moderado | Numerador = bloco de 1 kHz **inteiro** (S+D+N) | **S/N estrito** (energia do tom isolada) | Coordenar com **TC-033** |
| **P1-007** | MS-10 | **D4** | validação / arquitetura | Moderado | SNR sempre `Normal`/`Error`; saturação `pn≈0` (200 dB) não rebaixada; SNR baixo não reduz confiança | Propagação de estados de qualidade; SNR insuficiente reduz confiança. **Item aberto (D4):** definir o estado para `pn≈0` **durante a execução** | P1-005; **decisão aberta `pn≈0`** |
| **P1-008** | MS-10 | **V1** (via D4) | interface / validação | Moderado | Trust ladder só testa teto (clipping/pico) + `lowConf`; captura silenciosa lê "Confiável" | **Porta inferior**: sinal que não se eleva acima do piso (via SNR) **nunca** lê "Confiável" | P1-005, P1-006, P1-007 |

**Restrições transversais (aplicáveis a todos os itens):**
- **CC-A (V6):** cada correção deve ser aplicada **consistentemente nas duas implementações** — C++
  `AnalysisEngine` e JS `LabEngine` (`aamp_laboratory.html`).
- **CC-B (V2):** a propagação de qualidade deve **atravessar o caminho da evidência** do LabEngine, que
  hoje fixa `quality:'Normal'`.

**Conformidades já verificadas (sem ação):** MS-09/D1 — critério de teto (saturação) já conforme
(Apêndice C, MS09-A1); MS-10/D3 — referência output-referred mantida (sem mudança de código).

---

## Etapa 2 — Agrupamento por componente

- **Normalização Espectral (FFT / ENBW)** — P1-001
- **Noise Floor (MS-08)** — P1-002 *(consome a normalização)*
- **Dynamic Range (MS-09)** — P1-003, P1-004
- **SNR (MS-10)** — P1-005, P1-006, P1-007
- **Trust Ladder** — P1-008
- **Export** — verificação de conformidade (Noise Floor dBFS + banda/condição; DR dB + `NotApplicable`;
  SNR) — *congelamento do formato é **Pilar 3***

---

## Etapa 3 — Grafo de dependências

```
                         AR-01
                           │
                           ▼
        Normalização Espectral (FFT/ENBW)   ── P1-001
                           │
                           ▼
              Noise Floor (MS-08)           ── P1-002
                           │
             ┌─────────────┴─────────────┐
             ▼                           ▼
   Dynamic Range (MS-09)          SNR (MS-10)
   P1-003, P1-004                 P1-005, P1-006, P1-007
                                         │
                                         ▼
                                  Trust Ladder ── P1-008
             └─────────────┬─────────────┘
                           ▼
                    Export (verificação)   →  formato congelado no Pilar 3

  Transversais a todos: CC-A (C++ ↔ JS) · CC-B (propagação de qualidade)
```

---

## Etapa 4 — Ordem ótima (lógica; sem cronograma)

```
1  Normalização Espectral (FFT / ENBW)      [P1-001]
        ↓
2  Noise Floor (MS-08)                       [P1-002]
        ↓
3  Dynamic Range (MS-09)                      [P1-003, P1-004]     ┐ paralelizáveis
        ↓                                                          │ após o Noise Floor
4  SNR (MS-10)                                [P1-005..P1-007]     ┘
        ↓
5  Trust Ladder                               [P1-008]
        ↓
6  Export (verificação de conformidade)
```

**Justificativa da ordem (apenas dependências técnicas):** AR-01 exige normalização validada **antes** de
qualquer reúso espectral de banda larga; Noise Floor consome essa normalização; Dynamic Range e SNR
consomem o Noise Floor (paralelizáveis entre si); Trust Ladder consome o SNR; Export verifica todas.

---

## Etapa 5 — Critérios de aceite por grupo

**Normalização Espectral (FFT / ENBW)** — P1-001 — ✅ **TODOS ATENDIDOS**
- ✅ Estimativa **energeticamente correta para ruído** demonstrada (equivalência ENBW/Parseval) — erro
  sintético 0,0037 dB (tolerância 0,1 dB), invariante em 5 décadas de nível
- ✅ **Sem regressão** para tons — `magSpectrum`/`fullSpectrum`/`peakBin`/`thdPercent` byte-idênticos;
  TC-031 e TC-032 preservados
- ✅ **AR-01 satisfeita** (precondição validada e registrada em `metrics-audit.md` → AR-01)
- ✅ Aplicada em **C++ e JS** (CC-A), confirmado também sobre dado físico real
- ✅ Documentação consistente; ✅ testes passam (Linux 461 · Windows 469, 0 falhas)
- ✅ **Regressão permanente:** TC-038, com controle negativo executado
- ✅ **Validação física** e **A/B por reprocessamento offline** (Δ = −4,772273 dB)

**Noise Floor (MS-08)** — P1-002
- ✓ Valor = nível **RMS in-band 20–20 k**, energeticamente correto (consome P1-001)
- ✓ Estados de qualidade para **silêncio degenerado** (vazio/curto/sujo)
- ✓ Conforme a `noise-floor-definition.md`; ✓ C++ ↔ JS consistentes; ✓ testes passam

**Dynamic Range (MS-09)** — P1-003, P1-004
- ✓ Teto na **banda 20–20 k** (mesma do Noise Floor)
- ✓ `teto ≤ piso` → **não caracterizável**
- ✓ Conforme a `dynamic-range-definition.md` (D1 já conforme; D2/D3 implementados)
- ✓ C++ ↔ JS consistentes; ✓ testes passam

**SNR (MS-10)** — P1-005, P1-006, P1-007
- ✓ `pn` = **Noise Floor band-limited** (D1)
- ✓ Numerador **S/N estrito** (D2/V8) com **TC-033** coerente
- ✓ Estados de qualidade propagados; **decisão aberta `pn≈0` fechada** durante a execução
- ✓ Conforme ao Registro de Decisão MS-10; ✓ C++ ↔ JS consistentes; ✓ testes passam

**Trust Ladder** — P1-008
- ✓ **Porta inferior** ativa: sinal que não se eleva acima do piso **nunca** lê "Confiável"
- ✓ Integra SNR/estados de qualidade (V1 fechado)
- ✓ Validação de integridade (casos silêncio/sem sinal) passa

**Export** — verificação
- ✓ Noise Floor exportado como **escalar dBFS + banda/condição** nos metadados
- ✓ Dynamic Range como **escalar dB + `NotApplicable`** na degeneração + metadados
- ✓ SNR conforme; ✓ (**congelamento do formato → Pilar 3**)

---

## Estado de execução

| Item | Estado | Observação |
|---|:--:|---|
| **P1-001** | ✅ **ENCERRADO** | Encerrado técnica, analítica, sintética e fisicamente (2026-08-05). Registro completo em [`metrics-audit.md`](./metrics-audit.md) → *Registro de Encerramento — P1-001*. |
| **P1-002** | ⬜ Não iniciado | Próximo na ordem. Aguarda autorização específica para a fase de análise. |
| P1-003 … P1-008 | ⬜ Não iniciados | — |

### P1-001 — síntese do encerramento

- **Implementação:** `windowPowerGain` / `windowEnbwBins` (nova abstração do **domínio de ruído**;
  nenhuma função de ENBW existia no código) consumidas por `noisePowerSpectrum()`, que reutiliza
  `magSpectrum()` **sem alterá-la**. Espelhado em `computeMS08()` do LabEngine (**CC-A**).
- **Commits:** `2bc5f88` (implementação) · `acca002` (TC-038) · `8fb6849` (infraestrutura de
  diagnóstico) · `9222ae0` (ferramenta de reprocessamento A/B, fora do produto).
- **Validação A/B — natureza do procedimento:** **não houve nova caracterização física nem nova
  aquisição da Helix.** O A/B foi feito por **reprocessamento offline** do
  `captures/ST09_Capture.wav` extraído do Evidence Package da caracterização já existente. Os dois
  binários processaram **o mesmo arquivo, byte a byte**; a **única variável alterada foi o algoritmo**.

  | Estado | Commit | `NoiseFloor_dBFS` |
  |---|---|---:|
  | Pré-correção | `692a49a` | −91,457365 |
  | Pós-correção | `9222ae0` | −96,229638 |
  | | **Δ** | **−4,772273 dB** = `−10·log10(2·ENBW_Hann)` |

- **Verificação independente:** RMS no domínio do tempo (sem FFT) reproduziu o resultado espectral com
  erro de **0,00016 dB**.

### Achados abertos que condicionam a execução

| ID | Achado | Bloqueia | Não bloqueia |
|---|---|---|---|
| **F2** | `Dynamic Range < SNR` contraria `dynamic-range-definition.md` §6 | **P1-003, P1-005** | **P1-002** |
| **F3** | Interface declara métricas "não aplicáveis" e as exibe | — (tratar **durante P1-008** / Pilar 2) | — |
| **F8** | Contagem de checks divergente entre plataformas (469 × 461) | — (higiene de evidência) | — |

**F1** e **F5** foram encerrados. Detalhamento de todos em [`metrics-audit.md`](./metrics-audit.md) →
*Registro de Encerramento — P1-001* → §E.10.

> **F2 exige decisão antes do P1-003.** Nenhum estudo conceitual foi iniciado. Como **F2 não bloqueia o
> P1-002**, a ordem do Plano Mestre permanece **inalterada**.

---

## Encerramento do plano

Este é o **Plano Mestre do Pilar 1**: descreve **tudo que ainda diverge** da norma (P1-001…P1-008 +
CC-A/CC-B), **em que ordem** corrigir, **quais dependências** existem e **quais critérios** encerram cada
etapa. Um item aberto permanece explicitamente rastreado para ser fechado **durante a execução**: o estado
de qualidade para `pn≈0` (P1-007 / D4).

**Nenhuma implementação foi iniciada.** A execução controlada — **um item por vez, validado e rastreado
antes de prosseguir** — será objeto da próxima etapa.
