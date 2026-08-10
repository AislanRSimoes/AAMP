# Estudo Arquitetural — Representação de Teto Censurado (MS-09)

> **Status:** ESTUDO PRÉ-DECISÃO **concluído**, **DECISÃO RATIFICADA** (§15) e **ESTÁGIO 1
> IMPLEMENTADO E ENCERRADO** (§16). O **estágio 2 permanece aberto**.
> **Escopo:** analisa *como* representar a natureza unilateral de uma estimativa de Dynamic Range
> quando o ensaio não demonstra saturação. Não altera a definição normativa da grandeza, que
> permanece em [`dynamic-range-definition.md`](./dynamic-range-definition.md).
> **Origem:** decisão **D** do MS-09, desmembrada do **P1-004** por fronteira arquitetural.
>
> **Preservação do texto pré-decisão.** As §§1–14 registram a investigação **como foi conduzida,
> antes da decisão**, e não devem ser reescritas retrospectivamente. A decisão normativa que dela
> resultou está isolada na **§15**, e o desfecho de implementação na **§16**. Onde a investigação corrigiu uma premissa anterior, a correção
> está registrada como tal — a premissa original é preservada, não apagada.
>
> **Nenhuma implementação acompanhou o estudo.** Nenhum código, teste, exportação, contrato,
> estímulo ou Evidence Package foi alterado em qualquer momento da investigação (§§1–15). A
> implementação do **estágio 1** veio depois, em item próprio, e está registrada na **§16**.

---

## Achado que reordena o problema

O ST-08 é uma escada de 8 degraus com pico máximo de **−6 dBFS**
(`StimulusGenerator.cpp:154`, níveis `{−40, −34, −28, −22, −16, −10, −8, −6}`). Uma cadeia de ganho
unitário nunca satura. O laço do MS-09 foi reproduzido **fora do produto**, em sonda descartável,
para observar os fatos primários:

```
loopback limpo (x1)    selecionado=7  algumClipou=NAO  validos=8/8  -> TETO CENSURADO
ganho x2               selecionado=6  algumClipou=sim  primeiroClipado=7  -> teto observado
ganho x4               selecionado=4  algumClipou=sim  primeiroClipado=5  -> teto observado
ganho x100             selecionado=-1 algumClipou=sim  primeiroClipado=0  -> nenhum degrau valido
```

**O teto censurado não é um caso de borda: é o regime dominante.** Toda cadeia com ganho inferior a
+6 dB — o que inclui o loopback de referência e qualquer DUT de ganho unitário — produz um teto
censurado. O valor `86,789489 dB` que o P1-004 preservou como não-regressão é, fisicamente,
`DR ≥ 86,789489 dB`.

Isso também fecha a explicação de **F2** (`DR = 80,1` < `SNR = 80,4`): é exatamente o regime que a
decisão D previu, e a inversão observada era o sintoma esperado de um teto censurado — **não** viés
de estimador.

O que hoje ocorre não é omissão de um detalhe raro; é a **afirmação de igualdade onde a física
sustenta apenas desigualdade**, na maioria das medições.

---

## 1. Definição formal

Sejam `L₀ < L₁ < … < L₇` os degraus do ST-08, `V ⊆ {0..7}` o conjunto dos que produziram estimativa
in-band válida, `C ⊆ {0..7}` o dos rejeitados por saturação (`peak ≥ 1.0`), e
`s* = argmax_{k∈V} nível(k)` o degrau selecionado.

**Teto observado.** `C ≠ ∅` **e** `min(C) = s* + 1`. A saturação foi demonstrada e está *encaixotada*
entre `s*` e `min(C)`: o início da saturação existe e foi localizado. Afirmação sustentada:
`Dynamic Range = teto − piso`.

**Teto censurado.** `C = ∅` **e** `s* = 7`. O ensaio terminou no maior nível disponível sem demonstrar
saturação. O número permanece válido; muda a **quantificação lógica**: sabe-se que a cadeia *suporta
ao menos* `L₇`, não que satura logo acima. Afirmação sustentada: `Dynamic Range ≥ X dB`.

**Terceiro caso, não previsto no enunciado do estudo.** `C ≠ ∅` **e** `min(C) > s* + 1` — existe
degrau saturado, mas não imediatamente acima do selecionado; há degraus intermediários inválidos por
outra causa (captura truncada, energia in-band nula). A saturação foi observada mas **não
encaixotada**: o início pode estar em qualquer ponto do intervalo. Um classificador binário o
rotularia incorretamente como observado. Registrar este caso é parte do resultado do estudo.

