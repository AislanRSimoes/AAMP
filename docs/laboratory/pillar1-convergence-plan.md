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
>
> *(**V3 e V4 foram tratados no Pilar 2**, em **AAMP-HG-20260811-001**, e **não** por revisão de suas
> métricas: V3 por **renomeação da apresentação** — a grandeza não mudou — e V4 por **correção do
> contrato** — o placeholder deixou de se publicar como medição, e **continua placeholder**. V5 e DV-4
> permanecem abertos, encaminhados ao **estudo normativo**, com nenhum limiar herdado ratificado.)*

---

## Etapa 1 — Inventário de Convergência

| ID | Origem | Ref. normativa | Tipo | Risco | Situação atual | Situação esperada | Dependências |
|----|--------|----------------|------|:----:|----------------|-------------------|--------------|
| ~~**P1-001**~~ ✅ | MS-08 | N1-R1 / **AR-01** | algoritmo | Moderado | ~~Normalização por **ganho coerente** (calibrada para tom); ruído de banda larga **enviesado** por offset ~constante da janela (Apêndice B)~~ | **CONCLUÍDO** — normalização energeticamente correta para ruído (ENBW/Parseval) implementada em C++ e JS, preservando a grandeza normativa do Noise Floor | **AR-01** → **SATISFEITA** |
| ~~**P1-002**~~ ✅ | MS-08 | N4 / D4 (princípio) | validação | Baixo | ~~Só `Normal`/`Error`; silêncio vazio/curto → piso degenerado reportado como `Normal`~~ | **CONCLUÍDO** — `NotApplicable` para não caracterizável, `LowConfidence` para observação insuficiente, `Error` preservado. Silêncio **"sujo"** (DV-4) **excluído do escopo → Pilar 2** (exige limiar) | P1-001 ✅ |
| ~~**P1-003**~~ ✅ | MS-09 | **D2** | algoritmo | Moderado | ~~Teto = RMS **banda larga no tempo** (inclui DC/fora de banda) — Apêndice C, MS09-A2~~ | **CONCLUÍDO** — teto = nível RMS **in-band 20–20 k**, pelo **mesmo estimador do MS-08** (Hann, N=4096, ENBW/Parseval), quadro centrado no degrau. **Validado independentemente no Windows** | AR-01 ✅, P1-001 ✅, **F9** ✅ |
| ~~**P1-004**~~ ✅ *(núcleo)* | MS-09 | **D3** | validação / trat. de erro | Baixo | ~~`teto ≤ piso`: DR **negativo** reportado como `Normal`; só "sem degrau válido" → `NotApplicable` — Apêndice C, MS09-A4~~ | **NÚCLEO SEMÂNTICO CONCLUÍDO** — `teto ≤ piso` → **não caracterizável**; estados do piso propagados ao DR; sentinela `−300` deixa de ser lida como medição. **Validado independentemente no Windows.** A **representação do teto censurado (decisão D) foi desmembrada e permanece item aberto** | P1-003 ✅ |
| ~~**P1-005**~~ ✅ | MS-10 | **D1** | algoritmo | Moderado | ~~`pn` do SNR = `meanSquare` no tempo (banda larga), separado do MS-08 — Apêndice B.5~~ | **CONCLUÍDO** — `pn = 10^(NoiseFloor_dBFS/10)`, o piso *band-limited* oficial do MS-08 como **fonte única de verdade**; sem segundo Welch. Propagação semântica completa. **Validado independentemente no Windows.** Checagem fora-de-banda na Integridade permanece fora do escopo | AR-01 ✅, P1-001 ✅ |
| ~~**P1-006**~~ ✅ | MS-10 | **D2/V8** | algoritmo | Moderado | ~~Numerador = bloco de 1 kHz **inteiro** (S+D+N)~~ | **CONCLUÍDO** — numerador = **potência da fundamental retornada** (quadro único centrado `N=4096`, Hann + ENBW/Parseval, `k0 = round(1000·N/fs)`, integração em `k0±2`); referência *output-referred* preservada; **sem alinhamento temporal**. **Validado independentemente no Windows.** `snrDb()` e **TC-033** intactos | P1-005 ✅ |
| ~~**P1-007**~~ ✅ *(reclassificado)* | MS-10 **+ MS-08** | **D4** | validação / arquitetura | Moderado | ~~**DV-10** — a janela `0 < integFull < 1e-30` satura o piso em `−300 dBFS` e o publica como `Normal`/`valuePresent=true`; MS-09 e MS-10 o consomem como medição válida (≈ 291 dB, `Point`)~~ | **CONCLUÍDO** — guarda do MS-08 passou de `integFull <= 0` para `integFull < PISO_MIN` (`1e-30`) em C++ e JS (`64163e3`); a janela resulta em `NotApplicable`/`valuePresent=false` e MS-09/MS-10 herdam pela propagação existente. `EstimateNature` **não** aplicada (D6); `snrDb()` intacta e coberta. **TC-056**. **V&V independente no Windows** | P1-005 ✅, P1-006 ✅ |
| ~~**P1-008**~~ ✅ | MS-10 | **V1** (via D4) | interface / validação | Moderado | ~~Trust ladder só testa teto (clipping/pico) + `lowConf`; captura silenciosa lê "Confiável"~~ | **CONCLUÍDO** — porta inferior em `classifyTrust()`: MS-10 oficial com `SNR_dB ≤ 0`, família `Error`/`NotApplicable`, escalar ausente/sem valor/não-finito → `bad`. **Sem margem** (limiares → Pilar 2); `MIN_SNR_OVER_FLOOR_DB` **não** importada. Mudança **aditiva**: `lowConf` agregado preservado como `warn`; MS-10 ausente preserva o comportamento anterior. **TC-057** (browser) | P1-005 ✅, P1-006 ✅, P1-007 ✅ |
| ~~**P1-009**~~ ✅ *(estágio 1)* | MS-09 *(transversal)* | **D** (teto censurado) | contrato / semântica | Moderado | ~~Uma estimativa **unilateral** é publicada como se fosse pontual: `DynamicRange_dB` afirma `DR = X` mesmo quando o ensaio só sustenta `DR ≥ X`~~ *(o MS-10, `pn≤0` → `200 dB`, permanece — P1-007)* | **ESTÁGIO 1 CONCLUÍDO** — natureza da estimativa tipada no nível de `Scalar` (`Point`/`LowerBound`/`UpperBound`), ortogonal a `Quality` e `valuePresent`, padrão `Point`. **Validado independentemente no Windows.** **Estágio 2 (fatos primários / exportação) permanece aberto** | **P1-004** ✅ |

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
- **Natureza da estimativa (contrato transversal)** — P1-009 ✅ *estágio 1* *(origem no MS-09; a ser consumido por MS-10/P1-007)*
- **SNR (MS-10)** — P1-005 ✅ · P1-006 ✅ · P1-007
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
   P1-003, P1-004                 P1-005 ✅ → P1-006 ✅
             │                           │
             ▼                           │
   Natureza da estimativa                │
   P1-009 (estágio 1) ✅ ────────────────┤
                                         ▼
                                     P1-007
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
4  SNR (MS-10)                                [P1-005 ✅ → P1-006 ✅]┘
        ↓
