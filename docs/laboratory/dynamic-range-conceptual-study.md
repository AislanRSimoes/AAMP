# Estudo Conceitual Pré-Decisão — MS-09 (Dynamic Range)

> **Natureza:** estudo conceitual. **Não é** auditoria (isso é o Apêndice C de `metrics-audit.md`) **nem**
> registro de decisão. Fornece a fundamentação técnica — *quais são as opções normativas e suas
> implicações* — para que o **Registro de Decisão do MS-09** seja elaborado posteriormente.
> **Não contém recomendações, decisões, nem discussão de implementação.**
>
> **Escopo:** exclusivamente os achados **MS09-A1** (critério de teto), **MS09-A2** (coerência de banda)
> e **MS09-A4** (degeneração). A3, A5, A6 e A7 estão **fora** desta etapa.
> **Referências de contexto:** proposta normativa [`dynamic-range-definition.md`](./dynamic-range-definition.md);
> auditoria [`metrics-audit.md`](./metrics-audit.md) Apêndice C.

---

## Parte 1 — Critério de teto (MS09-A1)

Comparação conceitual das definições normativas possíveis de "teto". **Sem recomendação.**

| Critério | Significado físico | Vantagens | Limitações | Comparabilidade | Reprodutibilidade | Compat. c/ proposta |
|---|---|---|---|---|---|---|
| **A — Início do clipping** (pico atinge o fundo de escala) | Maior nível cujo pico não ultrapassa a plena escala; acima disso há truncamento/saturação numérica | Objetivo, binário, determinístico; independe de análise espectral | É critério de **pico** → sensível ao fator de crista; não distingue saturação numérica de distorção analógica; nada diz sobre linearidade abaixo do clipping | Alta, **se** o tipo de sinal/crest for padronizado | Alta (evento determinístico) | **Compatível** (a proposta adota "início da saturação/clipping") |
| **B — Último nível linear** (antes da não-linearidade) | Maior nível em que a saída ainda é proporcional à entrada, antes de compressão/limitação | Fisicamente significativo para "faixa útil sem distorção"; capta compressão suave que o critério de pico não vê | Exige definir "linearidade" e um limiar de desvio; não-linearidade gradual não tem ponto único | Depende do limiar de linearidade adotado; comparável se padronizado | Moderada (depende do método de detecção de desvio) | **Parcial** — redefiniria o teto (desvio de linearidade ≠ clipping) |
| **C — Limiar de distorção** (ex.: X% THD/THD+N) | Nível em que a distorção atinge um valor especificado (convenção clássica de conversores) | Padrão de facto (AES17); vincula o teto à qualidade real do sinal; robusto a soft-clipping | Acopla o DR à medição de distorção; depende do limiar (0.5%, 1%…); cria dependência entre métricas | Alta e padronizada **se** o limiar for declarado | Alta com limiar fixo | **Divergente** — a proposta exclui explicitamente o critério por distorção |
| **D — Maior nível sem clipping, medido por RMS** (variante de A) | Entre os níveis que não clipam, toma-se o de maior RMS; o teto é o "maior nível utilizável sem saturação", como nível RMS | Combina o *gate* de clipping com uma medida de **nível** (coerente com o piso, também RMS); dá um "nível" ao teto, não só um evento | Herda a sensibilidade a crest de A; o valor RMS depende da forma do sinal de teste | Depende do sinal de teste; comparável se padronizado | Alta | **Compatível** (concretiza "início da saturação" como nível RMS) |
| **E — Fundo de escala nominal fixo** (0 dBFS) | Adotar a própria plena escala como teto de referência, independentemente do DUT; o DR vira ≈ −NoiseFloor | Máxima comparabilidade (teto constante); simples; é a "faixa dinâmica da cadeia de medição" | Não caracteriza o teto **real** do DUT; assume que o DUT alcança plena escala | Máxima (teto constante) | Máxima | **Divergente** — a proposta define teto como o **maior sinal medido** sem saturação, não como referência fixa |
| **F — Ponto de compressão de 1 dB** (convenção analógica) | Nível em que o ganho cai 1 dB vs. o regime linear; início prático da compressão | Padrão em amplificadores/RF; ligado à saturação suave | Exige varrer níveis e medir ganho; "1 dB" é convenção arbitrária; incomum em áudio digital | Padronizada se adotada | Moderada | **Divergente/Parcial** (critério de compressão, não de clipping digital) |

*Observação neutra:* os critérios dividem-se em duas famílias conceituais — **por saturação** (A, D, e no limite E) e **por qualidade/linearidade** (B, C, F). A escolha entre famílias é matéria do Registro de Decisão (Parte 5), não deste estudo.

---

## Parte 2 — Coerência de banda (MS09-A2)

**Questão conceitual:** *um Dynamic Range continua representando uma única grandeza física quando o teto
e o piso são obtidos em bandas diferentes?* (Análise puramente conceitual; sem implementação.)

- **Homogeneidade dimensional.** `DR = teto − piso`, ambos expressos como níveis em dBFS. Formalmente,
  `dBFS − dBFS = dB` **independentemente da banda** de cada termo. Ou seja, a subtração é
  **dimensionalmente expressável** mesmo com bandas distintas. Porém homogeneidade dimensional **não
  implica** significado físico único.
- **Coerência metrológica.** Uma faixa dinâmica fisicamente bem-definida é a razão entre **duas potências
  da mesma grandeza sobre o mesmo suporte de frequência**. Se o teto abrange uma banda **irrestrita**
  (inclui energia fora de 20–20 k) e o piso é **restrito** a 20–20 k, os dois níveis medem potência sobre
  **domínios de frequência diferentes**. A razão resultante é **híbrida**: `(potência de sinal em banda
  larga) / (potência de ruído em banda)`. Metrologicamente, isso **não** é uma única grandeza coerente —
  é a razão de duas quantidades definidas em suportes distintos.