**Quarto caso, já resolvido.** `V = ∅` → `NotApplicable`, sem valor. Já implementado no núcleo do
P1-004; não pertence a esta taxonomia.

## 2. Fatos mínimos necessários

A classificação exige exatamente **dois números inteiros** e nada mais:

| Fato | Origem no laço atual | Custo |
|---|---|:--:|
| `s*` — índice do degrau selecionado | já é a variável do `argmax`; hoje descartada | zero |
| `min(C)` — índice do primeiro degrau saturado (`−1` se nenhum) | já é o ramo `if (peak(part) >= 1.0) continue;` | zero |

`anyStepClipped` é redundante (`min(C) ≥ 0`). `|V|` e o total de degraus são derivados ou constantes.
**Nenhum dado novo precisa ser calculado, e nada precisa ser recomputado depois.** As três condições
da §1 são funções apenas de `s*` e `min(C)`.

Regra de derivação — **crítica para F10**: a classificação é função de **inteiros e comparações de
índice**, jamais de comparações de ponto flutuante. Um `NaN` em `lmaxDb` torna toda comparação falsa
e faria um classificador numérico degradar silenciosamente para "observado"; um classificador por
índices é imune.

## 3. Fluxo real de `MetricResult`

```
AnalysisEngine::computeAllMetrics
        │  ResultBundle (em memória)
        ▼
MeasurementOrchestrator ──setResultCallback──► AampWebBridge (progresso; NÃO serializa escalares)
        │  RunResult.bundle
        ▼
AampWebBridge::m_lastBundle ──────┬─────────────────────────────────────────────┐
                                  ▼                                             ▼
                        ExportEngine (M-07)                        SessionSerializer
                     CSV escalares · CSV curvas                   serializeAampBytes()
                     PDF (ReportModel) · WAV · PNG                        │
                                                                   ✗ DESATIVADO
                                                          (kDeliverEvidence = true,
                                                           AampWebBridge.cpp:488)

  ┌──────────────────────── caminho vivo do produto ────────────────────────┐
  │ EvidenceRecorder ─► EvidencePackage ─► .aamp (só evidência bruta)       │
  │        ─► frontend ingest/fromAamp ─► LabEngine (JS) ─► computeMS09     │
  │        ─► contrato {byId, scalars} ─► instrumentos · Trust Ladder       │
  └─────────────────────────────────────────────────────────────────────────┘

DashboardModel / DashboardComponent ── construídos e testados, NÃO instanciados no app
```

**Consequência decisiva:** o `MetricResult` do C++ **não alcança o frontend**.
`kDeliverEvidence = true` faz o `.aamp` transportar apenas evidência bruta, e a ciência exibida é
**recomputada pelo LabEngine em JS**. O `MetricResult` alimenta hoje somente CSV/PDF e um *callback*
de progresso em memória.

Logo o problema tem **duas metades independentes**: a qualificação precisa existir no C++ (para
exportação) *e* no JS (para a interface e a Trust Ladder). Qualquer alternativa que resolva só uma
delas resolve metade do problema.

## 4. Superfícies atuais de transporte

| Superfície | Campos de `Scalar` consumidos | Novo **escalar** aparece? | Novo **campo** aparece? |
|---|---|:--:|:--:|
| `exportScalarsCSV` | `key`, `value`, `quality`, `valuePresent` | **sim, automático** | **não** |
| `exportCurvesCSV` | — (só curvas) | não | não |
| `buildReportModel` / PDF | busca por chave fixa | não | não |
| `DashboardModel` | `value`, `quality`, `valuePresent` | não *(itens fixos)* | não |
| `SessionSerializer` (JSON/.aamp legado) | lista explícita de 5 propriedades | sim | **não** — desativado de todo modo |
| `EvidencePackage` / `.aamp` vivo | **nenhum** — não carrega ciência derivada | não | não |
| `AampWebBridge` result callback | `MetricResult` inteiro, só em memória | irrelevante | irrelevante |
| LabEngine JS → `normalizeMetrics` | objeto livre; `scalars[key]` | sim | só se lido |

### 4.1 Correção de premissa — campos novos versus escalares novos

