# Regressão browser-native — máquina-alvo

Testes que exercitam o **JavaScript produtivo** do `resources/aamp_laboratory.html`.

> [!IMPORTANT]
> **Não fazem parte da suíte headless do CTest.** O projeto não possui runner JavaScript headless — sem Node, sem engine vendorizado (`duktape`/`quickjs`/`v8`) — e acrescentar um seria dependência e componente de build novos, fora do escopo dos itens que estes testes cobrem. A limitação é conhecida e deliberada: estes casos são de **máquina-alvo**, executados no browser ou no WebView, e sua evidência é a saída do console.

Nenhum arquivo aqui reimplementa lógica de produto. Todos invocam as funções reais já carregadas pela página; se a implementação for perturbada, o teste diverge e falha.

## Como executar

1. Abrir `resources/aamp_laboratory.html` no browser (ou no WebView2 do `AAMP.exe`).
2. Abrir o console de desenvolvedor.
3. Colar o conteúdo do arquivo `.js` desejado e executar.

O teste imprime uma linha por caso e um total. Em caso de divergência, **lança exceção** com o resumo — falha silenciosa não é possível.

## Casos

| ID | Arquivo | Cobre |
|---|---|---|
| **TC-057** | `tc057_trust_ladder.js` | Trust Ladder: porta inferior do MS-10 e precedência de integridade (P1-008 / V1) |
| **TC-058** | `tc058_scalar_presentation_contract.js` | Apresentação honra o contrato do escalar nas três rotas de leitura (P2-001 / DV-8) |
| **TC-059** | `tc059_normative_applicability.js` | Coerência entre aplicabilidade declarada e apresentação normativa (P2-002 / F3) |
| **TC-064** | `tc064_semantic_honesty.js` | Honestidade semântica: rótulo de V3, placeholder de V4 e asserções de fase (AAMP-HG-20260811-001) |

> [!NOTE]
> Os identificadores `TC-060`…`TC-063` **já pertencem à suíte C++** (orquestração: fluxo automático, descarte, cancelamento, degradação). O espaço de IDs é **único** entre CTest e regressão browser — daí este caso ser **TC-064**.

### TC-057 — o que protege

- porta inferior `SNR_dB ≤ 0` → `bad`, incluindo o limite exato em `0`;
- estado da **família** MS-10 governando antes do escalar (`Error`, `NotApplicable`);
- **consumo exclusivo da família** — um `SNR_dB` publicado no índice global por outra família não alimenta a porta;
- `valuePresent=false`, `NaN` e `±Infinity` tratados como inutilizáveis, nunca como "SNR baixo";
- sentinela `−300` nunca reconhecida pelo valor, só por `quality`/`valuePresent`;
- porta superior (clipping, pico crítico) preservada e dominante;
- agregado `lowConf` preservado como ressalva `warn`, inclusive periférico;
- `bad` dominando `warn`;
- **MS-10 ausente preservando o comportamento anterior** da escada.

### TC-058 — o que protege

- **as três rotas** de leitura apresentacional convergem em `scalarForDisplay`: `val()` global, o `val` local de `scanAnomalies()` e `LAB.getMetric()`;
- `valuePresent !== true`, `NotApplicable` e `Error` nunca produzem número;
- `null`, `NaN` e `±Infinity` nunca produzem número;
- `LowConfidence` com valor presente **continua** exibido — a ressalva vive na Trust Ladder, não na leitura;
- **a sentinela nunca é reconhecida pelo valor**: `−300` legitimamente `Normal`/presente permanece visível;
- `getMetricObj()` permanece intacto, para quem precisa do contrato completo.

> [!NOTE]
> O discriminante do TC-058 é comparar o **mesmo** `−300` sob contratos diferentes. Com contrato válido, `scanAnomalies` gera *"SNR baixo −300.0 dB"*; com `NotApplicable`, gera `[]`. Valor idêntico, comportamento distinto — prova que quem governa é `quality`/`valuePresent`, não o número. `scalarForDisplay` é `const`, então não pode ser mutada em memória; este discriminante substitui a prova de mutação.

### TC-059 — o que protege

- com `hasAnalysis === false`, **SNR** e **Consistência** não aparecem na aba Integridade — nem como chip nem como gauge — mesmo com escalares numericamente válidos no contrato;
- o instrumento **Ruído** não reapresenta Piso, SNR ou Faixa Dinâmica como resultados normativos;
- os **descritores brutos** da captura — headroom, crista, clipping, DC — permanecem visíveis, por serem fatos diretos do sinal;
- com `hasAnalysis === true`, a apresentação anterior é integralmente preservada;
- o contrato do **P2-001** continua vigente dentro da sessão normativa: `NotApplicable` segue oculto;
- **a navegação concorda com o builder**: `hasData('noise')` e `domLed('noise')` exigem `hasAnalysis`, para o LED não acender `ok` enquanto o instrumento mostra "sem dados";
- ausência real de dados não lança exceção.

