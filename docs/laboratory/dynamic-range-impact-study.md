# Estudo de Impacto Arquitetural Pré-Decisão — MS-09 (Dynamic Range)

> **Natureza:** estudo de impacto. **Não é** Registro de Decisão. **Não escolhe** alternativa, **não
> recomenda**. Mapeia apenas as **consequências arquiteturais** de cada opção do estudo conceitual.
> **Base:** [`dynamic-range-definition.md`](./dynamic-range-definition.md),
> [`dynamic-range-conceptual-study.md`](./dynamic-range-conceptual-study.md), auditoria do MS-09
> ([`metrics-audit.md`](./metrics-audit.md) Apêndice C), e a normativa ratificada (Noise Floor + AR-01).
>
> **Famílias de critério de teto** (do estudo conceitual): **Saturação** (A início do clipping / D maior
> RMS sem clipping / E fundo de escala fixo) · **Linearidade** (B último nível linear / F 1 dB compressão)
> · **Distorção** (C limiar THD/THD+N).

---

## Parte 1 — Impacto do critério de teto

Impacto de cada **família** sobre cada componente. Apenas registro dos impactos.

| Componente | **Saturação (A/D/E)** | **Linearidade (B/F)** | **Distorção (C)** |
|---|---|---|---|
| **Noise Floor** | Sem impacto (NF independente) | Sem impacto | Sem impacto |
| **Headroom** | Sobreposição conceitual — mesmo evento de plena escala/clipping (E usa FS nominal) | Distinto do Headroom | Distinto do Headroom |
| **SNR** | Sem impacto direto (SNR usa tom de ref. + NF) | Sem impacto direto | Sem impacto direto |
| **THD** | Nenhum (não invoca distorção) | Proximidade conceitual (avaliar não-linearidade), sem acoplamento ao THD | **Acoplamento** — teto definido por limiar de distorção |
| **THD+N** | Nenhum | Baixo | **Acoplamento direto** (se o limiar for em THD+N) |
| **IMD** | Nenhum | Baixo | Acoplamento possível (se o limiar considerar IMD); caso contrário baixo |
| **Exportação** | Escalar dB autocontido; metadados mínimos (A/D); E declara "teto = FS nominal" | Requer declarar critério/limiar de linearidade nos metadados | Requer declarar a métrica de distorção e o limiar; acopla o DR exportado à distorção |
| **Trust Ladder** | Alinhado à lógica de clipping/Headroom já existente | Exige interpretação nova (span por linearidade) | Vincula a confiança à caracterização de distorção |
| **Comparabilidade** | A/D sensíveis a crest/sinal de teste; **E** máxima (teto constante) mas não caracteriza o DUT | Depende do limiar; mais robusto a crest | Alta e padronizada (padrão de facto) se o limiar for declarado |
| **Independência entre métricas** | Preservada; leve proximidade conceitual ao Headroom | Reduzida (acopla a avaliação de linearidade/ganho) | Mais reduzida (acopla explicitamente a THD/THD+N) |

---

## Parte 2 — Impacto da escolha de banda

| Consequência | **Teto em banda irrestrita** | **Teto limitado a 20 Hz–20 kHz** |
|---|---|---|
| **Coerência normativa** | Tensão — reimporta, pelo teto, energia fora de banda que o Noise Floor exclui | Alinhada — mesma banda que o Noise Floor |
| **Coerência dimensional** | Subtração em dB formalmente válida, porém grandeza **híbrida** (não única) | Grandeza única (razão de potências em banda) |
| **Comparabilidade** | Reduzida — valor depende do conteúdo fora de banda | Maior — independe do conteúdo fora de banda |
| **Dependência do conteúdo fora de banda** | **Sim** (DC/subsônico/ultrassônico entram no teto) | Não |
| **Compatibilidade com a definição do Noise Floor** | Divergente (NF é band-limited; teto irrestrito quebra a premissa "mesma banda") | Compatível |
| **AR-01** | Em geral **não acionada** pelo teto: um teto de banda larga não passa pelo pipeline espectral | **Herda** a precondição de AR-01 — limitar o teto à banda envolve estimativa de energia em banda, sujeita à validação ENBW/Parseval |
| **Futuras métricas espectrais** | Estabelece precedente de mistura de bandas — risco de propagar incoerência | Estabelece precedente consistente (todos os níveis na mesma banda) |

> *Trade-off central registrado:* o teto irrestrito **evita** a precondição AR-01 mas **rompe** a coerência
> de banda com o Noise Floor; o teto em 20–20 k **restaura** a coerência mas **herda** a precondição AR-01.

---

## Parte 3 — Impacto da degeneração (`teto ≤ piso`)

| Impacto sobre | **Extensão nula (0)** | **Não caracterizável** | **Valor negativo diagnóstico** | **Distinção `=` vs `<`** |
|---|---|---|---|---|
| **Exportação** | Número 0 válido; ambíguo com um "0 dB" real | Nulo/ausente explícito; consumidores tratam ausência | dB negativo assinado; carrega diagnóstico, não é "faixa" | Dois sub-estados (limiar zero vs falha); metadados mais ricos |
| **Interface** | Exibe 0 dB (ambíguo) | Exibe N/A (claro que não mensurável) | DR negativo pode confundir (faixa não é negativa) | Apresentação diferenciada (limiar vs inválido) |
| **Trust Ladder** | 0 não sinaliza falha por si | Sinaliza ausência de faixa útil → pode reduzir confiança | Negativo é forte indicador de falha | Lógica de confiança mais fina possível |
| **Classificação de qualidade** | Reportado como valor (Normal) — não sinaliza degeneração | Mapeia a `NotApplicable` — alinhado à taxonomia | Ambíguo (Normal? flag?) | `=` → limiar; `<` → NotApplicable/Error |
| **Processamento posterior** | Numericamente seguro; semanticamente ambíguo | Requer tratamento de nulo; sem número espúrio | Requer tratar negativos como caso especial | Máxima informação, máxima complexidade |