A fronteira arquitetural demonstrada durante o **P1-004** é **mais estreita** do que se registrou na
ocasião. O acoplamento automático com a exportação vale para **escalares novos**, não para **campos
novos**. `exportScalarsCSV` imprime `key=formatScalar(s)`; um membro adicional de `Scalar` é ignorado
até que `formatScalar` seja deliberadamente alterado.

*(A conclusão original do P1-004 — "não é possível preservar o descritor sem alterar a exportação ou
o contrato estrutural" — permanece **correta para a alternativa então considerada**, que era
acrescentar um **escalar**. Ela é aqui **refinada**, não invalidada: a investigação arquitetural
posterior identificou uma superfície que não havia sido examinada.)*

Prova de compatibilidade obtida por **compilação**, sobre uma cópia do cabeçalho **fora do
repositório** (nada do produto foi alterado):

```
compilou: 3 escalares, campo novo default=0
```

Um membro com inicializador padrão, declarado **por último** em `Scalar`, mantém válidas as **76
inicializações agregadas** existentes em `AnalysisEngine.cpp`
(`{key, value, unit, quality, valuePresent}`), inclusive as parciais. Compilado com
`-Wall -Wextra -Werror`, C++17.

### 4.2 Precedentes e ausência de infraestrutura reutilizável

Não existe infraestrutura de *metadata* / *annotations* / *context* reutilizável. O que existe são
**precedentes de qualificador booleano fora do canal escalar**, todos legítimos e semanticamente
próximos: `CaptureEvidence::partial`, `StimulusReport::clipped`, `StimulusReport::nonDecayed`.
Nenhum é reaproveitável — pertencem ao domínio da **aquisição**, não ao do **resultado derivado** —,
mas estabelecem que o projeto já trata qualificadores como **campo tipado**, não como escalar
numérico.

### 4.3 Contraprova quantitativa do custo de exportação

O CSV escalar de uma medição normal tem **140 linhas, das quais 99 (70,7 %) já são `DBG_*`**. O canal
não é um relatório curado; é um despejo diagnóstico com **V7** aberto. Acrescentar duas linhas ali é
um custo real, mas de outra ordem de grandeza do que se supunha.

## 5. Comparação das alternativas

| | Fatos primários | Semântica única | Superfície CSV | Estrutura | Cobre JS | Risco às 15 famílias |
|---|:--:|:--:|:--:|:--:|:--:|:--:|
| **A** escalares públicos | **sim** | não — cada consumidor re-deriva | +2 linhas, automático | nenhuma | por espelhamento | nenhum |
| **B** escalar semântico derivado | não | sim | +1 linha, automático | nenhuma | por espelhamento | nenhum |
| **C** metadados em `MetricResult` | sim | sim | nenhuma | contrato de 15 famílias | plumbing próprio | **alto** |
| **D** natureza da estimativa em `Scalar` | não | sim | **nenhuma** | 1 campo, compat. provada | por espelhamento | baixo |
| **E** novo valor em `Quality` | não | confunde eixos | muda toda formatação | enum central | sim | **alto** |
| **F** recomputação posterior | reconstruídos | sim | nenhuma | nenhuma | duplicada | **alto** |

**A** — preserva o fato bruto, permitindo reinterpretar CSVs antigos se a regra da §1 for refinada (e
a §1 mostra que ela *precisa* de refinamento: o terceiro caso). Custo: a semântica fica no consumidor,
e há dois consumidores independentes (ExportEngine e LabEngine) — precisamente o modo de falha que
**CC-A** existe para impedir.

**B** — resolve a duplicação, perde a auditabilidade. Um booleano transportado como `double` com
unidade num canal de escalares numéricos é impróprio, e amarra o formato exportado à versão da regra
de classificação.

**C** — granularidade errada. O qualificador pertence ao **escalar** `DynamicRange_dB`, não à família
MS-09. Uma família pode publicar simultaneamente um valor pontual e um limite. Além disso nada
consome metadados hoje; seria preciso plumbing em cada superfície de qualquer forma — sem nenhuma das
vantagens de **D**.

**D** — granularidade correta, compatibilidade de API **provada por compilação**, e **invisível à
exportação por construção**: nenhuma linha nova no CSV até que alguém decida. É a única alternativa em
que "não mexer na exportação agora" é o comportamento **padrão** em vez de uma restrição a ser
vigiada.

**E** — rejeitada. Um teto censurado é uma medição *plenamente confiável*; codificá-lo em `Quality`
destrói a ortogonalidade dos eixos e obriga a revisitar todo `if`/`switch` sobre `Quality` em
`DashboardModel`, `ExportEngine`, `SessionSerializer`, frontend e testes.

