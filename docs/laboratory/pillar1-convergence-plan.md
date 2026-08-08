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
| ~~**P1-002**~~ ✅ | MS-08 | N4 / D4 (princípio) | validação | Baixo | ~~Só `Normal`/`Error`; silêncio vazio/curto → piso degenerado reportado como `Normal`~~ | **CONCLUÍDO** — `NotApplicable` para não caracterizável, `LowConfidence` para observação insuficiente, `Error` preservado. Silêncio **"sujo"** (DV-4) **excluído do escopo → Pilar 2** (exige limiar) | P1-001 ✅ |
| ~~**P1-003**~~ ✅ | MS-09 | **D2** | algoritmo | Moderado | ~~Teto = RMS **banda larga no tempo** (inclui DC/fora de banda) — Apêndice C, MS09-A2~~ | **CONCLUÍDO** — teto = nível RMS **in-band 20–20 k**, pelo **mesmo estimador do MS-08** (Hann, N=4096, ENBW/Parseval), quadro centrado no degrau. **Validado independentemente no Windows** | AR-01 ✅, P1-001 ✅, **F9** ✅ |
| ~~**P1-004**~~ ✅ *(núcleo)* | MS-09 | **D3** | validação / trat. de erro | Baixo | ~~`teto ≤ piso`: DR **negativo** reportado como `Normal`; só "sem degrau válido" → `NotApplicable` — Apêndice C, MS09-A4~~ | **NÚCLEO SEMÂNTICO CONCLUÍDO** — `teto ≤ piso` → **não caracterizável**; estados do piso propagados ao DR; sentinela `−300` deixa de ser lida como medição. **Validado independentemente no Windows.** A **representação do teto censurado (decisão D) foi desmembrada e permanece item aberto** | P1-003 ✅ |
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

**Noise Floor (MS-08)** — P1-002 — ✅ **ATENDIDOS NO ESCOPO AUTORIZADO**
- ✅ Valor = nível **RMS in-band 20–20 k**, energeticamente correto (consome P1-001, intocado)
- ✅ Estados de qualidade para silêncio degenerado **vazio / curto / não-mensurável**
- 🔴 Silêncio **"sujo"** — **excluído do escopo** por exigir limiar (**N4**: limiares → Pilar 2) → **DV-4**
- ✅ Conforme a `noise-floor-definition.md` §7; ✅ testes passam (Linux 481 · Windows aprovada)
- ⚠️ C++ ↔ JS consistentes **nas condições degeneradas**; divergente no caso ST-09 ausente → **DV-6**
- ✅ **Regressão permanente:** TC-039, com controle negativo (7 falhas)
- ✅ **Gate visual** no WebView nativo com *fixture* sintético mínimo (caso C4)

**Dynamic Range (MS-09)** — P1-003 ✅ · P1-004 ✅ *(núcleo semântico)*
- ✅ **P1-003:** teto na **banda 20–20 k**, pelo **mesmo estimador do MS-08** (Hann, N=4096,
  ENBW/Parseval), quadro de 4096 **centrado** no degrau de 4800; F9 aplicado antes da segmentação;
  gate de clipping preservado no sinal **temporal bruto**
- ✅ DC, subsônicos e ultrassônicos **excluídos do teto** (§5) — verificado com controle negativo
- ✅ **Regressão permanente:** TC-043; ✅ C++ ↔ JS convergentes; ✅ MS-08 bit-idêntico após a
  extração de `officialBandPower()`; ✅ MS-13 inalterado
- ✅ CTest completo: `aamp_headless_suite` 517 · `aamp_scientific_validation` 8 · 2/2
- ✅ **P1-004 (núcleo semântico):** `teto ≤ piso` → **não caracterizável**, sem valor; piso
  `NotApplicable`/sem valor → DR `NotApplicable`; piso `LowConfidence` → DR `LowConfidence` com o
  valor **presente**; nenhum DR negativo apresentado como medição; sem `abs()`, `clamp` ou zero
  artificial; medição normal **numericamente inalterada** (`≈86,789489 dB`)
- ✅ A sentinela `−300` do MS-08 **deixa de ser interpretada como medição** pelo MS-09 (**DV-5**),
  permanecendo no campo por decisão **D3**
- ✅ **Regressão permanente:** TC-046 (10 condições + 3 controles negativos); guarda Q4 do TC-039
  retirada por ser proteção temporária de escopo do P1-002
- ✅ C++ ↔ JS convergentes em valor, `quality` **e** `valuePresent`; `MetricResult`, enum `Quality`
  e exportação **inalterados**
