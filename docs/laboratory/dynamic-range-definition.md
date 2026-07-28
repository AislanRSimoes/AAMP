# Definição Normativa de Dynamic Range

> **Status:** PROPOSTA NORMATIVA — pendente de ratificação (decisão do MS-09).
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
  Floor** como piso comum. Em geral, **Dynamic Range ≥ SNR** (o teto é ≥ o nível de referência). Não
  confundir: o SNR mede a margem de um sinal de referência; o Dynamic Range mede a **extensão total
  utilizável**.
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
  ruído), a faixa dinâmica é **nula ou indefinida** — condição que deve ser **sinalizada** como
  caracterização não utilizável, e nunca reportada como um valor positivo espúrio.

---

> **Nota de ratificação.** O único ponto substantivo desta definição a confirmar conscientemente é o
> **critério de teto**: aqui adotado como **início da saturação (clipping)**, e não como um limiar de
> distorção. A ratificação deste documento corresponde ao **Passo 4** (registro de decisão) do fluxo do
> MS-09 e habilita o **Passo 2** (auditoria da implementação).
>
> **Referência de embasamento:** [`noise-floor-definition.md`](./noise-floor-definition.md) — o piso do
> Dynamic Range.