**F** — rejeitada. Repete exatamente o modo de falha de **F9** e **P1-003**: derivar fora da família
que detém a física produz um segundo estimador que diverge. Também desnecessária: os dois inteiros
custam zero.

**A e D não competem — compõem.** **D** transporta a *conclusão* com semântica única; **A** preserva
os *fatos* que a sustentam.

## 6. Matriz de impacto

| Superfície | A (escalares) | D (campo em `Scalar`) | E (`Quality`) |
|---|---|---|---|
| `Scalar` / `MetricResult` | intactos | +1 campo NSDMI ao final | enum central alterado |
| 76 inicializações agregadas | intactas | **intactas** (provado) | intactas |
| `exportScalarsCSV` | +2 linhas automáticas | nenhuma mudança | `formatScalar` revisto |
| PDF / `ReportModel` | nenhuma | nenhuma | revisão de rótulos |
| `DashboardModel` | nenhuma | nenhuma | novo `ItemState` |
| `SessionSerializer` | +2 propriedades | nenhuma (lista explícita) | +1 string |
| `.aamp` vivo (Evidence) | **nenhuma** | **nenhuma** | **nenhuma** |
| LabEngine JS | espelhamento CC-A | espelhamento CC-A | espelhamento CC-A |
| Trust Ladder | precisa decidir se consome | idem | consome por acidente |
| Testes existentes | nenhum quebra | nenhum quebra | vários revistos |
| Outras 14 famílias | nenhum | `Point` por padrão | todas revisadas |

Nenhum teste atual depende da cardinalidade de escalares do MS-09 — os únicos usos de `scalars[0]`
são do MS-11.

## 7. Compatibilidade histórica

| Mudança | Classificação |
|---|---|
| Campo novo em `Scalar` (D) | **apenas semântica** — nada muda em nenhuma saída até uma superfície optar por lê-lo |
| Escalares novos (A) | **adição compatível** — `key=value` por linha; leitores existentes ignoram linhas desconhecidas |
| `formatScalar` emitindo `>=86.789` | **quebra de formato** — consumidores que fazem *parse* numérico falham. **Não fazer** |
| `.aamp` históricos | **nenhum impacto** — só evidência bruta; a ciência é recomputada e ganharia o qualificador automaticamente, o que é o comportamento desejado e coerente com a regra já registrada |
| CSVs históricos | arquivos inalterados; CSVs *novos* trazem linhas extras. Comparação por *diff* entre versões degrada — já coberto pelo carimbo `CommitFull`/`BuildConfig`/`GitDirty` do DiagLog |

## 8. Generalização — o problema não é exclusivo do MS-09

Levantamento no código, **sem alterar nada**:

| Local | Fenômeno | Estado |
|---|---|---|
| `snrDb`, `AnalysisEngine.cpp:112` — `if (pn <= 0.0) return 200.0;` | **limite inferior** apresentado como valor pontual; o próprio comentário diz "muito alto (limitado)" | **decisão aberta P1-007/D4** |
| `noiseFloor` — `std::max(integFull, 1e-30)` → −300 dBFS | **limite superior**: o piso é *no máximo* isso; hoje tratado como `NotApplicable` | DV-5, fechada de forma mais grosseira |
| `crosstalk`, `:927` — `1e-30` nos dois lados | limite quando um canal é digitalmente silencioso | **latente, não verificado** |
| `dynamicBehaviour` settling, `:981` — `settleIdx = 0` inicial sem `break` | "nunca estabilizou" e "estabilizou instantaneamente" produzem ambos `0 ms` | **latente, não verificado** |
| `bestLag(..., maxLag = 0.050·fs)` em MS-09/13/14 | latência real acima de 50 ms satura no limite de busca, silenciosamente | **latente, não verificado** |

Os latentes ficam **registrados como observação, sem correção e sem expansão de escopo**. Mas dois
deles — MS-10 `pn≈0` e MS-08 `−300` — **já são itens abertos do Plano Mestre**, e o primeiro é
precisamente a decisão que **P1-007/D4** terá de tomar.