4b Natureza da estimativa                     [P1-009 estágio 1] ✅ — paralelo a 4 (CONCLUÍDO)
        ↓
4c SNR: estados de qualidade                  [P1-007]             — consome 4b
        ↓
5  Trust Ladder                               [P1-008]
        ↓
6  Export (verificação de conformidade)
```

**Justificativa da ordem (apenas dependências técnicas):** AR-01 exige normalização validada **antes** de
qualquer reúso espectral de banda larga; Noise Floor consome essa normalização; Dynamic Range e SNR
consomem o Noise Floor (paralelizáveis entre si); Trust Ladder consome o SNR; Export verifica todas.

**Inserção do P1-009 (2026-08-08).** O item nasce no MS-09 (decisão **D**) mas o contrato que cria é
**transversal**. Ele **não bloqueou P1-005 nem P1-006** — a dependência é unidirecional
(MS-09 → MS-08, MS-10 → MS-08) e **o MS-10 não lê o MS-09**. É, porém, **pré-requisito de P1-007**:
a decisão **D4** (estado para `pn≈0`) é o mesmo fenômeno de estimativa unilateral, e executá-la antes
da taxonomia produziria uma segunda solução incompatível.

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

**SNR (MS-10)** — P1-005 ✅ · P1-006 ✅ · P1-007
- ✅ **P1-005:** `pn` = **Noise Floor band-limited** oficial do MS-08 (D1), consumido como **fonte
  única de verdade** — `pn = 10^(NoiseFloor_dBFS/10)`, sem recomputar o ST-09 e sem segundo Welch
- ✅ Escala verificada: somado em toda a banda, o estimador do MS-08 reproduz `meanSquare` com erro
  de **0,0022 dB**; a única diferença sistemática é a banda (**−0,7998 dB** medido)
- ✅ Energia **fora da banda** deixa de contaminar o SNR (22 kHz: 34,00 → **86,79 dB**); ruído
  **in-band** continua degradando-o (1 kHz: **34,00 dB** em ambos os métodos)
- ✅ Estados propagados do piso: `Normal` / `LowConfidence` / `NotApplicable` / `Error`; a sentinela
  `−300` **nunca** reaparece como ≈ 291 dB
- ✅ **Regressão permanente:** TC-048, com **oráculo independente** do método antigo; ✅ C++ ↔ JS
  convergentes em valor, `quality` e `valuePresent` (`4825fcc` fechou a divergência de montagem)
- ✅ CTest completo: `aamp_headless_suite` 817 · `aamp_scientific_validation` 8 · 2/2
- ✅ **P1-006:** numerador = **potência da fundamental retornada** (D2/V8) — quadro único **centrado**
  `N=4096`, Hann + ENBW/Parseval, `k0 = round(1000·N/fs)` (= 85 em 48 kHz/4096 → bins 83–87),
  integração em `k0±2`; **`snrDb()` e TC-033 intactos**
- ✅ Referência **output-referred** honrada (D3): `ps` medido **exclusivamente da captura**; a fonte
  ST-03 **emitida** não entra no valor
- ✅ O `meanSquare` antigo incorporava **harmônicas, DC, energia ultrassônica e energia de clipping** —
  o "SNR" carregava THD+N (`ps_total/ps_fund = 1 + (THD+N/100)²`); hoje a distorção é exclusivamente de
  **MS-05/MS-06**
- ✅ **Sem alinhamento temporal**: invariância do **quadro centrado** verificada em offsets 0/913/2400
  (+ margem a 4800) — **condicionada** às premissas de §*P1-006 — síntese do encerramento*
- ✅ Viés determinístico de **−0,001220644 dB** (tom fora do centro do bin + integração ±2 bins)
  aceito como **propriedade do estimador**, não defeito pendente
- ✅ CC-A: **12 condições**, `SNR_dB`/`quality`/`valuePresent`/`EstimateNature` convergentes, maior
  `|Δ|` **0,0 dB**; `EstimateNature = Point`
- ✅ CTest completo: `aamp_headless_suite` **874** · `aamp_scientific_validation` **8** · 2/2
- ✅ **P1-007 — RATIFICADO (§P) e IMPLEMENTADO (`64163e3`).** Objeto **reclassificado**: não é `pn≈0` — provado
  **estruturalmente inalcançável** no caminho oficial (`NoiseFloor_dBFS ≥ −300` ⇒ `pn ≥ 1e-30 > 0`) —,
  e sim **DV-10**, a saturação numérica do piso publicada como medição pontual
- ✅ Política implementada: **`0 < integFull < 1e-30` → `NotApplicable`/`valuePresent=false`** no MS-08,
  herdado pelo MS-10 via propagação existente. **`EstimateNature` NÃO é aplicada** (falta `L`
  metrologicamente justificável); `200 dB` **não** vira `LowerBound`; `−300 dBFS` **não** é limite físico

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
| **P1-009 — estágio 1** | ✅ **CONCLUÍDO — validado independentemente no Windows** | Natureza tipada da estimativa em `Scalar`, ortogonal a `Quality` e `valuePresent` (`a175101`). **Qualificador obrigatório:** encerra **apenas o estágio 1**. **Pré-requisito de P1-007 — CUMPRIDO.** Registro completo em [`metrics-audit.md`](./metrics-audit.md) → *Registro de Encerramento — P1-009, Estágio 1*. |
| **P1-009 — estágio 2** | 🔴 **ABERTO — não iniciado** | Exposição/auditabilidade dos fatos primários (`DynamicRange_stepIndex`, `DynamicRange_firstClippedIndex`) e eventual integração com exportação/UI. Diferível ao Pilar 3. |
| **P1-005** | ✅ **CONCLUÍDO — validado independentemente no Windows** | `pn` do SNR passou a ser o Noise Floor *band-limited* oficial do MS-08, fonte única de verdade (`4825fcc`). Registro completo em [`metrics-audit.md`](./metrics-audit.md) → *Registro de Encerramento — P1-005*. |
| **P1-006** | ✅ **CONCLUÍDO — validado independentemente no Windows** | Numerador do SNR passou a ser a **potência da fundamental retornada** — `S/N` estrito (D2/V8) (`a702c0f`). **TC-033 não foi alterado**; `snrDb()` intacta. Registro completo em [`metrics-audit.md`](./metrics-audit.md) → *Registro de Encerramento — P1-006*. |
| **P1-007** | ✅ **CONCLUÍDO — validado independentemente no Windows** | Objeto **reclassificado** (§P): eliminar valores pontuais derivados da **saturação numérica** do estimador de Noise Floor (**DV-10**). Política **alternativa E** implementada em `64163e3`: guarda do MS-08 passa a `integFull < PISO_MIN`, em C++ e no LabEngine JS; MS-09/MS-10 herdam pelo contrato do operando, sem reconhecer a sentinela por valor. **`EstimateNature` não aplicada** (D6). `snrDb()` preservada e agora coberta. **TC-056**. Registro completo em [`metrics-audit.md`](./metrics-audit.md) → *Registro de Decisão — P1-007*. Os seis pontos do gate foram executados; no ponto (5), `snrDb()` foi **preservada** e seu domínio direto **coberto** por TC-056 (g). Eventual depreciação ou remoção permanece decisão futura de API, **fora do P1-007**. |
| **P1-008** | ✅ **CONCLUÍDO** | Porta inferior da Trust Ladder via MS-10 oficial (`SNR_dB ≤ 0` e gates de validade), em `classifyTrust()`. Nenhuma alteração científica: MS-08/09/10, exportação e Evidence Package intactos. **TC-057** é regressão **browser/WebView de máquina-alvo** — não há runner JS headless no projeto, limitação registrada. **F3 foi concluído no P2-002** e **DV-8 no P2-001** (ambos no Pilar 2). |

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
| ~~**F3**~~ ✅ | ~~Interface declara métricas "não aplicáveis" e as exibe~~ | ✅ **Concluído no P2-002** (Pilar 2) — apresentação normativa passa a exigir `hasAnalysis`; **TC-059** | — |
| **F8** | Contagem de checks divergente entre plataformas (469 × 461) | — (higiene de evidência) | — |
| **F10** | **NaN atravessa MS-08 e MS-09 como `Normal`** — uma captura com `NaN` chega ao contrato rotulada como medição válida. *Manifestação adicional registrada no P1-007: com `nf` não-finito o **C++** publica `Normal`/`valuePresent=true` enquanto o **JS** retorna `Error` (guarda `!isFinite(nf)` em `computeMS10`) — divergência CC-A real, **distinta da DV-6**.* | — (registrar, **não corrigir** — decisão **D4** do P1-004; **D8** do P1-007) | — |
| **F11** | **Comentário residual** em `AnalysisEngine.cpp:790-792` afirmando que a sentinela existe "para que o MS-09 mantenha comportamento idêntico" e que honrar `nf.quality` "pertence a P1-003/P1-004" — **inconsistência textual sem efeito funcional** | — (corrigi-lo modificaria código já validado) | — |
| ~~**Decisão D**~~ ✅ *(estágio 1)* | **Representação do teto censurado** — **estágio 1 CONCLUÍDO e validado no Windows** (`a175101`); **estágio 2 aberto** (fatos primários / exportação) | — (pré-requisito de P1-007 **cumprido**) | P1-005, P1-006 |
| **DV-4** | Silêncio "sujo" não detectado (exige limiar) | — (**Pilar 2**) | — |
| ~~**DV-5**~~ ✅ | Sentinela `−300` lida como medição pelo MS-09 — **RESOLVIDA no P1-004** (sentinela conservada no campo por **D3**; nenhum consumidor normativo a interpreta) | — (fechada) | — |
| **DV-6** | CC-A: ST-09 ausente — C++ `Error` × JS omite a família. *Corrigido para o **MS-10** em `4825fcc` (P1-005); permanece aberto para o **MS-08**.* | — | — |
| **DV-10** | **Saturação numérica do piso publicada como medição pontual.** Com `0 < integFull < 1e-30`, o MS-08 publica o clamp `−300 dBFS` com `quality = Normal` e `valuePresent = true`; **MS-09** (DR ≈ 290,9897 dB) e **MS-10** (SNR ≈ 290,9885 dB, `Point`) o consomem como valor válido. Mesma classe da **DV-5**, por rota distinta: o P1-005 fechou a rota do **estado**, esta é a rota do **clamp**. Sobrevive ao *round-trip* float32 do Evidence Package; **coberta por TC-056** desde o P1-007. *Alcançável por entrada sintética/processada — nenhum ADC real produz piso in-band < −300 dBFS* | ✅ **Corrigida no P1-007** (`64163e3`) — guarda `integFull < PISO_MIN` em C++ e JS; **TC-056** cobre a janela | MS-08, MS-09, MS-10 |
| ~~**DV-8**~~ ✅ | ~~Apresentação ignora `valuePresent`/`quality` e exibe a sentinela `−300`~~ | ✅ **Concluído no P2-001** (Pilar 2) — leitura apresentacional unificada em `scalarForDisplay`; **TC-058** | — |
| **DV-9** | **Assimetria legítima da fonte ST-08** — Core **regenera** o estímulo (ramo inalcançável) × reprocessamento lê do pacote (pode faltar). **Preservada por decisão D5**, documentada nos dois lados | — (legítima; **não** corrigir) | — |
| ~~**V1**~~ ✅ | ~~Trust Ladder rotula "Medição confiável" com única métrica `NotApplicable`~~ | ✅ **Fechado no P1-008**: `NotApplicable`, `Error`, ausência de valor, não-finito e `SNR ≤ 0` no MS-10 impedem qualquer leitura confiável | — |

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
> **Atualização (P1-009, 2026-08-08) — arquitetura ratificada.** O item recebeu identificador
> próprio (**P1-009**) e a decisão foi tomada: **qualificador tipado no nível de `Scalar`**
> (`Point`/`LowerBound`/`UpperBound`, padrão `Point`), ortogonal a `Quality` e `valuePresent`.
> Nenhuma das três opções enunciadas acima foi adotada na forma em que estavam: a investigação
> **refinou a premissa** — um **membro** novo de `Scalar` não é exportado automaticamente, ao
> contrário de um **escalar** novo. **Implementação não iniciada.** Registro completo em
> [`metrics-audit.md`](./metrics-audit.md) → *Registro de Decisão — P1-009*.
>
> **Encerramento (P1-009 estágio 1, 2026-08-08).** Implementado e **validado independentemente no
> Windows** (`a175101`): a natureza da estimativa é hoje um campo tipado de `Scalar`, e o MS-09
> declara `LowerBound` sempre que a saturação não foi demonstrada imediatamente acima do maior
> degrau válido — o que, com o ST-08 atual, é o **regime normal**. O **estágio 2** (fatos primários
> e exportação) **permanece aberto**. **P1-005 e P1-006 estão liberados; P1-007 deve reutilizar a
> taxonomia, não criar outra.** *(P1-005 e P1-006 foram concluídos depois — ver as respectivas
> sínteses; P1-007 segue com a mesma obrigação de reúso.)*
>
> DV-4, DV-8 e V1 são de **apresentação/limiar** e não bloqueiam a execução. *(**V1 foi resolvido no
> P1-008**; **DV-8 foi concluída no P2-001**; DV-4 permanece no Pilar 2.)*

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

### P1-009 — síntese do encerramento *(estágio 1)*

> **Qualificador obrigatório:** o P1-009 só pode ser denominado "encerrado" com a ressalva explícita
> de que se trata do **estágio 1**. O **estágio 2 permanece aberto**.

- **Escopo entregue:** `Scalar` passa a possuir **natureza tipada da estimativa**, o **terceiro eixo**
  do contrato — `Quality` (validade/confiança), `valuePresent` (existência do valor) e
  `EstimateNature` (o que o valor afirma). Taxonomia `Point` · `LowerBound` · `UpperBound`, com
  **`Point` como padrão**, preservando integralmente o comportamento das demais métricas.
- **Combinações registradas como válidas e intencionais:** `Normal + valuePresent=true + LowerBound`
  (teto censurado é medição plenamente confiável) e `LowConfidence + valuePresent=true + LowerBound`
  (comprova a **independência dos três eixos**).
- **Regra do MS-09:** teto observado/encaixotado → `Point`; teto não atingido → `LowerBound`;
  saturação observada mas não encaixotada → `LowerBound`; `C = ∅` com `s* <` último degrau →
  `LowerBound`; sem valor caracterizável → `NotApplicable` / `valuePresent=false`.
- **Lacuna fechada:** o commit de teste `a1751011db5f83c5e2258374adee713642bf24f9` demonstra
  diretamente o quarto ramo (truncamento em `0..3`, `peak < 1.0` ⇒ `C = ∅`, `s* = 3` confirmado por
  Δ ≈ 16 dB, resultado `Normal + presente + LowerBound`), com controle discriminante em ganho ×2
  (`C ≠ ∅`, `min(C) = s*+1` ⇒ `Point`).
- **Commit validado:** `a1751011db5f83c5e2258374adee713642bf24f9` · **validação independente
  Windows: APROVADO** (`Release`, `dirty=false`, 0 warnings / 0 erros; **773 + 8 checks**; CTest 2/2;
  TC-030/034/038/039/041/043/046/047).
- **Não-regressões:** `DynamicRange_dB ≈ 86,789489300 dB` inalterado; CSV, `ReportModel` e
  `DashboardModel` **invariáveis**; Evidence Package inalterado; nenhum escalar público novo;
  `Quality` inalterado; **F10/NaN permanece aberto e não corrigido** — a natureza é derivada de
  índices, nunca de comparação de `double`.
- **Estágio 2 — ABERTO:** `DynamicRange_stepIndex` e `DynamicRange_firstClippedIndex` **não** foram
  implementados; exposição/auditabilidade e integração com exportação/UI ficam para etapa futura.

#### Ratificação arquitetural que precedeu a implementação

- **Arquitetura aprovada:** **D + A em dois estágios**.
  **Estágio 1 (D)** — qualificador **tipado no nível de `Scalar`**, ortogonal a `Quality` e
  `valuePresent`, taxonomia `Point` · `LowerBound` · `UpperBound`, **padrão `Point`**.
  **Estágio 2 (A)** — `DynamicRange_stepIndex` e `DynamicRange_firstClippedIndex` preservados como
  **proposta futura**, sem implementação e **sem autorização de exportação**; diferível ao Pilar 3.
- **Regra formal dos casos do MS-09** (`V` = degraus válidos, `C` = saturados, `s*` = selecionado):

  | Condição | Natureza |
  |---|:--:|
  | `C ≠ ∅` e `min(C) = s* + 1` — saturação demonstrada logo acima | **`Point`** |
  | `C = ∅` e `s* = último degrau` — censura pelo limite do ensaio | **`LowerBound`** |
  | `C ≠ ∅` e `min(C) > s* + 1` — saturação não encaixotada | **`LowerBound`** |
  | `V = ∅` — sem valor | *(irrelevante)* — `NotApplicable`, `valuePresent = false` |

- **O regime censurado é o NORMAL.** O ST-08 tem pico máximo de **−6 dBFS**: cadeias de ganho
  unitário ou inferior a +6 dB **nunca** demonstram saturação. `86,789489 dB` é, fisicamente,
  `DR ≥ 86,789489 dB`. Isto completa a explicação de **F2**.
- **Vedado:** criar `Quality::LowerBound`; usar `LowConfidence` para unilateralidade; transformar
  ausência de valor em `LowerBound`; derivar a natureza de comparações de `double` (F10).
- **Premissa refinada:** escalar novo é exportado automaticamente; **membro novo de `Scalar` não é**
  — provado por compilação, mantendo as 76 inicializações agregadas existentes.

### P1-005 — síntese do encerramento

- **Escopo entregue:** o denominador do SNR. O MS-10 deixou de usar `meanSquare(ST-09, 0..Nyquist)` e
  passou a consumir o **Noise Floor oficial do MS-08**, já limitado a 20 Hz–20 kHz:
  `pn = 10^(NoiseFloor_dBFS/10)`, `SNR_dB = 10·log10(ps/pn)`, com **`ps` inalterado**.
- **Fonte única de verdade:** o MS-10 **não recomputa o ST-09** e **nenhum segundo Welch foi criado**.
  A conversão é direta porque as escalas coincidem — o estimador do MS-08 somado em toda a banda
  reproduz `meanSquare` com erro de **0,0022 dB**, independente do nível.
- **Antes → depois:** ruído branco 85,9897 → **86,7895 dB**; +22 kHz 34,0000 → **86,7895 dB**;
  +5 Hz 34,0000 → **52,9384 dB**; +1 kHz *in-band* 34,0000 → **34,0000 dB** (inalterado, como deve).
- **O caso de 5 Hz é comportamento esperado do estimador oficial do MS-08** — vazamento de janela na
  borda da banda —, **não** divergência remanescente do P1-005.
- **Propagação semântica:** `Normal` / `LowConfidence` / `NotApplicable` / `Error` herdados do piso;
  a sentinela `−300` **nunca** reaparece como SNR artificial de ≈ 291 dB.
- **Commits:** `09a9edc` (implementação) · **`4825fcc`** (correção CC-A da montagem JS, com
  `computeMS10()` byte-idêntica) · **validação independente Windows: APROVADO** (`Release`,
  `GitDirty=false`, 0 warnings / 0 erros; **817 + 8 checks**; CTest 2/2;
  TC-030/033/034/038/039/041/043/046/047/048).
- **Fronteiras preservadas:** `snrDb()` inalterada e ainda coberta por **TC-033**; numerador com o
  **P1-006** *(concluído depois — ver a síntese seguinte)*; `EstimateNature` do MS-10 em **`Point`**;
  `pn ≤ 0 → 200` **não reinterpretado** —
  registrado como **estruturalmente inalcançável** para operando válido, sem caso artificial;
  limite inferior reservado ao **P1-007**; MS-08, MS-09, MS-13, ST-03, ST-09, exportação e Evidence
  Package intactos.

### P1-006 — síntese do encerramento

- **Escopo entregue:** o **numerador** do SNR. O MS-10 deixou de usar `meanSquare` do bloco de 1 kHz
  **inteiro** e passou a medir a **potência espectral absoluta da fundamental de 1 kHz retornada pelo
  DUT**, fechando **D2/V8**. Composição final: `ps` = fundamental retornada · `pn` = piso oficial do
  MS-08 (P1-005) · `SNR = 10·log10(ps/pn)`.
- **Estimador:** a **mesma máquina espectral absoluta** dos dois lados da razão — captura do **ST-03**,
  bloco nominal de 1 kHz, **quadro único centrado** de `N = 4096`, **Hann**, **ENBW/Parseval**,
  frequência nominal **fixa em 1000 Hz**, `k0 = round(1000·N/fs)` — em 48 kHz/4096, `k0 = 85`, bins
  **83, 84, 85, 86, 87** — integração em **`k0 ± 2`**. `inBandPower` ganhou limites parametrizáveis
  com os valores normativos como **padrão**, deixando o **MS-09 numericamente idêntico**.
- **Referência output-referred (D3) honrada:** `ps` é medido **exclusivamente da captura**; a fonte
  **ST-03 emitida não entra no valor**.
- **O que o método antigo incorporava indevidamente:** **harmônicas**, **DC**, **energia
  ultrassônica** e a **energia adicional produzida por clipping**. Vale exatamente
  `ps_total/ps_fund = 1 + (THD+N/100)²` — o "SNR" carregava informação de **THD+N** e **melhorava** num
  DUT mais distorcido, contrariando **D2/V8**. A distorção é responsabilidade de **MS-05/MS-06**, que
  **não** foram alteradas.
- **Resultados:** limpo **86,788268656 dB**; harmônicas **86,788268650**; DC **86,788268656**;
  22 kHz **86,788268656**; ruído −60 dBFS **86,787547890**; ganho ×1,5 **90,310093837** (= limpo +
  `20·log10(1,5)` exato); clipping ×3 **94,161086680**; offsets 913 e 2400 **86,788268656**.
- **Alinhamento temporal — NÃO introduzido.** Nenhum `bestLag`, nenhuma dependência da fonte emitida,
  nenhuma política nova para ST-03 ausente, **F9 não reutilizado**. A invariância vem do
  **enquadramento**: offsets 0/913/2400 (e margem a 4800) produzem o **mesmo** SNR estrito.
  **Justificativa condicionada às premissas atuais** — bloco de 1 kHz com **2 s**, quadro de **4096
  centralizado**, política de latência **≤ 50 ms**. **Se qualquer premissa mudar, a necessidade de
  alinhamento deve ser reavaliada**; este registro **não** generaliza para arquiteturas futuras.
- **Viés conhecido:** **≈ −0,001220644 dB** em relação ao valor analítico ideal do seno, por o tom de
  1 kHz não cair no centro de um bin (85,333) somado à integração ratificada em **±2 bins**.
  **Propriedade conhecida do estimador adotado, não defeito pendente**; a decisão de ±2 bins **não é
  reaberta** e a largura não foi ampliada.
- **Testes:** **TC-049** criado como regressão permanente (seno puro, harmônicas, ruído, DC, 22 kHz,
  ganho, clipping, offsets, ST-03 ausente, CC-A, controle negativo independente com o `meanSquare`
  antigo, não-regressão do denominador do P1-005). **TC-048 atualizado somente** nos quatro pontos
  que fixavam o numerador pré-P1-006, com todas as proteções do P1-005 preservadas. **TC-033
  intacto.**
- **CC-A:** **12 condições** sobre entradas bit-idênticas, executando o bloco real de montagem —
  `SNR_dB`, `quality`, `valuePresent` e `EstimateNature` convergentes, maior `|Δ|` **0,0 dB**,
  `EstimateNature = Point`; cobertos `LowConfidence`, `NotApplicable` e `Error`.
- **Commit:** **`a702c0f`** · **validação independente Windows: APROVADO** (`Release`,
  `GitDirty=false`, build `--clean-first`, 0 warnings / 0 erros; `AAMP.exe` **4.660.736 bytes**,
  SHA-256 `1488CD91…3243306`; **874 + 8 checks**; CTest 2/2;
  TC-030/033/034/038/039/041/043/046/047/048/049).
- **Não-regressões:** **MS-08 ≈ −95,799789257 dBFS** e **MS-09 ≈ 86,789489300 dB** inalterados.
  Intactos `snrDb()`, **TC-033**, MS-05, MS-06, MS-08, MS-09, MS-13, ST-03, ST-09, `EstimateNature`,
  `Quality`, exportação e Evidence Package.

### P1-007 — ratificação arquitetural *(seguida de implementação)*

> **Ratificação em `3459490`, implementação em `64163e3`.** Esta seção registra a política ratificada em
> 2026-08-10, quando nenhuma alteração de código a acompanhava. A implementação veio no commit seguinte
> e está registrada na *Síntese do encerramento — P1-007*, adiante. As decisões D1–D9 abaixo permanecem
> normativas e **não** foram alteradas pela implementação.

- **Escopo reclassificado.** O item deixa de ser *“decidir o comportamento `pn ≈ 0 → 200 dB`”* e passa
  a ser: **eliminar a apresentação de valores pontuais derivados da saturação numérica do estimador de
  Noise Floor e retirar do pipeline oficial a semântica artificial de `200 dB`**, preservando
  separadamente o contrato da primitiva pública `snrDb()`.
- **D1 — o ramo `pn ≤ 0 → 200` não é condição física do pipeline oficial.** Provado que
  `NoiseFloor_dBFS ≥ −300` ⇒ `pn = 10^(NoiseFloor_dBFS/10) ≥ 1e-30 > 0`, logo o ramo é
  **estruturalmente inalcançável** em `AnalysisEngine::snr()` e no equivalente JS. **Não autoriza**
  remover ou modificar `snrDb()`.
- **D2 — `200 dB` não é medição nem limite.** Vedado tratá-lo como estimativa pontual **ou** convertê-lo
  em `LowerBound`.
- **D3 — `−300 dBFS` não é limite físico de observabilidade.** Deriva de `max(integFull, 1e-30)`, guarda
  contra `log10(0)`. Não representa resolução do ADC, piso mínimo mensurável, limite metrológico nem
  limite de confiança. **Vedado** derivar `SNR ≥ Ps − (−300)` ou qualquer `LowerBound` dessa constante.
- **D4 — o defeito material é a janela `0 < integFull < 1e-30`** (**DV-10**).
- **D5 — política escolhida: alternativa E.** Nessa janela o MS-08 resulta em
  `NotApplicable`/`valuePresent = false`, herdado pelo MS-10 pela propagação já existente. Significa
  **apenas** *“o estimador atual não fornece medição quantitativa utilizável para essa entrada”*.
- **D6 — `UpperBound(MS-08) → LowerBound(MS-10)` é válido, mas não aplicável hoje.** Exige `L` de origem
  metrológica justificável; o Evidence Package **não preserva** informação suficiente para estabelecê-lo
  (`metadata.json` tem `clipThresholdDbfs`, `prngSeed`, pre/post-roll — **sem** profundidade de bits,
  piso especificado ou modelo de resolução). Não implementar `UpperBound` no MS-08 nem `LowerBound` no
  MS-10 agora; não usar `−300` como substituto de `L`.
- **D7 — P1-009 estágio 2 não será antecipado** pelo P1-007.
- **D8 — F10 permanece separado** (ver tabela de achados). **D9 — DV-6 permanece fora do escopo.**

**Distinção registrada:**

```
integFull == 0            → ausência de observação quantitativa
0 < integFull < 1e-30     → saturação numérica do estimador → NotApplicable (política ratificada)
integFull >= 1e-30        → valor quantitativo representável
```

`1e-30` é **fronteira computacional da implementação atual**, não especificação metrológica do AAMP.

**`snrDb()` — situação registrada, decisão adiada:** zero chamadores de produção; superfície pública de
`aamp_core`; ramo `pn ≤ 0 → 200` **alcançável por chamada direta**. **TC-033 continua não cobrindo esse
ramo**; a cobertura foi acrescentada por **TC-056 (g)**, que fixa `snrDb(sinal, zeros)`,
`snrDb(sinal, {})` e `snrDb(zeros, zeros)` em `200 dB`. A primitiva **não foi alterada**: seu destino —
preservar, depreciar ou remover — permanece **decisão de API em aberto**, fora do P1-007.

**Gate para implementação posterior** — a próxima autorização deverá definir separadamente: **(1)**
alteração do MS-08 para a janela; **(2)** propagação MS-08 → MS-10; **(3)** comportamento equivalente
C++/JS; **(4)** regressão permanente específica para a janela do clamp; **(5)** destino de `snrDb()` e
cobertura do seu domínio direto; **(6)** não-regressão de **MS-09**, que também consome o `−300`
saturado como valor válido.

### Rastreabilidade histórica dos Evidence Packages

Pacotes `.aamp` anteriores **continuam válidos como evidência bruta** da captura original — nenhum
item do Pilar 1 alterou o áudio preservado, e **o formato do Evidence Package não foi alterado**.
Mas o **reprocessamento por versões posteriores do AAMP pode produzir `MS-09` diferente do relatório
originalmente calculado/exportado**, por causas cumulativas: as correções **científicas** do teto
(**banda**, em P1-003; **alinhamento**, em **F9**) e as correções **semânticas** do **P1-004** — que
podem converter um número outrora reportado como `Normal` em `NotApplicable`, ou rebaixá-lo a
`LowConfidence`. O `MS-08` já podia divergir por **P1-001** (ENBW), e o **`MS-10`** passa a divergir
por **P1-005** (correção científica do **denominador**).
**Todo resultado histórico deve ser interpretado junto da versão/commit que o produziu**, o que a
infraestrutura de diagnóstico (`CommitFull`, `BuildConfig`, `GitDirty`) torna rastreável.

> **Atualização (P1-006).** O `MS-10` passa a divergir por **duas** causas cumulativas: o
> **denominador** (P1-005) e agora o **numerador** (P1-006). Um `.aamp` histórico reprocessado por
> versão posterior pode, portanto, produzir **SNR diferente** do originalmente exportado — em geral
> **menor**, por deixar de contar distorção, DC e energia fora de banda no numerador. O pacote
> **continua válido como evidência bruta** e o **formato não foi alterado**.

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
