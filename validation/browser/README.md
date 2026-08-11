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