> **Nota de desfecho (P1-007, 2026-08-10).** O levantamento acima é preservado como estava. A análise
> posterior **corrigiu duas de suas premissas**, e a correção é registrada aqui sem reescrever o
> original:
>
> 1. **`pn ≤ 0 → 200` não é "limite inferior apresentado como valor pontual" no caminho oficial** — o
>    ramo é **estruturalmente inalcançável** em `AnalysisEngine::snr()` (`NoiseFloor_dBFS ≥ −300` ⇒
>    `pn ≥ 1e-30 > 0`). Ele permanece **alcançável apenas na primitiva pública `snrDb()`**, por
>    chamada direta. Ratificado que `200 dB` **não** será convertido em `LowerBound`.
> 2. **O clamp `−300 dBFS` NÃO é "hoje tratado como `NotApplicable`" em todos os casos.** Apenas
>    `integFull == 0` o é. Na janela `0 < integFull < 1e-30` o piso é publicado com
>    `quality = Normal` e `valuePresent = true`, e **MS-09 e MS-10 o consomem como medição válida**
>    (≈ 291 dB). Esse é o defeito material **DV-10**, hoje o objeto reclassificado do **P1-007**.
> 3. Ratificado que `−300 dBFS` **não é limite superior metrológico do piso**, e sim guarda contra
>    `log10(0)`: a leitura "o piso é *no máximo* isso" **não** sustenta um `UpperBound` físico.
>
> **P1-007 permanece NÃO IMPLEMENTADO.** Ver [`metrics-audit.md`](./metrics-audit.md) → *Registro de
> Decisão — P1-007*.

O problema é transversal **por evidência, não por especulação**. Um mecanismo compartilhado não é
superengenharia; a ausência dele forçaria P1-007 a inventar um segundo tratamento *ad hoc* e
incompatível para o mesmo fenômeno.

## 9. Recomendação arquitetural do estudo

**D + A, em dois estágios, D primeiro.**

*Estágio 1 — natureza da estimativa.* Campo tipado em `Scalar`, declarado por último, com
inicializador padrão. Ortogonal a `Quality` e a `valuePresent`. Derivado exclusivamente de `s*` e
`min(C)` — inteiros, nunca comparações de ponto flutuante. Espelhado no `computeMS09` do LabEngine
(**CC-A**). Zero mudança em exportação, PDF, dashboard, `.aamp` e nas outras 14 famílias.

*Estágio 2 — fatos primários.* `DynamicRange_stepIndex` e `DynamicRange_firstClippedIndex` como
escalares públicos, para que a classificação seja auditável e reinterpretável. Este estágio, e só
este, acrescenta linhas ao CSV e exige autorização própria.

Separar os estágios permite decidir a exportação como uma questão à parte da ciência.

## 10. Representação futura sugerida

Regra: **o número permanece número nos canais de máquina; o qualificador só vira prefixo nos canais
humanos.**

| Canal | Representação | Justificativa |
|---|---|---|
| CSV escalar | `DynamicRange_dB=86.789` + `DynamicRange_dB_nature=LowerBound` | *parse* numérico preservado; adição compatível; sem ambiguidade |
| Relatório/PDF | `Dynamic Range: ≥ 86.789 dB (teto não demonstrado)` | legibilidade humana; sem consumidor de máquina |
| Dashboard / frontend | `≥ 86.8 dB` com legenda "o ensaio não alcançou a saturação" | idem |
| Trust Ladder | consumir explicitamente; um limite inferior **não** deve rebaixar a confiança | é medição confiável, não incerta |

Rejeitado: `DynamicRange_dB=>=86.789` no CSV — quebra todo leitor numérico e é a única variante que
introduz ambiguidade real.

## 11. Relação com os próximos itens

**Não bloqueia P1-005.** P1-005 substitui o `pn` do MS-10 pelo Noise Floor *band-limited*; P1-006
isola a energia do tom no numerador. A dependência é **unidirecional**: MS-09 → MS-08 e
MS-10 → MS-08. **MS-10 não lê MS-09.** Nada em P1-005/P1-006 depende de como o *teto* é qualificado,
e nenhum dos dois toca o laço do ST-08.

**Acoplado a P1-007.** A decisão **D4** de P1-007 — qual estado atribuir a `pn≈0` — é literalmente o
mesmo fenômeno. Se P1-007 for executado antes de existir a taxonomia, decidirá isoladamente e
provavelmente escolherá `LowConfidence` ou um novo `Quality`, contaminando o eixo que deve
permanecer separado.

Conclusão: **paralelo a P1-005 e P1-006; resolver antes de P1-007.**

## 12. Testes permanentes propostos