- ✅ CTest completo: `aamp_headless_suite` 588 · `aamp_scientific_validation` 8 · 2/2
- 🔴 **Decisão D / teto censurado — NÃO RESOLVIDA.** Desmembrada por **fronteira arquitetural** em
  **item próprio e separado** (nenhum escalar novo criado)

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
| **P1-002** | ✅ **ENCERRADO** | Aprovado e concluído no escopo autorizado de **cálculo, contrato semântico e exportação** (2026-08-07). Registro completo em [`metrics-audit.md`](./metrics-audit.md) → *Registro de Encerramento — P1-002*. |
| **P1-003** | ✅ **CONCLUÍDO — validado independentemente no Windows** | Teto restrito à banda 20–20 k pelo mesmo estimador do MS-08 (`ffa739c`). Registro completo em [`metrics-audit.md`](./metrics-audit.md) → *Registro de Encerramento — P1-003*. |
| **P1-004** | ✅ **NÚCLEO SEMÂNTICO CONCLUÍDO — validado independentemente no Windows** | Estados do piso propagados ao MS-09; `teto ≤ piso` → não caracterizável; **DV-5 fechada** (`6ed71e8`). **Qualificador obrigatório:** encerra **apenas o núcleo semântico**. Registro completo em [`metrics-audit.md`](./metrics-audit.md) → *Registro de Encerramento — P1-004*. |
| **Teto censurado (decisão D)** | 🔴 **ABERTO — item próprio, não iniciado** | Desmembrado do P1-004 por **fronteira arquitetural** demonstrada. Ver §K.6 do `metrics-audit.md`. |
| P1-005 … P1-008 | ⬜ Não iniciados | — |

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

### P1-002 — síntese do encerramento

- **Escopo entregue:** cálculo, contrato semântico e exportação. `r.quality` deixa de ser
  `Normal` incondicional; `NoiseFloor_frames` passa a ser o descritor auditável da condição.
- **Sem limiar novo:** o limite de observação insuficiente reutiliza `k::NOISE_AVG_FRAMES = 16`,
  já dimensionado pelo projeto. Limiares numéricos permanecem no **Pilar 2** (N4/N5/D4).
- **Commit:** `18f64abb3db6dcb9505e4ee2aae65075ef5b3c38`.
- **Validação:** Linux 481 checks · Windows aprovada · TC-039 com controle negativo (7 falhas) ·
  convergência C++ ↔ JS nas 5 condições · exportação conferida em CSV real (`N/A`, sem `−300`).
- **Gate visual nativo:** *fixture* sintético mínimo `p1002_c4_digital_silence.aamp`
  (SHA-256 `c97e1be1…3954`), não versionado, exercitando o caso C4 sem nova aquisição.
- **Sentinela `−300` conservada no campo numérico** para que o **MS-09 permaneça inalterado**;
  neutralizada na exportação por `valuePresent = false`. **Só poderá ser removida em
  P1-003/P1-004**, quando o MS-09 passar a honrar `nf.quality`.

### Achados abertos que condicionam a execução

