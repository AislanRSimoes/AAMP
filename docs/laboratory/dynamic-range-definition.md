# Definição Normativa de Dynamic Range

> **Status:** ESPECIFICAÇÃO NORMATIVA APROVADA.
> **Escopo:** define *o que* o AAMP mede e reporta como "Dynamic Range" (faixa dinâmica), em termos
> físicos e normativos. Não descreve *como* a medida é obtida. Documento independente de qualquer método
> de análise. **Passo 1** do fluxo metodológico (especificação normativa — *o que medir*).

---

## 1. O que o AAMP chama de Dynamic Range

**Dynamic Range** (faixa dinâmica) é a **extensão útil, em dB, entre o maior sinal que a cadeia de
medição reproduz sem saturação e o piso de ruído residual (Noise Floor)**. Representa o "espaço"
disponível entre o **teto** — nível a partir do qual o sinal começaria a saturar — e o **piso** — nível
abaixo do qual o sinal se confunde com o ruído.

É um **valor escalar único** por medição (por canal).

## 2. Qual é a grandeza física medida

A grandeza é a **diferença entre dois níveis**, ambos tomados na **mesma banda** e sob a **mesma
referência de fundo de escala**:

- o **nível máximo de sinal reproduzido sem saturação** (teto), obtido a partir de uma progressão
  controlada de níveis (ST-08); e
- o **nível do Noise Floor** (piso), conforme a especificação normativa
  [`noise-floor-definition.md`](./noise-floor-definition.md), medido em silêncio (ST-09).

É uma grandeza **intensiva de razão** (uma razão de potências expressa em dB) — **não** um nível
absoluto. Não é um *ponto* na escala, e sim a *distância* entre dois pontos.

### Independência da Grandeza Física

O Dynamic Range representa uma **propriedade física da aquisição** (a extensão útil entre a saturação e
o ruído), e não do método utilizado para estimá-la. Qualquer algoritmo empregado para determinar essa
grandeza constitui apenas um **estimador** da definição normativa aqui estabelecida. Alterações futuras
na implementação somente serão aceitas se **preservarem a equivalência metrológica** desta definição.

### Princípio da Independência do Estimador

A definição normativa do Dynamic Range é **independente do algoritmo utilizado para sua estimação**.
Mudanças futuras em qualquer técnica de processamento **não alteram esta especificação**; apenas alteram
o método de estimativa da mesma grandeza física. Como o Dynamic Range **deriva do Noise Floor**, herda
integralmente o Princípio da Independência do Estimador daquela definição.

## 3. Qual a unidade

**dB** — a faixa dinâmica é uma **diferença de níveis** (razão de potências), portanto adimensional,
expressa em dB. **Não é dBFS**: "dBFS" designa um **nível absoluto** (o teto e o piso, individualmente,
são níveis em dBFS), enquanto o Dynamic Range é a **distância** entre esses dois níveis. É reportado
como um **único valor em dB**, não-negativo.

## 4. Qual a banda considerada

**20 Hz a 20 kHz**, **sem ponderação** (resposta plana / *flat*), **coerente com o Noise Floor** (do
qual o piso é derivado). O teto e o piso **devem** ser tomados na **mesma banda** para que a diferença
seja fisicamente válida. A banda e a ausência de ponderação são **parte integrante da definição** e
devem acompanhar o valor sempre que ele for reportado ou comparado.

## 5. O que está explicitamente excluído

- **Distorção.** O Dynamic Range **não** caracteriza distorção harmônica nem de intermodulação (isso é
  objeto de THD, THD+N, IMD). O **teto é definido pela saturação** (o sinal atinge o fundo de escala /
  *clipping*), **não** pela pureza do sinal; refinamentos por limiar de distorção **não** integram esta
  definição.
- **Energia fora da banda 20 Hz–20 kHz.** Componente contínua (DC), subsônicos e ultrassônicos são
  excluídos **tanto no teto quanto no piso** — avaliados separadamente na camada de **Integridade**.
- **Nível absoluto de operação.** O Dynamic Range **não** informa em que ponto da escala o sistema
  opera; informa **apenas a extensão**. (O teto e o piso são níveis; a faixa é a diferença entre eles.)
- **Ponderação psicoacústica** (p.ex. curva A): não é aplicada — o valor normativo é plano.
- **Sinal intencional no piso.** O piso é **sempre** medido na ausência de excitação (herança direta da
  definição de Noise Floor).

## 6. Relação com as demais camadas