Nenhum implementado. IDs a reconfirmar imediatamente antes de qualquer edição — `TC-045` já mostrou
por quê.

1. **Classificação da natureza** nos casos da §1.
2. **Ortogonalidade dos três eixos**: `LowerBound` com `quality = Normal` e `valuePresent = true`; e
   `LowConfidence × LowerBound` representável simultaneamente.
3. **Invariância de exportação (estágio 1):** um escalar `LowerBound` produz CSV **byte-idêntico** ao
   caso `Point`.
4. **CC-A:** convergência C++ ↔ JS sobre entradas bit-idênticas, comparando valor, `quality`,
   `valuePresent` **e** natureza.
5. **Controle negativo NaN:** entrada com `NaN` não pode produzir `Point` silenciosamente.
6. **Não-regressão:** `DynamicRange_dB` numericamente inalterado; TC-038/039/041/043/046 intactos.

## 13. F11 — comentário residual

Não corrigido. Deve ser atualizado na **primeira intervenção futura que já modifique
`AnalysisEngine::noiseFloor()` por motivo funcional**. Regra registrada: **nunca em commit isolado** —
alterar código validado sem gate funcional é churn sobre binário auditado.

## 14. F10 — NaN

Fora de escopo, não corrigido. Verificação realizada:

- **A, B, D não pioram** a detecção, *desde que* a derivação use `s*` e `min(C)`. Um classificador
  baseado em comparação de dB herdaria a propriedade de `NaN` de tornar toda comparação falsa e
  degradaria em silêncio para "observado" — motivo pelo qual a regra de derivação por índices é
  **requisito**, não preferência.
- **Agravamento marginal de apresentação:** renderizar `≥ nan dB` é visualmente mais autoritativo que
  `nan dB`. Mitigável exigindo que a superfície humana só aplique o prefixo quando o valor for finito.
- **E agravaria**: um estado novo em `Quality` tenderia a dominar a apresentação e mascarar a
  anomalia numérica.

---

## 15. Decisão e desfecho — **RATIFICADO**

> **Esta seção é o desfecho normativo do estudo.** As §§1–14 permanecem como registro pré-decisão e
> não foram reescritas. Registro completo em
> [`metrics-audit.md`](./metrics-audit.md) → *Registro de Decisão — P1-009*.

**Arquitetura aprovada: D + A em dois estágios**, conforme a recomendação da §9.

**Estágio 1 (D)** — qualificador tipado no nível de `Scalar`, ortogonal a `Quality` e
`valuePresent`, com taxonomia inicial `Point` · `LowerBound` · `UpperBound` e **`Point` como
padrão**, preservando integralmente o comportamento das demais métricas. Vedado criar
`Quality::LowerBound`; vedado usar `LowConfidence` para representar unilateralidade.

**Estágio 2 (A)** — `DynamicRange_stepIndex` e `DynamicRange_firstClippedIndex` preservados como
**proposta futura**, sem implementação e **sem autorização de exportação**. Depende de decisão
explícita sobre contrato/exportação e pode ser diferido ao **Pilar 3**.

**Regra formal dos casos do MS-09** (decidida; inclusive o terceiro caso, que o estudo levantou):

| Condição | Natureza | Observação |
|---|:--:|---|
| `C ≠ ∅` e `min(C) = s* + 1` | **`Point`** | saturação demonstrada imediatamente acima do maior degrau válido |
| `C = ∅` e `s* = último` | **`LowerBound`** | censura pelo limite superior do ensaio |
| `C ≠ ∅` e `min(C) > s* + 1` | **`LowerBound`** | saturação observada mas **não** encaixotada |
| `V = ∅` | *(irrelevante)* | `quality = NotApplicable`, `valuePresent = false` — **ausência de valor nunca vira `LowerBound`** |

Fundamento do terceiro caso: sabe-se que o DUT suporta **ao menos** o nível de `s*`, logo `DR ≥ X`
permanece verdadeira; não há evidência para afirmar `DR = X`; não é `NotApplicable`, pois existe
informação quantitativa válida; e não é `LowConfidence`, pois unilateralidade e confiança são
dimensões distintas. Registrado que este caso **poderá** futuramente admitir representação mais rica
(intervalo), **sem** introduzir essa taxonomia agora, por falta de necessidade demonstrada.

**Item formalizado como `P1-009`** no Plano Mestre — estágio 1 é **pré-requisito de P1-007** e **não
bloqueia P1-005 nem P1-006**.

