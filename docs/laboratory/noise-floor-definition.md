# Definição Normativa de Noise Floor

> **Status:** ESPECIFICAÇÃO NORMATIVA APROVADA.
> **Escopo:** define *o que* o AAMP mede e reporta como "Noise Floor", em termos físicos e normativos.
> Não descreve *como* a medida é obtida. Documento independente de qualquer método de análise.

---

## 1. O que o AAMP chama de Noise Floor

**Noise Floor** é o **nível de ruído residual da cadeia de medição na ausência de sinal intencional** —
o "piso" acima do qual qualquer sinal medido precisa se elevar para ser observável. É determinado a
partir da caracterização realizada sob **silêncio intencional** (estímulo ST-09), quando nenhuma
excitação é aplicada e o que se observa é exclusivamente o ruído próprio do sistema sob teste e da
cadeia de aquisição.

É um **valor escalar único** por medição (por canal), representativo de toda a banda de áudio.

## 2. Qual é a grandeza física medida

A grandeza é o **nível total de ruído contido na banda de áudio** — equivalentemente, o **nível eficaz
(RMS) do ruído residual restrito à banda de áudio**, expresso em relação ao fundo de escala.

É uma grandeza **extensiva de nível**: representa o *quanto de ruído total* existe na banda. **Não** é
uma densidade espectral (nível por unidade de frequência). A densidade espectral é uma grandeza
**complementar e distinta**, que pode ser oferecida como descritor adicional, mas **não** é o que o
AAMP nomeia "Noise Floor".

### Independência da Grandeza Física

O Noise Floor representa uma **propriedade física da aquisição**, e não do método utilizado para
estimá-la. Qualquer algoritmo empregado para determinar essa grandeza constitui apenas um **estimador**
da definição normativa aqui estabelecida. Alterações futuras na implementação somente serão aceitas se
**preservarem a equivalência metrológica** desta definição.

### Princípio da Independência do Estimador

A definição normativa do Noise Floor é **independente do algoritmo utilizado para sua estimação**.
Mudanças futuras em FFT, janelas, estimadores espectrais ou qualquer técnica de processamento **não
alteram esta especificação**; apenas alteram o método de estimativa da mesma grandeza física.

## 3. Qual a unidade

**dBFS** — decibéis relativos ao fundo de escala do sistema (0 dBFS = plena escala; valores de ruído são
negativos). O Noise Floor é reportado como **um único nível em dBFS**.

## 4. Qual a banda considerada

**20 Hz a 20 kHz** (banda de áudio nominal), **sem ponderação** (resposta plana / *flat*). A banda e a
ausência de ponderação são **parte integrante da definição** e devem acompanhar o valor sempre que ele
for reportado ou comparado.

## 5. O que está explicitamente excluído

- **Energia fora da banda 20 Hz–20 kHz:** componente contínua (DC), subsônicos (< 20 Hz) e ultrassônicos
  (> 20 kHz) **não** integram o Noise Floor. Essas componentes são avaliadas **separadamente**, na camada
  de **Integridade** da medição, e nunca inflam ou reduzem o Noise Floor.
- **Qualquer sinal intencional:** o Noise Floor é medido exclusivamente na ausência de excitação. Ele não
  contém tom, sweep, nem qualquer estímulo aplicado.
- **Distorção do sinal:** harmônicos, intermodulação e demais produtos de distorção **não** são ruído e
  **não** pertencem ao Noise Floor; são objeto das métricas de distorção (THD, THD+N, IMD).
- **Densidade espectral (dBFS/Hz):** não é o Noise Floor; é uma grandeza complementar.
- **Ponderação psicoacústica** (p.ex. curva A): não é aplicada — o valor normativo é plano.

## 6. Relação com as demais camadas

- **MS-09 (Dynamic Range).** A faixa dinâmica é definida como a distância entre o **maior nível não
  saturado** e o **Noise Floor**, ambos expressos como níveis em dBFS na **mesma banda**. O Noise Floor é,
  portanto, o **limite inferior** da faixa dinâmica; a subtração de níveis é dimensionalmente coerente.
- **MS-10 (SNR).** A relação sinal-ruído é definida como a distância entre o **nível do sinal de
  referência** e o **Noise Floor**, na mesma banda. O Noise Floor é o **ruído de referência** do SNR.
- **Exportação.** O Noise Floor é exportado como **um escalar em dBFS**, acompanhado, nos metadados, da
  **banda** (20 Hz–20 kHz) e da **condição** (silêncio, sem ponderação). A unidade é **homogênea** com as
  demais métricas de nível, permitindo composição direta (subtrações) no pacote de evidência.
- **Trust Ladder (confiança da medição).** O Noise Floor, por meio do SNR, condiciona a **confiança** da
  caracterização: uma medição cujo sinal **não se eleva suficientemente acima do Noise Floor** não pode
  ser classificada como confiável. Ele é, assim, um dos fundamentos da porta inferior de confiança.

## 7. Propriedades que a definição deve preservar

- **Comparabilidade.** O valor deve ser comparável entre **sessões, instrumentos e dispositivos sob
  teste**, desde que a **banda e a ausência de ponderação** sejam declaradas e idênticas (o que aqui é
  garantido por serem fixas: 20 Hz–20 kHz, plana).
- **Estabilidade.** Dado tempo de observação suficiente, o valor deve ser **estável** (baixa dispersão) e
  não deve depender de **parâmetros de resolução da observação** — apenas da banda e do ruído físico
  efetivamente presente.
- **Rastreabilidade.** O valor deve ser **auditável**: referenciado ao fundo de escala, com banda e
  condição de medição explicitamente declaradas junto ao número.
- **Coerência dimensional.** Deve ser um **nível em dBFS**, homogêneo com MS-09, MS-10 e a Trust Ladder,
  de modo que subtrações entre níveis permaneçam fisicamente válidas.
- **Reprodutibilidade.** Sob a mesma condição de medição, o Noise Floor deve produzir o **mesmo valor**
  (comportamento determinístico).
- **Neutralidade de escopo.** Deve representar **apenas** ruído dentro da banda — nunca distorção, sinal
  intencional ou energia fora da banda, cuja avaliação pertence a outras camadas.

---

> **Referência de embasamento:** `docs/laboratory/metrics-audit.md`, Apêndice A (estudo comparativo das
> definições de Noise Floor). A ratificação desta especificação corresponde à decisão **N1** e habilita
> a especificação completa do MS-08.