- **Comparabilidade.** Com o teto em banda irrestrita, o valor passa a **depender do conteúdo fora de
  banda** (do sinal de teste e da cadeia) que o piso deliberadamente ignora. Dois sistemas com
  comportamento **idêntico em banda** mas conteúdo fora de banda diferente produziriam **DR diferentes** —
  o que enfraquece a comparabilidade. Um teto na mesma banda restauraria essa propriedade.
- **Relação com a definição normativa do Noise Floor.** O Noise Floor ratificado é **explicitamente
  band-limited** (20–20 k) e **exclui** energia fora de banda (delegada à camada de Integridade). Para que
  `DR = teto − Noise Floor` seja coerente com essa definição, o teto precisaria viver **na mesma banda**;
  caso contrário, o DR **reimporta silenciosamente**, pelo teto, a energia fora de banda que a
  especificação do Noise Floor **deliberadamente excluiu** — uma tensão interna com a norma já ratificada.

**Conclusão conceitual (não é decisão):** um DR com teto e piso em bandas distintas é *dimensionalmente*
expressável em dB, mas **não representa uma única grandeza física bem-definida** — é uma razão híbrida
cujo valor depende de conteúdo fora de banda. Se isso é aceitável (e sob quais condições) é uma questão
normativa, formulada na Parte 5.

---

## Parte 3 — Degeneração (MS09-A4): caso `teto ≤ piso`

Análise conceitual do caso em que o maior sinal utilizável **não excede** o piso de ruído.

- **É um valor físico?** Uma "faixa dinâmica" é, por construção, a **extensão útil acima do ruído**. Se o
  teto é ≤ piso, essa extensão é **nula ou inexistente**. Um número **negativo** não representa uma
  "faixa" (uma extensão não pode ser negativa) — representaria, no máximo, um *déficit* de sinal em
  relação ao ruído, que é uma informação de **outra natureza**, não uma faixa dinâmica.
- **É uma condição inválida?** Não é um "erro de cálculo": a operação é bem-definida. É a **ausência da
  premissa física** da métrica — não há sinal mensuravelmente acima do ruído. A condição sinaliza que o
  pressuposto da grandeza (existir margem útil) **não foi satisfeito**.
- **Deve ser "não caracterizável"?** Conceitualmente, `teto ≤ piso` significa que o sistema/medição **não
  possui faixa dinâmica útil**; a grandeza não é caracterizável como uma extensão positiva. Trata-se,
  portanto, de um candidato natural a **condição não caracterizável** — sem que este estudo decida o
  comportamento.
- **Interpretações alternativas** (todas conceitualmente admissíveis; a escolha é do Registro de Decisão):
  1. **Extensão nula** — a faixa é tratada como zero ("sem margem útil").
  2. **Não caracterizável / indefinida** — a premissa física está ausente; a métrica não se aplica.
  3. **Número assinado (déficit)** — reportar o valor negativo literal, válido apenas como **diagnóstico**
     (sinal abaixo do ruído), nunca como "faixa".
  4. **Distinção de fronteira** — separar `teto = piso` (extensão exatamente nula, caso-limite) de
     `teto < piso` (sinal abaixo do ruído, falha de premissa), tratando-os de formas distintas.

---

## Parte 4 — Dependência entre decisões

| Decisão | Depende de |
|---|---|
| **D-Teto** — critério de teto (Parte 1) | — (fundacional) |
| **D-Banda** — banda do teto vs. banda do Noise Floor (Parte 2) | **D-Teto** |
| **D-Degeneração** — caracterização de `teto ≤ piso` (Parte 3) | **D-Teto**, **D-Banda** |

**Encadeamento lógico:**
- **D-Banda depende de D-Teto** — a própria noção de "banda do teto" muda conforme o critério: um teto por
  **pico/clipping** (A/D) é, por natureza, uma grandeza de banda larga; um teto por **distorção** (C) é
  medido sobre uma banda de análise. A pergunta de banda só se define **depois** de fixado o critério.
- **D-Degeneração depende de D-Teto e D-Banda** — se e quando `teto ≤ piso` ocorre, e o que isso
  significa, depende de como teto e piso são definidos (critério + banda). Um teto de banda larga tende a
  ser numericamente maior (menos propenso à degeneração) que um teto restrito à banda.

Ordem lógica de deliberação: **D-Teto → D-Banda → D-Degeneração.**

---

## Parte 5 — Questões para o Registro de Decisão

Apenas **formuladas** (não respondidas):

1. **Critério de teto (D-Teto):** qual definição normativa de teto o AAMP adotará — início do clipping,
   último nível linear, limiar de distorção, ou outro critério (Parte 1)? Deve o teto ser expresso como
   um **evento** (onset) ou como um **nível** (RMS/pico) do maior degrau válido?
2. **Banda do teto (D-Banda):** o teto deve ser tomado **exatamente na mesma banda** do Noise Floor
   (20–20 k) — preservando a exclusão de energia fora de banda já ratificada — ou pode permanecer em banda
   irrestrita, assumindo-se conscientemente a razão híbrida (Parte 2)?
3. **Degeneração (D-Degeneração):** como caracterizar formalmente a condição `teto ≤ piso` — extensão
   nula, não caracterizável, número assinado (diagnóstico), ou distinção entre `teto = piso` e
   `teto < piso` (Parte 3)?

Estas três questões, na ordem lógica da Parte 4, constituem a base do futuro **Registro de Decisão do
MS-09**.