**Correção de premissa ratificada** (§4.1): escalar novo é exportado automaticamente; **membro novo
de `Scalar` não é**. A conclusão registrada no P1-004 fica **refinada, não invalidada**.

**Nada foi implementado.** A ratificação é exclusivamente normativa e documental.

---

## 16. Encerramento do Estágio 1 — **IMPLEMENTADO E VALIDADO**

> **As §§1–15 permanecem intactas** como registro pré-decisão e da ratificação. Esta seção acrescenta
> **apenas o desfecho de implementação** e não reescreve nada do que veio antes.
> Registro completo em [`metrics-audit.md`](./metrics-audit.md) → *Registro de Encerramento —
> P1-009, Estágio 1*.

**O que foi entregue.** A recomendação **D** da §9 foi implementada exatamente como ratificada:
`Scalar` passou a possuir uma **natureza tipada da estimativa** — `Point` · `LowerBound` ·
`UpperBound`, com **`Point` por padrão** —, **ortogonal** a `Quality` e a `valuePresent`. O MS-09
declara `Point` apenas quando a saturação foi demonstrada **imediatamente acima** do maior degrau
válido; em qualquer outra situação declara `LowerBound`.

**Confirmação do achado que reordenou o problema.** A previsão registrada no início deste estudo —
*"o teto censurado não é um caso de borda: é o regime dominante"* — foi confirmada na implementação:
em cadeia de ganho unitário o resultado é `LowerBound`, e só passa a `Point` quando o ganho leva
algum degrau ao fundo de escala.

**A premissa refinada da §4.1 sustentou-se na prática.** O membro acrescentado ao final de `Scalar`
**não escapou para nenhuma saída**: dois bundles que diferem apenas na natureza produzem CSV escalar
**byte-idêntico**, `ReportModel` idêntico e `DashboardModel` idêntico — verificado por teste
permanente. Foi essa distinção entre **campo novo** e **escalar novo** que tornou possível entregar a
ciência sem tocar a exportação.

**Arranjo adicional identificado na implementação.** A §1 havia enumerado três condições; a
implementação revelou uma quarta — `C = ∅` **e** `s* <` último degrau, quando degraus superiores são
inválidos por captura truncada ou ausência de energia in-band. Foi classificada como **`LowerBound`**
por aplicação direta do princípio ratificado, e a lacuna foi fechada por evidência direta no commit
de teste `a1751011db5f83c5e2258374adee713642bf24f9` (TC-047, caso 1b), com controle discriminante em
ganho ×2. *Registro de honestidade metodológica: a enumeração original do estudo era incompleta.*

**Testes.** As seis proteções propostas na §12 foram implementadas em **TC-047**, incluindo a
invariância de exportação (§12.3) e o controle negativo relacionado a NaN (§12.5) — este último
demonstrando que a classificação depende **apenas de índices**, nunca de comparação de `double`.
**F10 permanece aberto e não foi corrigido**; nenhuma sanitização de entrada foi acrescentada.

**Validação independente no Windows — APROVADO.** Commit
`a1751011db5f83c5e2258374adee713642bf24f9`, `Release`, `dirty = false`, 0 warnings / 0 erros;
`aamp_headless_suite` **773 checks / 0 falhas**; `aamp_scientific_validation` **8 checks / 0
falhas**; CTest **2/2**; TC-030 · TC-034 · TC-038 · TC-039 · TC-041 · TC-043 · TC-046 · TC-047.

**Estágio 2 — ABERTO.** A recomendação **A** da §9 — `DynamicRange_stepIndex` e
`DynamicRange_firstClippedIndex` como escalares públicos — **não foi implementada** e permanece como
evolução futura, junto da eventual integração com exportação e interface (§10). A conclusão da §11
sobre sequenciamento mantém-se: **P1-005 e P1-006 estão liberados; P1-007 deve reutilizar esta
taxonomia em vez de criar outra.**

> **Nota de desfecho (2026-08-10).** **P1-005** (`4825fcc`) e **P1-006** (`a702c0f`) foram
> **concluídos** e validados independentemente no Windows; o MS-10 publica `EstimateNature = Point`
> nos dois motores, sem criar `LowerBound`. A obrigação sobre o **P1-007** — reutilizar esta
> taxonomia — **permanece integralmente**. O **estágio 2** deste estudo segue **aberto**. O texto
> acima é preservado como estava no encerramento do estágio 1.