- **MS-08 (Noise Floor).** É o **limite inferior** do Dynamic Range: `Dynamic Range = teto − Noise
  Floor`. O Dynamic Range **consome** a definição já ratificada de Noise Floor; ambos na **mesma banda e
  referência**, o que torna a subtração dimensionalmente coerente. Dependência **unidirecional**: o
  Dynamic Range depende do Noise Floor; o Noise Floor **não** depende do Dynamic Range.
- **MS-10 (SNR).** Grandeza **aparentada, porém distinta**. O SNR usa um **nível de sinal de referência
  fixo**; o Dynamic Range usa o **maior nível sem saturação** (teto). Ambos compartilham o **Noise
  Floor** como piso comum. Não confundir: o SNR mede a margem de um sinal de referência; o Dynamic
  Range mede a **extensão total utilizável**.

  **Relação `Dynamic Range ≥ SNR` — propriedade condicional (decisão B).** Esta relação **não é uma
  invariante numérica obrigatória**: é uma **consequência** da condição física

  > *o nível de referência do SNR está contido no teto efetivamente caracterizado* — `referência ≤ teto`.

  Quando essa condição se verifica, e sendo o piso comum, segue-se `Dynamic Range ≥ SNR`. Quando ela
  **não** se verifica — porque o teto não foi observado (ver o parágrafo seguinte), porque o suíte de
  estímulos coloca referência e teto no **mesmo nível nominal**, ou porque os estimadores das duas
  grandezas têm vieses distintos — a relação **pode não se sustentar numericamente**, e essa
  ocorrência **não constitui, por si só, violação normativa**.
  É **expressamente vedado** impor a desigualdade por meios artificiais: **não** se aplica saturação
  (`clamp`), **não** se substitui `DR` por `max(DR, SNR)` nem equivalente, e **não** se alteram os
  estímulos para fabricá-la. Uma inversão observada deve ser **investigada como indício** — de teto
  censurado ou de viés de estimador — nunca corrigida por construção.

  **Teto não atingido — censura pelo limite superior do ensaio (decisão D).** O teto normativo é o
  **início da saturação**. Quando a progressão de níveis do ST-08 **não alcança a saturação**, o maior
  nível observado **não demonstra** esse início: sabe-se apenas que a cadeia **suporta ao menos** aquele
  nível. Nessa condição o teto está **censurado pelo limite superior do ensaio**, e o Dynamic Range
  dele derivado é um **limite inferior** da faixa dinâmica real — não a faixa dinâmica. É precisamente
  nesse regime que a relação `Dynamic Range ≥ SNR` perde qualquer garantia.
  *Este princípio está **registrado**; a sinalização do estado, a exportação, a apresentação e o
  cálculo **não** foram alterados nesta etapa.*
  *Situação após o **P1-004** (2026-08-08): continua **sem implementação**, agora como **item próprio
  e aberto** — desmembrado por fronteira arquitetural, não abandonado nem implicitamente
  implementado.*

  **Natureza da estimativa — ratificação P1-009 (2026-08-08).** A representação do teto censurado
  está **decidida** (implementação **não** iniciada). Um resultado de Dynamic Range passa a admitir
  uma **natureza** própria, **ortogonal** à qualidade da medição e à presença do valor:

  | Natureza | Significado |
  |---|---|
  | **`Point`** | estimativa pontual — `Dynamic Range = X dB` |
  | **`LowerBound`** | limite inferior — `Dynamic Range ≥ X dB` |
  | **`UpperBound`** | limite superior — reservado; sem uso no MS-09 |

  Um teto censurado é `quality = Normal`, `valuePresent = true`, natureza `LowerBound`: a medição é
  **plenamente confiável**, o valor **existe**, e apenas a *quantificação lógica* é unilateral.
  É **vedado** representar unilateralidade por `LowConfidence` — confiança e unilateralidade são
  dimensões distintas — e é **vedado** criar um valor novo de `Quality` para isso.

  Regra formal, com `V` = degraus de estimativa válida, `C` = degraus rejeitados por saturação e
  `s*` = degrau selecionado:

  | Condição | Natureza |
  |---|:--:|
  | `C ≠ ∅` e `min(C) = s* + 1` — saturação demonstrada imediatamente acima | **`Point`** |
  | `C = ∅` e `s* = último degrau` — censura pelo limite superior do ensaio | **`LowerBound`** |
  | `C ≠ ∅` e `min(C) > s* + 1` — saturação observada mas **não encaixotada** | **`LowerBound`** |
  | `C = ∅` e `s* <` último degrau — degraus superiores inválidos por outra causa | **`LowerBound`** |
  | `V = ∅` — sem valor | *(irrelevante)* — `NotApplicable`, `valuePresent = false` |

  A **ausência de valor nunca é convertida em `LowerBound`**. O caso de **saturação não encaixotada**
  poderá futuramente admitir representação mais rica (intervalo); essa taxonomia **não** é
  introduzida agora.

  **Registro de regime.** Com o ST-08 atual — escada de 8 degraus com pico máximo de **−6 dBFS** —,
  cadeias de **ganho unitário ou inferior a +6 dB nunca demonstram saturação**. O teto censurado é,
  portanto, o **regime normal** dessas medições, e **não** um caso raro. É este o regime em que a
  propriedade condicional `Dynamic Range ≥ SNR` (decisão **B**) perde garantia.

  **Conformidade de implementação (P1-009, estágio 1), 2026-08-08.** A natureza da estimativa passou
  a ser **efetivamente implementada** como campo tipado de `Scalar`, ortogonal a `Quality` e a
  `valuePresent`, com `Point` por padrão. O MS-09 declara `LowerBound` sempre que a saturação não foi
  demonstrada imediatamente acima do maior degrau válido. Commit `a175101`, protegido por
  **TC-047**, validado independentemente no Windows. O valor de `DynamicRange_dB` permanece
  numericamente inalterado, e **exportação, relatório, dashboard e Evidence Package continuam
  invariáveis** — o **estágio 2** (fatos primários e integração com exportação/UI) **permanece
  aberto**.

  Registro completo em [`metrics-audit.md`](./metrics-audit.md) → *Registro de Encerramento —
  P1-009, Estágio 1*; decisão em *Registro de Decisão — P1-009*; estudo em
  [`censored-ceiling-architecture-study.md`](./censored-ceiling-architecture-study.md).