> [!NOTE]
> A prova de dentes é o discriminante: os casos A e B têm escalares **idênticos** e diferem apenas em `hasAnalysis`. No comportamento anterior os dois exibiriam os valores; hoje divergem. Remover o gate faz `E1` (renderização) ou `E2` (navegação) falhar.

### TC-064 — o que protege

Ratificado em **AAMP-HG-20260811-001** (alternativa B).

- **V3** — chip e gauge exibem *"Variação"* / *"Variation"*, nunca *"Consistência"* / *"Consistency"*, e o **valor do CV aparece sem transformação** (não vira `100 − CV`);
- **V4** — o contrato **emitido pelo `LabEngine.run()` produtivo** declara `NotApplicable` + `valuePresent = false`, conservando o valor bruto `100` e sem criar `EstimateNature` novo; a família MS-15 **continua `Normal`** e `AggregateLinearity_dB` continua sendo medição válida;
- **V4** — `scalarForDisplay()` rejeita o placeholder, e o **mesmo** `100` sob contrato válido **seria** exibido — quem governa é `quality`/`valuePresent`, não o número;
- **fase** — nenhuma superfície afirma *"coincide em fase"*, *"fase mínima"* ou correção por EQ como conclusão demonstrada, em pt-BR ou em inglês;
- **fase** — a descrição neutra carrega o **ripple observado** em graus.

> [!IMPORTANT]
> Os limiares de fase (`> 25°` na janela `0,7–1,4×`; `> 20°` na janela `0,8–1,25×`) permanecem **NÃO RATIFICADOS** e **INALTERADOS** — vão para o estudo normativo V5/DV-4. O teste prova que **só a afirmação mudou**, e prova isso **na fronteira exata**, não por aproximação.

#### Como a fronteira é fixada sem depender do detector

A curva de fase é um **degrau de 0 para `V` exatamente em 1 kHz**, o centro do pico. Assim, **qualquer** janela que atravesse o centro tem ripple **igual a `V`**, independentemente de qual janela o código produtivo escolha (`pk.fL…pk.fR` ou `0,7–1,4×`). As comparações produtivas são **estritas**, então:

Como a comparação produtiva é **estrita** (`ripple > L`), o par de casos abaixo determina `L` **sem intervalo de folga**:

| Caso | `V` | Esperado | O que prova |
|---|---|---|---|
| `F5` | **25** | **sem** sufixo | `L ≥ 25` |
| `F6` | **`nextUp(25)`** | sufixo | `L < nextUp(25)` |
| `F7` | — | os dois acima | **`L === 25`** |
| `G5` | **20** | **sem** correlato | `L ≥ 20` |
| `G6` | **`nextUp(20)`** | correlato | `L < nextUp(20)` |
| `G7` | — | os dois acima | **`L === 20`** |
| `F1`/`F2` | 25,5 | sufixo com `26°` | arredondamento textual |
| `G1`…`G4` | 20,5 | correlato com `21°` | arredondamento textual |

`nextUp(x)` é o **próximo `double` representável** — obtido incrementando em 1 ULP o padrão de bits IEEE-754. Como **não existe `double` entre `x` e `nextUp(x)`**, as duas desigualdades juntas fixam o literal **exatamente**, e não um intervalo.

Duas precondições sustentam isso: `rangeStats` mede **exatamente** `V` nas duas janelas (igualdade estrita), e `25`/`20` e seus `nextUp` são de fato **adjacentes** — verificado pelo ponto médio, que para dois `double` adjacentes não é representável e arredonda para um dos extremos.

**Controles negativos executados**, não hipotéticos:

| Alteração no literal produtivo | Checks derrubados |
|---|---|
| `25 → 30` | `F1`, `F2`, `F6`, `F7` |
| `25 → 24` | `F5`, `F7` |
| **`25 → 25,25` e `20 → 20,25`** | **`F6`, `F7`, `G6`, `G7`** |

O terceiro é o decisivo: um deslocamento de **um quarto de grau** — que a versão anterior deste teste deixava passar — derruba o caso. A primeira versão comparava uma rampa muito acima do limiar contra fase zero e provava apenas que **existe** uma fronteira em algum lugar; a segunda fixava o intervalo `[25 , 25,5)`; só a atual fixa o **valor**.