| ID | Achado | Bloqueia | Não bloqueia |
|---|---|---|---|
| ~~**F2**~~ ✅ | `Dynamic Range < SNR` — **RESOLVIDO**: relação é propriedade condicional (B); teto pode estar censurado (D) | — (decidido) | — |
| ~~**F9**~~ ✅ | Segmentação temporal do ST-08 — **RESOLVIDO** pela alternativa **A2** (`cfb95fa`), como **item próprio e pré-requisito** do P1-003. **Validado independentemente no Windows — APROVADO** | — (pré-requisito cumprido) | — |
| **F3** | Interface declara métricas "não aplicáveis" e as exibe | — (tratar **durante P1-008** / Pilar 2) | — |
| **F8** | Contagem de checks divergente entre plataformas (469 × 461) | — (higiene de evidência) | — |
| **F10** | **NaN atravessa MS-08 e MS-09 como `Normal`** — uma captura com `NaN` chega ao contrato rotulada como medição válida | — (registrar, **não corrigir** — decisão **D4** do P1-004) | — |
| **F11** | **Comentário residual** em `AnalysisEngine.cpp:790-792` afirmando que a sentinela existe "para que o MS-09 mantenha comportamento idêntico" e que honrar `nf.quality` "pertence a P1-003/P1-004" — **inconsistência textual sem efeito funcional** | — (corrigi-lo modificaria código já validado) | — |
| **Decisão D** | **Representação do teto censurado** — desmembrada do P1-004 por fronteira arquitetural; **item próprio, não iniciado** | — (**item próprio**; ver `metrics-audit.md` §K.6) | — |
| **DV-4** | Silêncio "sujo" não detectado (exige limiar) | — (**Pilar 2**) | — |
| ~~**DV-5**~~ ✅ | Sentinela `−300` lida como medição pelo MS-09 — **RESOLVIDA no P1-004** (sentinela conservada no campo por **D3**; nenhum consumidor normativo a interpreta) | — (fechada) | — |
| **DV-6** | CC-A: ST-09 ausente — C++ `Error` × JS omite a família | — | — |
| **DV-8** | Apresentação ignora `valuePresent`/`quality` e exibe a sentinela `−300` | — (**Pilar 2**) | — |
| **DV-9** | **Assimetria legítima da fonte ST-08** — Core **regenera** o estímulo (ramo inalcançável) × reprocessamento lê do pacote (pode faltar). **Preservada por decisão D5**, documentada nos dois lados | — (legítima; **não** corrigir) | — |
| **V1** | Trust Ladder rotula "Medição confiável" com única métrica `NotApplicable`. **Impacto novo, não avaliado:** a propagação de `LowConfidence` do MS-08 alcança agora o contrato do MS-09 | — (**P1-008**) | — |

**F1** e **F5** foram encerrados. Detalhamento de todos em [`metrics-audit.md`](./metrics-audit.md) →
*Registro de Encerramento — P1-001* → §E.10.

> **F2 está decidido** (Registro de Decisão — F2: **B** + **D**), e com ele caiu o bloqueio que pesava
> sobre P1-003 e P1-005. Em seu lugar surgiu **F9** — a segmentação temporal do ST-08 —, cujo estudo de
> impacto está concluído em
> [`temporal-segmentation-impact-study.md`](./temporal-segmentation-impact-study.md).
>
> **F9 foi decidido e encerrado:** adotada a alternativa **A2** (alinhamento local por
> `extractEnvelope`/`bestLag`, mesma convenção do MS-13), tratado como **item próprio e
> pré-requisito** do P1-003 — **não** incorporado a ele. Implementado em `cfb95fa`, protegido por
> **TC-041** e **validado independentemente no Windows (APROVADO)**. Registro completo em
> [`metrics-audit.md`](./metrics-audit.md) → *Registro de Encerramento — F9*.
>
> **Permanece em aberto:** se a decisão **D** (teto censurado) passa a ser sinalizada, o que
> alcançaria **P1-004** e a exportação. A ordem do Plano Mestre permanece **inalterada**.
>
> **Atualização (P1-004, 2026-08-08).** A questão foi levada até a fronteira e **desmembrada**: o
> **núcleo semântico** do P1-004 está concluído e validado, enquanto a **representação do teto
> censurado** passou a ser **item próprio, aberto e não iniciado**, por ter-se demonstrado que
> qualquer escalar novo de `MetricResult` é **exportado automaticamente** e que `MetricResult` **não
> possui superfície para metadados internos**. Sua solução exigirá decisão explícita entre evolução
> do contrato/exportação, extensão estrutural de `MetricResult` ou superfície própria de metadados —
> **sem** criar novo valor no enum `Quality` e **sem** recomputar alinhamento, segmentação ou
> espectro apenas para redescobrir a censura.
>
> DV-4, DV-8 e V1 são de **apresentação/limiar** e não bloqueiam a execução — serão tratados no
> Pilar 2 e no P1-008.

---

### P1-003 — síntese do encerramento

- **Escopo entregue:** banda do teto. Teto = nível RMS **in-band 20 Hz–20 kHz**, pelo **mesmo
  estimador espectral do MS-08**; quadro de 4096 centrado no degrau; F9 preservado antes da
  segmentação; gate de clipping mantido no sinal temporal bruto.
- **Em cadeia limpa o valor não muda** — a correção só atua onde há energia fora da banda.
- **Commit:** `ffa739c04d20d9c4aeccab45cc97556e0500da29` · **regressão:** TC-043 com controle negativo
  independente · **validação independente Windows: APROVADO** (SHA-256 `DFE1E17F…EFD0`; CTest 2/2).
- **Preservados sem alteração:** decisão **D** (teto censurado) **não implementada**;
  **DV-5 / sentinela `−300`** aberta; MS-10, MS-13, ST-08, `ENABLE_SYNC_ALIGNMENT`, Noise Floor,
  exportação, Evidence Package e apresentação intactos.