- **Exportação.** O Dynamic Range é exportado como um **escalar em dB**, acompanhado, nos metadados, da
  **banda** (20 Hz–20 kHz), da **ausência de ponderação** e da **condição** (teto sem saturação; piso em
  silêncio). É **homogêneo** com as demais métricas expressas em dB.
- **Trust Ladder (confiança da medição).** O Dynamic Range delimita a **extensão útil** da medição: uma
  faixa dinâmica estreita **limita a confiança** em leituras finas (assim como um SNR baixo). Contribui,
  portanto, para a avaliação de confiabilidade da caracterização.

## 7. Propriedades que a definição deve preservar

- **Comparabilidade.** Comparável entre **sessões, instrumentos e dispositivos sob teste**, desde que
  **banda, ausência de ponderação e critério de teto (saturação)** sejam declarados e idênticos (banda e
  ponderação são fixas).
- **Estabilidade.** Dado tempo de observação suficiente, o valor deve ser **estável** e depender apenas
  do **teto físico** e do **piso físico** — não de parâmetros de resolução da observação.
- **Rastreabilidade.** O valor deve ser **auditável**: teto, piso, banda e condições declarados junto ao
  número.
- **Coerência dimensional.** Deve resultar da **diferença de dois níveis em dBFS** (mesma banda, mesma
  referência), produzindo um valor em **dB**; a subtração permanece fisicamente válida por repousar na
  **homogeneidade** desses níveis.
- **Reprodutibilidade.** Sob a mesma condição de medição, deve produzir o **mesmo valor**
  (comportamento determinístico).
- **Neutralidade de escopo.** Deve representar **apenas** a extensão entre a saturação e o ruído — nunca
  a distorção, o nível absoluto de operação, ou energia fora da banda.
- **Tratamento da degeneração.** Quando o teto **não excede** o piso (o sinal não se eleva acima do
  ruído), a faixa dinâmica é **não caracterizável** (faixa não utilizável) — condição que deve ser
  **sinalizada** como não caracterizável, e **nunca** reportada como valor positivo espúrio **nem como
  número negativo** (decisão **D3**). Inclui o caso-limite `teto = piso`.
  *(Implementado no **P1-004** — commit `6ed71e8`, protegido por **TC-046**.)*

---