---

## Parte 4 — Matriz de impacto

Níveis: **sem impacto · baixo · moderado · alto**. Na coluna **Compatibilidade normativa**, o nível
indica o *grau de perturbação* à compatibilidade (sem impacto = plenamente alinhado; alto = divergente).
Sem justificar preferências.

| Alternativa | Noise Floor | SNR | Headroom | Trust | Export | Compatibilidade normativa |
|---|---|---|---|---|---|---|
| **Teto — Saturação (A/D)** | sem impacto | sem impacto | moderado | baixo | baixo | sem impacto |
| **Teto — Fundo de escala fixo (E)** | sem impacto | sem impacto | baixo | baixo | baixo | alto |
| **Teto — Linearidade (B/F)** | sem impacto | sem impacto | baixo | moderado | moderado | moderado |
| **Teto — Distorção (C)** | sem impacto | sem impacto | baixo | moderado | alto | alto |
| **Banda — irrestrita** | sem impacto | baixo | sem impacto | baixo | baixo | alto |
| **Banda — 20–20 k** | sem impacto | sem impacto | sem impacto | sem impacto | baixo | sem impacto |
| **Degeneração — extensão nula** | sem impacto | sem impacto | sem impacto | baixo | moderado | baixo |
| **Degeneração — não caracterizável** | sem impacto | sem impacto | sem impacto | moderado | moderado | sem impacto |
| **Degeneração — negativo diagnóstico** | sem impacto | sem impacto | sem impacto | moderado | alto | moderado |
| **Degeneração — distinção `=`/`<`** | sem impacto | sem impacto | sem impacto | moderado | alto | baixo |

---

## Parte 5 — Restrições arquiteturais (válidas independentemente da decisão)

Restrições que **decorrem da arquitetura atual** e permanecem válidas qualquer que seja a escolha — **não
são novas**, apenas identificadas:

- **Mesma referência.** Teto e piso devem compartilhar a mesma referência (dBFS, referenciado a RMS) para
  que a subtração seja fisicamente válida.
- **Coerência dimensional.** O Dynamic Range deve permanecer uma **diferença de dois níveis homogêneos**,
  resultando em dB.
- **Independência do estimador.** Herdada da proposta normativa (e do Noise Floor): a grandeza é
  independente do algoritmo; qualquer método é apenas um estimador.
- **Compatibilidade com a normativa existente.** O DR consome o Noise Floor **ratificado** (limite
  inferior) e permanece sujeito ao seu escopo (banda, exclusão de fora de banda).
- **AR-01.** Qualquer reúso de **potência espectral de banda larga** (p.ex. um teto band-limited via
  pipeline espectral) permanece condicionado à validação ENBW/Parseval — restrição já vigente.
- **Herança do Noise Floor (MS09-A5).** O valor absoluto do DR carrega o offset constante do piso
  enquanto AR-01 não for cumprida — a considerar, mas já governado por AR-01.
- **Não-negatividade conceitual da "faixa".** Uma faixa dinâmica é, por identidade, uma extensão acima do
  ruído — restrição conceitual que qualquer tratamento de degeneração deve respeitar.

---

## Parte 6 — Preparação para o Registro de Decisão

Apenas o mapa das dependências e dos impactos a considerar (sem responder nada):

- **Decisões independentes:**
  - **D-Teto** (critério de teto) — fundacional; não depende de outra decisão do MS-09.
- **Decisões que condicionam outras:**
  - **D-Teto → D-Banda** — a noção de "banda do teto" só se define após fixado o critério (um teto por
    pico/clipping é de banda larga por natureza; um teto por distorção é medido sobre uma banda de análise).
  - **D-Teto + D-Banda → D-Degeneração** — se e quando `teto ≤ piso` ocorre, e o que significa, depende do
    critério e da banda.
  - Ordem lógica: **D-Teto → D-Banda → D-Degeneração**.
- **Impactos a considerar explicitamente no Registro de Decisão:**
  1. **Independência vs. comparabilidade** no critério de teto: a família **Distorção** oferece
     comparabilidade padronizada mas **acopla** o DR a THD/THD+N; a família **Saturação** preserva a
     independência mas é **sensível ao fator de crista**; **Linearidade** fica entre as duas.
  2. **Trade-off de banda**: teto **20–20 k** restaura a coerência com o Noise Floor mas **herda AR-01**;
     teto **irrestrito** evita AR-01 mas rompe a coerência de banda.
  3. **Degeneração**: impacto do tratamento escolhido sobre **exportação**, **taxonomia de qualidade** e
     **Trust Ladder** (ambiguidade do 0/negativo vs clareza do "não caracterizável").
  4. **Sobreposição conceitual com o Headroom** na família Saturação.
  5. **Offset ENBW herdado do Noise Floor (MS09-A5)** — presente em qualquer cenário, governado por AR-01.

Estas dependências e impactos completam a cadeia metodológica do MS-09 (Especificação → Auditoria →
Estudo Conceitual → Estudo de Impacto) e servirão de base ao futuro **Registro de Decisão**.