### P1-004 — síntese do encerramento *(núcleo semântico)*

> **Qualificador obrigatório:** o P1-004 só pode ser denominado "encerrado" com a ressalva explícita
> de que se trata do **núcleo semântico**. A **representação de teto censurado permanece item aberto
> e separado**.

- **Escopo entregue:** tratamento das condições **não caracterizáveis** do Dynamic Range. O MS-09
  passou a **honrar o estado do piso** (`quality`/`valuePresent`) em vez de ler apenas o número, e a
  degeneração `teto ≤ piso` passou a ser sinalizada como não caracterizável — nunca valor positivo
  espúrio, nunca número negativo, sem `abs()`, `clamp` ou zero artificial.
- **Em cadeia limpa o valor não muda** — `DynamicRange_dB = 86,789489 dB`, idêntico ao anterior.
- **Sentinela `−300` conservada no campo do MS-08** (decisão **D3**); o que mudou é que nenhum
  consumidor normativo a interpreta como medição — **DV-5 fechada**.
- **Commit:** `6ed71e80cc3e5b6859ce60e960f9f769569a1f43` · **regressão:** TC-046 (10 condições +
  3 controles negativos independentes) · **validação independente Windows: APROVADO**
  (`AAMP.exe` 4.659.712 bytes; SHA-256 `52EEE03C…4079`; `Release`, `GitDirty=false`;
  588 + 8 checks; CTest 2/2).
- **Preservados sem alteração:** matemática do **MS-08**, `MetricResult`, enum `Quality`,
  exportação, Evidence Package, estímulos, MS-10, MS-13 e apresentação.
- **Fronteira arquitetural demonstrada:** `ExportEngine::exportScalarsCSV` exporta **automaticamente**
  qualquer escalar novo e `MetricResult` **não tem superfície de metadados internos** ⇒ a decisão
  **D** foi **desmembrada, não abandonada nem implicitamente implementada**.

### Rastreabilidade histórica dos Evidence Packages

Pacotes `.aamp` anteriores **continuam válidos como evidência bruta** da captura original — nenhum
item do Pilar 1 alterou o áudio preservado, e **o formato do Evidence Package não foi alterado**.
Mas o **reprocessamento por versões posteriores do AAMP pode produzir `MS-09` diferente do relatório
originalmente calculado/exportado**, por causas cumulativas: as correções **científicas** do teto
(**banda**, em P1-003; **alinhamento**, em **F9**) e as correções **semânticas** do **P1-004** — que
podem converter um número outrora reportado como `Normal` em `NotApplicable`, ou rebaixá-lo a
`LowConfidence`. O `MS-08` já podia divergir por **P1-001** (ENBW).
**Todo resultado histórico deve ser interpretado junto da versão/commit que o produziu**, o que a
infraestrutura de diagnóstico (`CommitFull`, `BuildConfig`, `GitDirty`) torna rastreável.

---

## Disciplina de validação dos gates do Pilar 1

Todo gate de validação do Pilar 1 deve executar **a suíte CTest completa**, não apenas o binário
`aamp_tests`, e **reportar separadamente** as duas entradas hoje registradas:

| Entrada CTest | Observação |
|---|---|
| `aamp_headless_suite` | suíte principal (`aamp_tests`); é a que produz a contagem de *checks* |
| `aamp_scientific_validation` | **não é executada** ao rodar `aamp_tests` diretamente |

*Motivação registrada:* durante os ciclos de P1-001, P1-002 e F9 os relatórios de suíte foram
produzidos executando `aamp_tests` diretamente, de modo que `aamp_scientific_validation` **não era
exercitada** — lacuna revelada pelo gate de validação independente de F9, que exigiu `ctest`. A
partir daqui, "a suíte passou" só é afirmação completa quando **ambas** as entradas são reportadas.

---

## Encerramento do plano

Este é o **Plano Mestre do Pilar 1**: descreve **tudo que ainda diverge** da norma (P1-001…P1-008 +
CC-A/CC-B), **em que ordem** corrigir, **quais dependências** existem e **quais critérios** encerram cada
etapa. Um item aberto permanece explicitamente rastreado para ser fechado **durante a execução**: o estado
de qualidade para `pn≈0` (P1-007 / D4).

**Nenhuma implementação foi iniciada.** A execução controlada — **um item por vez, validado e rastreado
antes de prosseguir** — será objeto da próxima etapa.