> **Ratificação (Registro de Decisão MS-09 — D1/D2/D3).** Esta especificação está **APROVADA**:
> **D1** — teto = **início da saturação (clipping)**, expresso como **nível RMS do maior sinal sem
> clipping** (não por limiar de distorção); **D2** — teto na **mesma banda do Noise Floor (20–20 k)**, o
> que **herda a precondição AR-01** (validação energética antes do reúso em banda); **D3** — `teto ≤ piso`
> → **não caracterizável** (faixa não utilizável). Registro completo em
> [`metrics-audit.md`](./metrics-audit.md) → "Registro de Decisão — MS-09".
>
> **Referência de embasamento:** [`noise-floor-definition.md`](./noise-floor-definition.md) — o piso do
> Dynamic Range.
>
> **Conformidade de implementação (P1-003), 2026-08-07.** A exigência da **§4** — teto e piso na
> **mesma banda** — e a exclusão da **§5** — DC, subsônicos e ultrassônicos fora do teto — passaram a
> ser **efetivamente implementadas**: o teto é o **nível RMS in-band 20 Hz–20 kHz**, medido pelo
> **mesmo estimador espectral do Noise Floor** (Hann, N = 4096, ENBW/Parseval), sobre um quadro
> centrado no degrau e após o alinhamento temporal. O gate de saturação permanece no **sinal temporal
> bruto**. Commit `ffa739c`, protegido por **TC-043**, validado independentemente no Windows.
> **A §7 (Tratamento da degeneração) e a decisão D (teto censurado) continuam sem implementação.**
> Registro completo em [`metrics-audit.md`](./metrics-audit.md) → *Registro de Encerramento — P1-003*.
>
> **Ratificação complementar (Registro de Decisão — F2), 2026-08-07.** Precisada a §6 com duas
> decisões: **B** — `Dynamic Range ≥ SNR` é **propriedade condicional** de `referência ≤ teto`, nunca
> invariante imposta; **D** — quando o ST-08 não alcança a saturação, o teto está **censurado pelo
> limite superior do ensaio** e o Dynamic Range é um **limite inferior**. Nenhuma alteração de
> cálculo, estado de qualidade, exportação, interface ou estímulo acompanha esta ratificação.
> Registro completo em [`metrics-audit.md`](./metrics-audit.md) → *Registro de Decisão — F2*.
>
> **Conformidade de implementação (P1-004 — núcleo semântico), 2026-08-08.** A **§7 (Tratamento da
> degeneração)** passou a ser **efetivamente implementada**: `teto ≤ piso` — **inclusive o
> caso-limite `teto = piso`** — é sinalizado como **não caracterizável** (`NotApplicable`, valor
> ausente), **nunca** como valor positivo espúrio nem como número negativo, e sem `abs()`, `clamp`
> ou substituição por zero. Implementada também a **dependência unidirecional da §6**: o estado do
> **Noise Floor** governa o do Dynamic Range — piso não caracterizável torna o DR não
> caracterizável, e piso de confiança reduzida rebaixa o DR a `LowConfidence` **preservando o
> valor**. A medição normal permanece numericamente inalterada. Commit `6ed71e8`, protegido por
> **TC-046**, validado independentemente no Windows.
> **A decisão D (teto censurado, §6) continua SEM implementação** — foi **desmembrada por fronteira
> arquitetural** em item próprio e aberto, por se ter demonstrado que qualquer escalar novo é
> exportado automaticamente e que `MetricResult` não possui superfície para metadados internos.
> Registro completo em [`metrics-audit.md`](./metrics-audit.md) → *Registro de Encerramento — P1-004*.
>
> **Ratificação complementar (Registro de Decisão — P1-009), 2026-08-08.** Decidida a
> **representação** do teto censurado da decisão **D**: uma **natureza da estimativa**
> (`Point`/`LowerBound`/`UpperBound`, padrão `Point`) **ortogonal** a `Quality` e a `valuePresent`,
> com a regra formal dos casos do MS-09 fixada na **§6**. Nenhuma alteração de cálculo, de estado de
> qualidade, de exportação, de interface, de contrato ou de estímulo acompanha esta ratificação — a
> implementação **não** foi iniciada. Registro completo em
> [`metrics-audit.md`](./metrics-audit.md) → *Registro de Decisão — P1-009*.
>
> **Conformidade de implementação (P1-009 — estágio 1), 2026-08-08.** A natureza da estimativa está
> **implementada e validada independentemente no Windows** (`a175101`, protegida por **TC-047**):
> campo tipado de `Scalar`, `Point` por padrão, com o MS-09 declarando `Point` ou `LowerBound`
> conforme a regra da **§6**. `DynamicRange_dB` permanece numericamente inalterado; exportação,
> relatório, dashboard e Evidence Package permanecem invariáveis. **O estágio 2 — exposição dos
> fatos primários e integração com exportação/UI — permanece ABERTO.** Registro completo em
> [`metrics-audit.md`](./metrics-audit.md) → *Registro de Encerramento — P1-009, Estágio 1*.
