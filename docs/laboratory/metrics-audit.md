# Auditoria Técnica das Métricas do Laboratório — AAMP

> **Fase atual: AUDITORIA (consolidação da definição técnica).**
> Este documento cataloga, para cada indicador exibido pelo Laboratório, seus **objetivo,
> fórmula, unidade, domínio de validade, referência, limites normativos e aplicabilidade**.
> É a base para: **Pilar 1** (validar a implementação), **Pilar 2** (apresentação normativa ao
> usuário) e **Pilar 3** (congelar o formato de exportação). Nenhuma métrica deve ser alterada,
> validada ou exportada antes que sua definição aqui esteja aprovada.
>
> **Fonte-verdade do documento** (extraído do código real, não de memória):
> - `src/analysis/AnalysisEngine.cpp` — implementação normativa C++ (MS-01…MS-15).
> - `resources/aamp_laboratory.html` — reimplementação JS "LabEngine" + camada de apresentação/confiança.
> - `src/common/Constants.h` — constantes normativas.
>
> **Aviso de duplicidade:** as métricas espectrais existem em **duas implementações paralelas**
> (C++ `AnalysisEngine` e JS `LabEngine`). Divergências entre elas são um risco estrutural
> registrado na Seção D (achado V6).

---

## Legenda

**Estados de qualidade** (`Quality`, `AnalysisEngine.cpp:1039`):
- `Normal` — métrica computada dentro do domínio de validade.
- `LowConfidence` — computada, mas com pré-condição científica frágil (ex.: regime abaixo do piso).
- `NotApplicable` — não faz sentido para a configuração (ex.: Crosstalk em Mono).
- `Error` — estímulo obrigatório ausente; métrica não caracterizável.

**Convenções**: `fs` = taxa de amostragem (48 kHz, `Constants.h:12`); `FFT_ANALYSIS` = 65536
(`Constants.h:18`); banda oficial = **20 Hz – 20 kHz** (`BAND_LOW_HZ`/`BAND_HIGH_HZ`,
`Constants.h:21-22`); 0 dBFS ≡ 1.0 (`FULL_SCALE`, `Constants.h:13`); nível de calibração
= **−12 dBFS** (`CALIB_DBFS`, `Constants.h:41`).

---

## Seção A — Métricas de INTEGRIDADE (nível de captura, por estímulo)

Estas respondem *"posso confiar nesta captura como evidência, antes de interpretar as demais
métricas?"*. Derivam diretamente das amostras do `.wav` de cada captura, **sem depender da análise
normativa**. Fonte: `captureIntegrity()` — `aamp_laboratory.html:777-780`.

| Indicador | Objetivo | Fórmula | Unidade | Domínio de validade | Referência | Limites normativos | Aplicabilidade |
|---|---|---|---|---|---|---|---|
| **Integridade** (agregado) | Confiança da captura como evidência | Composição de Headroom + Crista + Clipping + DC + Silêncio (+ SNR e Consistência puxados da análise) | — | Requer `c.wav` com amostras | `html:2709-2721` | Escada de confiança (abaixo) | Toda captura |
| **Headroom** | Margem até o fundo de escala (risco de clipping) | `peakDb = 20·log10(max\|s\|)`; headroom = `−peakDb` abaixo de 0 dBFS | dBFS (pico) | Requer amostras | `html:777-780, 2243-2244, 2717` | `peakDb > −1` → **crítico**; `−6 < peakDb ≤ −1` → **baixo**; `≤ −6` → **ok** | Toda captura |
| **Crista** (crest factor) | Relação pico/RMS (dinâmica do sinal) | `crest = peakDb − rmsDb`, com `rmsDb = 20·log10(√(Σs²/n))` | dB | Requer amostras | `html:780` | *Informativo — sem banda definida* | Toda captura |
| **Clipping** | Amostras saturadas | `clip = #{ \|s\| ≥ 0.999 }` | contagem | Requer amostras | `html:778, 2202` | `clip > 0` → **comprometida** | Toda captura |
| **DC offset** | Deslocamento de nível | `dc = Σs/n` | fração FS | Requer amostras | `html:778-780, 2717` | `\|dc\| > 1e-3` → **atenção** | Toda captura |
| **Silêncio** (silPct) | Fração do buffer sem sinal | `silPct = 100·#{ \|s\| < 1e-4 }/n` | % | Requer amostras | `html:778, 780` | ⚠️ **calculado mas NÃO usado** na confiança (achado V1) | Toda captura |

### Escada de confiança (Confiança da medição)

Rótulos (`html:648, 690`): `Sem caracterização` · `Comprometida` · `Atenção` · `Externa` · `Confiável`.
Estados CSS: `idle / bad / warn / ok` (+ `ext`).

Lógica atual (`html:2226`):
```js
trust = (clipTot || peakDb > −1)            ? 'bad'
      : ((peakDb > −6) || lowConf)          ? 'warn'
      : (q ? 'ok' : 'idle');
```
- **Objetivo**: rótulo único de confiabilidade da medição, condicionando a leitura das demais métricas.
- **Domínio de validade / limitação**: só testa o **teto** (clipping / sinal quente) e `lowConf`.
  **Não há porta inferior** — captura silenciosa / sem sinal / SNR baixíssimo cai em `'ok'` = "Confiável".
  Ver achado **V1** (Seção D).

---

## Seção B — Métricas de ANÁLISE normativa (MS-01…MS-15)

Fonte primária: `AnalysisEngine::computeAllMetrics` (`AnalysisEngine.cpp:1017-1037`). Cada métrica
exige um estímulo específico; ausência ⇒ `Error`.

### Curvas espectrais (via transferência ESS por filtro inverso Tikhonov)

| ID | Nome | Objetivo | Fórmula (essência) | Unidade | Estímulo | Referência | Normativo | Aplicab. |
|---|---|---|---|---|---|---|---|---|
| **MS-01** | Frequency Response | Resposta em amplitude | `20·log10(\|H(f)\|)`, `H` = deconv. Tikhonov(Y,X) | dB × Hz | ST-01 ESS | `cpp:123-142` | *a definir (P2)* | Todas |
| **MS-02** | Phase Response | Fase (desembrulhada) | `arg(H(f))` unwrapped | ° × Hz | ST-01 ESS | `cpp:145-171` | *a definir* | Todas |
| **MS-03** | Group Delay | Atraso de grupo | `τ = −dφ/dω` | ms × Hz | ST-01 (via MS-02) | `cpp:174-192` | ripple **> ~1 ms** = audível (`html:1924`) | Todas |
| **MS-04** | Impulse Response | Resposta ao impulso | IR = `IFFT(H)`, normalizada (pico=1) | norm × ms | ST-01 ESS | `cpp:195-242` | *a definir* | Todas |

### Escalares de distorção (banda 20 Hz–20 kHz, FFT 65536, janela Blackman-Harris4)

| ID | Nome | Objetivo | Fórmula | Unidade | Estímulo | Referência | Normativo | Aplicab. |
|---|---|---|---|---|---|---|---|---|
| **MS-05** | THD | Distorção harmônica | `100·√(Σₙ₌₂¹⁰ Eₙ)/√(E₁)`, `Eₙ`=energia em ±4 bins de `n·f₀` (f₀=100/1k/10k) | % (e dB) | ST-03 PureSine | `cpp:413-507`; harm 2..10 = `Constants.h:55-56` | **LIMIAR 1%** (`html:2461,2466`) | Todas |
| **MS-06** | THD+N | Distorção + ruído | `100·√(E_banda − E₁)/√(E₁)` a 1 kHz | % (e dB) | ST-03 PureSine | `cpp:510-573` | **LIMIAR 1%** (`html:2468`) | Todas |
| **MS-07** | IMD | Intermodulação | `100·√(Σ E_produtos)/√(Σ E_tons)`; exclui produtos que coincidem com fundamentais (CR-IMD); 8 tons | % (e dB) | ST-02 MultiTone | `cpp:576-681` | **LIMIAR 1%** (`html:2470`) | Todas |

### Escalares de ruído / dinâmica

| ID | Nome | Objetivo | Fórmula | Unidade | Estímulo | Referência | Normativo | Aplicab. |
|---|---|---|---|---|---|---|---|---|
| **MS-08** | Noise Floor | Piso de ruído integrado | `10·log10(Σ_banda P̄(f))`, `P̄` média de frames Hann 4096 (16 frames / ou todos), 50% overlap | dBFS | ST-09 Silence | `cpp:684-746`; 16 frames = `Constants.h:59` | `docs/laboratory/noise-floor-definition.md` | Todas |
| **MS-09** | Dynamic Range | Faixa dinâmica útil | `20·log10(RMSmax_degrau_não-clipado) − NoiseFloor_dBFS` | dB | ST-08 + ST-09 | `cpp:749-776` | `NotApplicable` se todos os degraus clipados | Todas |
| **MS-10** | **SNR** | Relação sinal-ruído | `10·log10(ps/pn)`; `ps` = **potência da fundamental de 1 kHz retornada** (quadro centrado `N=4096`, Hann + ENBW/Parseval, `k0±2`) — **P1-006** (§O); `pn` = `10^(NoiseFloor_dBFS/10)`, piso oficial do MS-08 — **P1-005** (§N); ramo `pn≤0 → 200` preservado, **estruturalmente inalcançável** para operando válido | dB | ST-03 + ST-09 | `cpp:71-76, 779-791` | gauge exibido **0–120 dB** (`html:2721`); piso de retorno na captura **20 dB acima do floor** (`MIN_SNR_OVER_FLOOR_DB`, `Constants.h:47`) | Todas |
| **MS-11** | Crosstalk | Vazamento entre canais | `10·log10(p_inativo/p_ativo)` | dB | ST-01 (L+R) | `cpp:794-810` | *a definir* | **Só Estéreo** (`NotApplicable` em Mono) |
| **MS-12** | Latency | Latência round-trip | `(peakIdx − posição_impulso_emitido)/fs` | ms + samples | ST-06 Impulse | `cpp:813-827` | *a definir* | Todas |

### Escalares comportamentais (envelope, alinhados por correlação cruzada)

| ID / escalar | Objetivo | Fórmula | Unidade | Estímulo | Referência | Normativo | Semântica |
|---|---|---|---|---|---|---|---|
| **MS-13** SettlingTime | Tempo de assentamento | 1º índice com `\|env − final\| ≤ 5%·final` | ms | ST-07 Burst | `cpp:855-863` | *a definir* | menor = melhor |
| **MS-13** **RepetitionConsistency** (**"Consistência"**) | Repetibilidade de picos | `100·√(var)/mean` sobre 10 picos de 100 ms (**coef. de variação**) | % | ST-12 PulseTrain | `cpp:865-879`; `html:1069-1075` | *a definir* | ⚠️ **0% = perfeito; maior = pior** — ver achado V3 |
| **MS-13** Drift | Deriva de nível | `20·log10(e₃₀/e₁)` (env @1 s vs @29 s) | dB | ST-13 Sustained | `cpp:881-889` | *a definir* | ~0 = estável |
| **MS-13** LevelTransfer | Ganho por degrau + erro | ganho `20·log10(ro/ri)` por degrau; `maxError = max\|dbₖ − média\|` | dB | ST-08 | `cpp:891-914` | *a definir* | maxError baixo = linear |
| **MS-14** AttackTracking_hard/soft | Fidelidade de ataque | `100·Pearson(env_in, env_out)` alinhado por lag | % | ST-10 (/ST-11) | `cpp:920-961` | *a definir* | 100% = segue perfeito |
| **MS-14** AttackConsistency | Coerência hard×soft | `max(0, 100 − \|hard − soft\|)` | % | ST-10+ST-11 | `cpp:956-957` | *a definir* | 100% = coerente |
| **MS-14** Overshoot | Sobressinal do ataque | `20·log10(pk/regime)`; regime = env média 100–300 ms pós-pico | dB | ST-10 | `cpp:963-985` | `LowConfidence` se regime < −60 dB do pico | maior = mais overshoot |
| **MS-15** AggregateLinearity | Linearidade musical | média em 12 s de `\|20·log10(eout/ein)\|` | dB | ST-15 | `cpp:992-1011` | *a definir* | 0 dB = perfeitamente linear |
| **MS-15** Reproducibility | Reprodutibilidade | **fixo em 100.0** | % | ST-15 | `cpp:1012` | ⚠️ **placeholder** — ver achado V4 | — |

---

## Seção C — Limites normativos (estado atual)

| Categoria | Limiar existente no código | Origem | Status |
|---|---|---|---|
| Headroom | `−1 dBFS` (crítico), `−6 dBFS` (baixo) | `html:2226, 2717` | Implícito na UI |
| Clipping | `\|s\| ≥ 0.999` | `html:778` | Definido |
| DC offset | `\|dc\| > 1e-3` | `html:2717` | Definido |
| THD / THD+N / IMD | `1%` ("LIMIAR 1%") | `html:2461-2470` | Implícito, único limiar |
| Group delay | `~1 ms` (audível) | `html:1924` | Nota textual |
| SNR (retorno de captura) | `20 dB` acima do floor | `Constants.h:47` | É gate de captura, **não** de exibição |
| SNR (exibição) | gauge `0–120 dB` sem banda marcada | `html:2721` | Sem faixas bom/aceitável/ruim |
| Demais MS | — | — | **A DEFINIR no Pilar 2** |

> A maioria das métricas normativas **não tem faixas (bom / aceitável / comprometido) definidas nem
> expostas ao usuário**. Consolidá-las é o objeto do **Pilar 2**.

---

## Seção D — Achados de validade (questões abertas → Pilar 1/2)

- **V1 — Escada de confiança sem porta inferior** *(integridade, ativo)*. `trust` (`html:2226`) só
  testa teto/`lowConf`. Uma captura **silenciosa / sem sinal / SNR muito baixo** é rotulada
  **"Confiável"**. O material para a porta de piso (`rmsDb`, `silPct`, SNR) já existe em
  `captureIntegrity()` mas **não é usado**. *(Confirmado nas coletas Vostro −42 dBFS e loopback −240 dBFS.)*
  **Confirmado também no WebView nativo (2026-08-07, gate do P1-002):** uma sessão cuja **única**
  métrica disponível está em `NotApplicable` é rotulada **"Medição confiável"** — `NotApplicable`
  incrementa `na`, que **não é consumido** em lugar nenhum da UI, enquanto `lowConf` permanece 0.
  Tratamento em **P1-008**. Ver *Registro de Encerramento — P1-002* → §G.6.
- **V2 — `lowConf` quase nunca propaga.** A LabEngine JS fixa `quality:'Normal'` na maioria dos
  escalares (`html:1337+`), então `M.metrics.lowConf` (`html:806-811`) é ~sempre 0. O `Quality` por
  métrica do C++ **não** é o que alimenta o caminho da evidência do Lab.
- **V3 — "Consistência" com semântica invertida.** `RepetitionConsistency_pct` é um **coeficiente de
  variação** (0% = perfeito), mas é exibida num **gauge 0→100 rotulado "Consistência"** (`html:2721`),
  que lê visualmente *maior = melhor*. Necessário inverter (`100 − CV`) **ou** renomear para "Variação".
- **V4 — `Reproducibility_pct` é placeholder.** MS-15 grava `100.0` fixo (`cpp:1012`); não é medido.
- **V5 — Limiares normativos dispersos/implícitos.** THD/THD+N/IMD compartilham um único "1%"; Headroom
  usa −1/−6 dBFS; sem faixas para SNR, Noise Floor, Dynamic Range, Latency, etc.
- **V6 — Dupla implementação (C++ × JS).** Risco de divergência silenciosa das fórmulas espectrais.
- **V7 — Escalares `DBG_*` temporários** presentes em MS-04/05/06/07 (`cpp`), marcados
  "REMOVER antes do RC"; hoje entram no bundle de métricas.
- **V8 — MS-10 (SNR): numerador `S/N` vs `(S+D+N)/N` — RESOLVIDO pelo Registro de Decisão MS-10 (D2/V8)
  e ✅ IMPLEMENTADO no P1-006** (`a702c0f`, validado independentemente no Windows) — ver *Registro de
  Encerramento — P1-006* (§O).
  *(Histórico do achado preservado abaixo. Não é um defeito; foi uma escolha feita conscientemente — resolvida na direção `S/N` estrito.)*
  A primitiva `snrDb(signal, noise) = 10·log10(ps/pn)` (`cpp:71-76`) é agnóstica quanto ao conteúdo
  de `signal`. Há duas leituras coexistindo, cada uma apoiada por evidência:
  - **Intenção `S/N` puro** — sustentada por: (a) o comentário do header `// SNR em dB dado sinal e
    ruido (potencias RMS)` (`AnalysisEngine.h:34`), que descreve uma razão de potências limpa; e
    (b) o teste **TC-033** (`tests/test_analysis.cpp:93-103`), que valida `snrDb` com um tom **puro,
    sem distorção**, e ruído **separado**, esperando 80 dB ±0.5. O comportamento *especificado e
    verificado* da primitiva é `S/N`.
  - **Comportamento `(S+D+N)/N`** — no sítio de produção (`snr()`, `cpp:785-786`), `signal` é o bloco
    de 1 kHz **capturado inteiro**, que em medição real carrega tom + harmônicos + ruído. Assim o
    numerador efetivo é `(S+D+N)`, aproximando-se de um SINAD referido à saída.
  - **Rastreabilidade da origem:** as linhas do SNR entraram no commit único de baseline
    `4997d86 "AAMP baseline approved"` e **nunca foram modificadas** (blame `^`); **não há** histórico
    incremental nem mensagem de commit sobre o numerador, e o **DOC-03** citado nos comentários
    **não existe no repositório** — logo, não há especificação versionada que fixe a escolha.
  - **Decisão a tomar (antes de mexer em código/teste):** (a) formalizar `S/N` estrito — isolar o bin
    do tom no numerador, alinhado ao que o TC-033 já verifica; **ou** (b) assumir e **documentar**
    `(S+D+N)/N` como métrica tipo-SINAD, revisando o TC-033 para refletir essa definição. Hoje o
    código faz (b) enquanto o teste garante (a): o descompasso deve ser resolvido **na especificação**.

---

## Registro de Decisão — MS-10 (SNR) — **NORMATIVO**

> **Status: ESPECIFICAÇÃO APROVADA.** Este registro é normativo para o MS-10 e serve de referência
> para qualquer implementação futura. Aprovado pelo responsável do projeto. Nenhuma implementação,
> teste, constante ou limiar foi alterado nesta etapa — apenas a especificação foi consolidada.

### Decisões aprovadas

- **D1 — Definição de ruído.** ✅ **IMPLEMENTADA no P1-005** (`4825fcc`, validado independentemente
  no Windows) — ver *Registro de Encerramento — P1-005* (§N).
  A definição normativa de ruído do MS-10 passa a ser **band-limited,
  alinhada ao MS-08** (mesma banda 20 Hz–20 kHz). Uma verificação **independente** de energia fora da
  banda (DC, subsônicos, hum) permanece na **camada de Integridade** (não no `pn` do SNR). *Racional:
  consistência interna entre SNR e Noise Floor + comparabilidade normativa, sem perder detecção de
  anomalias fora de banda.*
- **D2 / V8 — Numerador.** ✅ **IMPLEMENTADA no P1-006** (`a702c0f`, validado independentemente no
  Windows) — ver *Registro de Encerramento — P1-006* (§O).
  O MS-10 é formalizado como **S/N estrito** (numerador = energia do tom de
  referência isolada, não o bloco inteiro). **THD, THD+N e IMD** continuam responsáveis pela distorção.
  O rótulo **"SNR" deve corresponder ao comportamento da métrica**. *Isto encerra a decisão pendente do
  V8 na direção (a): alinhado ao que o TC-033 já verifica.* **Concretização:** potência espectral
  absoluta da fundamental de 1 kHz retornada, quadro único centrado de `N=4096`, Hann + ENBW/Parseval,
  integração em `k0 ± 2` com `k0 = round(1000·N/fs)`.
- **D3 — Referência.** Mantida a referência **output-referred** (nível em que o tom retornou). Decisão
  **registrada explicitamente** na especificação. **Nenhuma alteração de implementação** necessária.
- **D4 — Estados de qualidade.** Aprovado o **princípio** de propagação de estados de qualidade ao
  MS-10 e que **SNR insuficiente possa reduzir a confiança** da caracterização. **Item em aberto:** qual
  estado representa `pn≈0` (saturação em 200 dB) **não** está decidido — será resolvido durante a
  **implementação do Pilar 1** ou, no máximo, **antes do Pilar 2**. Os limiares numéricos de "SNR
  insuficiente" pertencem ao **Pilar 2**.
- **D5 — Apresentação.** Aprovado o **conceito**. As **faixas normativas / valores** e a **apresentação
  da saturação** permanecem responsabilidade do **Pilar 2**.

### Tabela de decisão (com Impacto da Implementação)

| ID | Decisão aprovada | Recomendação origem | **Impacto da Implementação** | Aberto? |
|----|------------------|---------------------|------------------------------|:------:|
| ~~**D1**~~ ✅ | Ruído band-limited (alinhado ao MS-08) + checagem fora-de-banda na Integridade | B | **EXECUTADA em P1-005** — algoritmo convergido (`pn` do MS-08), TC-048 criado, documentação atualizada. Checagem fora-de-banda na Integridade permanece fora do escopo | — |
| ~~**D2/V8**~~ ✅ | S/N estrito; distorção fica com MS-05/06/07 | A | **EXECUTADA em P1-006** — numerador = potência da fundamental retornada, TC-049 criado, TC-048 atualizado nos pontos que fixavam o numerador antigo, documentação atualizada | — |
| **D3** | Manter output-referred | A | **Sem impacto de código** · **Altera documentação** | — |
| **D4** | Propagar estados de qualidade; SNR baixo reduz confiança | B (princípio) | **Altera algoritmo** · **Altera UI** · **Altera testes** · **Altera documentação** | **Sim** — estado de `pn≈0` (→ P1, no máx. antes do P2); limiares → P2 |
| **D5** | Conceito de apresentação aprovado | B | **Altera UI** · **Altera documentação** (concretização → P2) | Faixas/saturação → P2 |

### Matriz de Rastreabilidade — MS-10

| Decisão | Arquivos afetados | Testes afetados | Achados relacionados | Pilar |
|---------|-------------------|-----------------|----------------------|-------|
| **D1** | `src/analysis/AnalysisEngine.cpp` (`snr`/`snrDb`); `resources/aamp_laboratory.html` (checagem fora-de-banda na Integridade) | `tests/test_analysis.cpp` (TC-033) | V8, V1, V6 | Pilar 1 |
| ~~**D2/V8**~~ ✅ | `src/analysis/AnalysisEngine.cpp` (`snr`, `inBandPower` parametrizado); `resources/aamp_laboratory.html` (`computeMS10`). **`AnalysisEngine.h` NÃO foi alterado** — o contrato público não mudou | `tests/test_analysis.cpp` (**TC-049** novo; **TC-048** atualizado; **TC-033** intacto) | V8, V2 | Pilar 1 |
| **D3** | `docs/laboratory/metrics-audit.md` (somente documentação) | — | — | Pilar 1 (doc) |
| **D4** | `src/analysis/AnalysisEngine.cpp` (qualidade do `snr`); `resources/aamp_laboratory.html` (porta inferior da confiança `:2226`, `normalizeMetrics`) | `tests/test_analysis.cpp` (novos casos de estado) | V1, V2 | Pilar 1 (princípio) / Pilar 2 (limiares) |
| **D5** | `resources/aamp_laboratory.html` (gauge/chip `:2713,:2721`) | — | V5 | Pilar 2 |

### Encerramento

**MS-10 encerrado formalmente** como especificação aprovada, com **um único item em aberto**
explicitamente rastreado: o estado de qualidade para `pn≈0` (D4). A implementação (Pilar 1) do MS-10
deve seguir este registro como referência normativa.

> **Situação de implementação (2026-08-10).** **D1 CONCLUÍDA** pelo **P1-005**, validada
> independentemente no Windows (§N). **D2/V8 CONCLUÍDA** pelo **P1-006**, validada independentemente
> no Windows (§O) — o MS-10 é hoje **S/N estrito**. **D3** não exigia código e foi **honrada**: a
> referência permanece *output-referred*, medida exclusivamente da captura. **D4** (`pn ≈ 0`)
> permanece com o **P1-007**, **liberado para análise**, que deverá **reutilizar a taxonomia
> `EstimateNature`** entregue pelo P1-009 estágio 1, em vez de criar uma segunda solução. **D5**
> permanece no Pilar 2.

---

## Registro de Decisão — MS-08 (Noise Floor) — **NORMATIVO**

> **Status: ESPECIFICAÇÃO APROVADA.** As decisões normativas do MS-08 estão consolidadas. Documentação
> apenas — nenhuma implementação, teste, constante, limiar ou algoritmo foi alterado.

**N1 — Definição normativa de "Noise Floor": APROVADO.**
- **Decisão aprovada:** o Noise Floor do AAMP é definido como o **nível RMS total de ruído residual
  integrado na banda de 20 Hz a 20 kHz, expresso em dBFS**, conforme a especificação normativa
  [`docs/laboratory/noise-floor-definition.md`](./noise-floor-definition.md)
  (**ESPECIFICAÇÃO NORMATIVA APROVADA**).
- **Conformidade:** futuras implementações deverão **demonstrar conformidade com esta definição,
  independentemente do algoritmo empregado** (Princípio da Independência do Estimador). O algoritmo é
  apenas um *estimador* da grandeza normativa; nunca a redefine.
- **Disposição dos demais itens (N2–N6):** **N2** (banda 20–20 k) fica **subsumida** na definição
  ratificada; **N6** (reúso do ruído pelo SNR) = **D1**, já aprovado, sob a ordem obrigatória do
  **AR-01**; **N3** (método/observação) **deixa de ser questão normativa** — é escolha de *estimador*,
  regida pelo Princípio da Independência do Estimador (Pilar 1); **N4** (estados de qualidade) segue o
  princípio de **D4** (piso não-mensurável / silêncio sujo → Pilar 1; limiares → Pilar 2); **N5**
  (apresentação/faixas) → **Pilar 2**.

**N1-R1 — Precondição de normalização (ENBW).** A independência em relação à FFT pressupõe normalização
adequada para **ruído** (ENBW / Parseval). A implementação atual utiliza correção de **ganho coerente**
(`windowCoherentGain = Σw`, `Windows.cpp:46`), apropriada para **tons discretos**. Esta observação **não
invalida nenhuma definição normativa**, mas deverá ser considerada quando o **Pilar 1** entrar na fase de
implementação. **Verificado (Apêndice B):** a estimativa atual é energeticamente correta para **tons**,
**não** para ruído de banda larga (falta o fator ENBW / Σw²). *Provável reaparecimento ao revisar MS-09,
espectro, Dynamic Range e exportação.*

### Matriz de Rastreabilidade — MS-08

| Decisão | Documento Normativo | Arquivos afetados (impl. futura) | Testes afetados | Vínculos | Pilar |
|---------|---------------------|----------------------------------|-----------------|----------|-------|
| **N1** (definição do Noise Floor) | [`noise-floor-definition.md`](./noise-floor-definition.md) | `src/analysis/AnalysisEngine.cpp` (MS-08) — *sem alteração nesta etapa* | `tests/test_analysis.cpp` (conformidade futura) | MS-09; MS-10 (D1); AR-01; N1-R1; Apêndice A; Apêndice B | Pilar 1 |

### Encerramento do MS-08

**MS-08 conceitualmente encerrado.**
- Todas as decisões **normativas** foram **aprovadas** (definição ratificada em `noise-floor-definition.md`).
- Permanece **apenas a futura implementação (Pilar 1)**.
- **AR-01** continua **obrigatória** antes de qualquer reutilização normativa do pipeline espectral
  (ordem: corrigir e validar o MS-08 → só então implementar D1 no MS-10).

---

## Registro de Decisão — MS-09 (Dynamic Range) — **NORMATIVO**

> **Status: ESPECIFICAÇÃO APROVADA.** Documentação apenas — nenhum código, teste, constante, limiar ou
> algoritmo alterado. Decisões na ordem lógica **D1 → D2 → D3** (dependência do estudo conceitual, Parte 4).
> Base: `dynamic-range-definition.md`, `dynamic-range-conceptual-study.md`, `dynamic-range-impact-study.md`,
> Apêndice C, e a normativa ratificada (`noise-floor-definition.md` + AR-01).

### D1 — Critério de teto
- **Questão:** Qual definição normativa de teto o AAMP adota?
- **Alternativas consideradas:** Saturação (A início do clipping / D maior RMS sem clipping / E fundo de
  escala fixo); Linearidade (B último nível linear / F compressão 1 dB); Distorção (C limiar THD/THD+N).
- **Decisão adotada:** **Saturação** — o teto é o **nível RMS do maior sinal reproduzido sem saturação**
  (maior nível cujo pico não atinge o fundo de escala digital). *(Variante A/D; não é o fundo de escala
  fixo E, nem um critério por distorção.)*
- **Justificativa:** a especificação proposta já adota "início da saturação" e o expressa como **nível**
  (§1/§2/§5); o estudo de impacto mostra que a família Saturação **preserva a independência entre
  métricas** (não acopla o DR a THD/THD+N) com compatibilidade normativa **sem impacto**, enquanto
  Distorção acopla (alto) e Linearidade acopla (moderado); o critério RMS mantém a **coerência
  dimensional** com o piso.
- **Consequências arquiteturais:** sobreposição **conceitual** com o Headroom (mesmo evento de plena
  escala), sem acoplamento computacional; DR permanece autocontido (sem dependência de THD/THD+N/IMD).
- **Impactos aceitos:** sensibilidade ao **fator de crista** do sinal de teste; e a **renúncia** à
  definição por limiar de distorção (padrão de facto de conversores), em favor da independência entre
  métricas.
- **Impacto sobre documentação existente:** confirma `dynamic-range-definition.md` §1/§2/§5; a "Nota de
  ratificação" passa de "a confirmar" para "ratificado".
- **Impacto sobre implementação futura:** **conforme** ao já auditado (Apêndice C, MS09-A1); nenhuma
  mudança de critério necessária (a expressão do estimador é livre — Independência do Estimador).
- **Rastreabilidade:** `dynamic-range-definition.md` §1/§2/§5; conceptual-study P1; impact-study P1/P4;
  Apêndice C MS09-A1.

### D2 — Banda do teto
- **Questão:** O teto usa a mesma banda do Noise Floor (20–20 k) ou permanece em banda irrestrita?
- **Alternativas consideradas:** banda irrestrita; banda 20 Hz–20 kHz.
- **Decisão adotada:** **Mesma banda do Noise Floor — 20 Hz a 20 kHz, sem ponderação.**
- **Justificativa:** a especificação proposta (§4) já exige teto e piso **na mesma banda**; o estudo
  conceitual mostra que banda irrestrita produz uma **razão híbrida** (não uma grandeza física única) e
  cria **tensão com o Noise Floor ratificado** (que exclui energia fora de banda); mesma banda restaura
  **coerência dimensional, comparabilidade** e independência do conteúdo fora de banda.
- **Consequências arquiteturais:** o DR passa a ser a diferença de dois níveis **na mesma banda** →
  grandeza única e comparável; energia fora de banda permanece exclusivamente na camada de Integridade.
- **Relação com AR-01:** a decisão **herda** AR-01 — estimar o teto **em banda** envolve energia de banda,
  sujeita à **validação ENBW/Parseval**. A implementação do teto band-limited fica **condicionada** a
  AR-01 (normalização energética validada antes do reúso). **Não viola** AR-01; fica governada por ela.
- **Impacto sobre documentação existente:** confirma §4; acrescenta o registro da herança de AR-01.
- **Impacto sobre implementação futura:** cria uma **divergência a resolver no Pilar 1** — a implementação
  atual mede o teto em **banda larga no tempo** (Apêndice C, MS09-A2); alinhar à banda 20–20 k é tarefa do
  Pilar 1, sob AR-01.
- **Rastreabilidade:** `dynamic-range-definition.md` §4; conceptual-study P2; impact-study P2; AR-01;
  Apêndice C MS09-A2; `noise-floor-definition.md` §4/§5.

### D3 — Degeneração (`teto ≤ piso`)
- **Questão:** Como o AAMP caracteriza formalmente o caso `teto ≤ piso`?
- **Alternativas consideradas:** extensão nula (0); não caracterizável; valor negativo diagnóstico;
  distinção `teto = piso` vs `teto < piso`.
- **Decisão adotada:** **Não caracterizável** — quando o teto **não excede** o piso, o Dynamic Range é
  reportado como **faixa não utilizável / não caracterizável**, nunca como valor positivo espúrio nem como
  número negativo. *(Inclui o caso-limite `teto = piso`.)*
- **Justificativa:** a especificação proposta (§7) já determina "sinalizada como não utilizável, nunca
  como valor positivo espúrio"; o estudo conceitual estabelece que `teto ≤ piso` é a **ausência da
  premissa física** (uma "faixa" não pode ser negativa); o estudo de impacto mostra que "não
  caracterizável" **mapeia à taxonomia existente** (`NotApplicable`), é claro na interface e na
  exportação, e evita a ambiguidade do 0 e a confusão do negativo.
- **Implicações para exportação:** o DR é exportado como **ausente / não aplicável** (não um número), com
  a condição declarada — consumidores tratam a ausência.
- **Implicações para Trust Ladder:** sinaliza **ausência de faixa útil** → contribui para **reduzir a
  confiança** (coerente com o papel do DR/SNR na porta inferior).
- **Consequências arquiteturais:** reusa a taxonomia `NotApplicable`; a eventual **distinção**
  `teto = piso` vs `teto < piso` fica como refinamento de **Pilar 2/UI**, fora do escopo normativo.
- **Impacto sobre documentação existente:** confirma §7; wording ajustado para "não caracterizável".
- **Impacto sobre implementação futura:** a implementação atual trata só "nenhum degrau válido" →
  `NotApplicable`, mas **reporta DR negativo como `Normal`** (Apêndice C, MS09-A4) → alinhar ao "não
  caracterizável" para `teto ≤ piso` é tarefa do Pilar 1.
- **Rastreabilidade:** `dynamic-range-definition.md` §7; conceptual-study P3; impact-study P3;
  Apêndice C MS09-A4; Legenda (`NotApplicable`).

### Síntese

| Decisão | Estado |
|---|---|
| **D1** — Critério de teto (saturação; nível RMS do maior sinal sem clipping) | **Aprovada** |
| **D2** — Banda do teto (20–20 k, mesma do Noise Floor; herda AR-01) | **Aprovada** |
| **D3** — Degeneração (`teto ≤ piso` → não caracterizável) | **Aprovada** |

### Encerramento do MS-09

**MS-09 encerrado** como especificação aprovada. Permanece **apenas a implementação (Pilar 1)** — com uma
divergência conhecida de banda (MS09-A2, D2) e de degeneração (MS09-A4, D3) a alinhar, **condicionada a
AR-01** (D2). **Nenhum item normativo em aberto.**

---

## Restrições arquiteturais (AR)

> Regras normativas de **arquitetura**, distintas de achados (V*) e de decisões (D*/N*). Vinculam
> qualquer implementação ou métrica futura, independentemente do indicador.

- **AR-01 — Normalização energética antes de reúso espectral de banda larga.** Nenhuma métrica normativa
  poderá **reutilizar resultados espectrais de banda larga** (potência/densidade oriunda do pipeline FFT)
  enquanto a **normalização energética do pipeline FFT (ENBW / Parseval) não estiver validada** (ver
  **N1-R1** e **Apêndice B**). *Motivação:* o pipeline atual é calibrado para **tons** (ganho coerente);
  reutilizar seu resultado de **ruído** propaga o viés ENBW. *Consequência imediata — ordem de
  implementação obrigatória:* **(1) corrigir MS-08 → (2) validar MS-08 → (3) só então implementar D1 no
  MS-10**. A ordem deixa de ser arbitrária. *(Se, no futuro, alguém criar outra métrica baseada em
  potência espectral, esta regra já a alcança.)*

  > **Estado: SATISFEITA** (2026-08-05). A precondição foi cumprida e validada — ver *Registro de
  > Encerramento — P1-001* abaixo. Os passos (1) e (2) estão concluídos; o passo (3) (D1 no MS-10)
  > fica liberado quanto a AR-01, permanecendo sujeito à ordem do Plano Mestre (P1-005).

---

## Registro de Encerramento — P1-001 (Normalização Energética do MS-08) — **NORMATIVO**

> **Estado: ENCERRADO** — técnica, analítica, sintética e fisicamente.
> **Escopo do item:** normalização energética (ENBW / Parseval) da estimativa de ruído do MS-08.
> **Referências normativas:** `noise-floor-definition.md` §2/§3/§7 · **N1-R1** · **AR-01** · Apêndice B.

### E.1 Implementação

Nova abstração dedicada ao **domínio de ruído** (não existia nenhuma função de ENBW / ganho energético
/ *window energy* / normalização de potência no código — apenas `windowCoherentGain = Σw`, do domínio de
tom): `windowPowerGain(win) = Σw²` e `windowEnbwBins(win) = n·Σw²/(Σw)²` (`Windows.h/.cpp`), consumidas
pelo estimador estático `noisePowerSpectrum()` (`AnalysisEngine.cpp`), que **reutiliza `magSpectrum()`
sem alterá-la** e aplica o fator `1/(2·ENBW)` exigido por Parseval. Aplicado apenas nos dois laços de
acumulação de `noiseFloor()` e, de forma espelhada, em `computeMS08()` do LabEngine (**CC-A**).

A **primitiva de tom permaneceu inalterada**: `magSpectrum()`, `fullSpectrum()`, `peakBin()`,
`thdPercent()` e as famílias MS-05/06/07 ficaram *byte-idênticas* (verificado por diff).

Para a janela Hann o fator é a constante `10·log10(2·ENBW) = 4,772273 dB`, dependente apenas da **forma**
da janela — independente de `N`, `fs`, nível e *overlap*.

### E.2 Regressão permanente — TC-038

`TC-038 — MS-08 piso de ruído energeticamente correto (ENBW/Parseval) ±0,1 dB` (`tests/test_analysis.cpp`).
Oráculo **independente do código de produção**: o nível esperado é derivado no próprio teste por
`L_esperado = 20·log10(RMS_total) + 10·log10(bins_na_banda / (N/2))`, sobre ruído determinístico em três
níveis (−60, −95, −120 dBFS). Protege por três vias: comparação absoluta (tolerância 0,1 dB, a mesma de
TC-031), **asserção anti-regressão explícita** contra o valor que a normalização por ganho coerente
produziria, e **invariância de escala** (0,01 dB).

**Controle negativo executado:** com o fator `1/(2·ENBW)` removido, TC-038 falha em **7 checks**;
restaurado, passa. Registrado que as asserções de invariância de escala **continuam passando** sob o
defeito — um viés constante preserva diferenças —, razão pela qual a comparação absoluta e a asserção
anti-regressão são necessárias.

### E.3 Validação sintética

| Nível RMS total | Teórico in-band | Medido | Erro absoluto |
|---:|---:|---:|---:|
| −60,00 dBFS | −60,7961 | −60,7998 | 0,0037 dB |
| −95,00 dBFS | −95,7961 | −95,7998 | 0,0037 dB |
| −120,00 dBFS | −120,7961 | −120,7998 | 0,0037 dB |

Erro **constante em 5 décadas de nível** (*scale invariance*); pior erro = 3,7 % da tolerância adotada.

### E.4 Validação em ambiente

| Ambiente | Build | Resultado |
|---|---|---|
| Linux (headless, `aamp_core`/`aamp_tests`) | CMake Release | **461 checks, 0 falhas** |
| Windows (Vostro) | CMake + Ninja Release | **469 checks, 0 falhas** |

Divergência de contagem entre plataformas registrada como **F8** (abaixo). Em ambos os ambientes
TC-030, TC-031, TC-032, TC-033, TC-034 e TC-038 passaram.

### E.5 Validação física — caracterização com a Helix (preset AC30 Clean)

Caracterização física realizada em **loopback digital puro**, dispositivo Helix, preset **AC30 Clean**,
48 000 Hz, mono, 16 capturas. Resultados observados no Laboratório: Noise Floor **−96,2 dBFS**,
SNR **80,4 dB**, Dynamic Range **80,1 dB**, Clipping 0, Consistência 1,5 %.

Registrada também a caracterização anterior em **preset vazio** (Noise Floor −300 dBFS, SNR 200 dB,
Dynamic Range 290 dB): trata-se de **silêncio digital bit-exato** — `−300 dBFS` é o *clamp*
`10·log10(1e-30)` e `200 dB` é o retorno de saturação de `snrDb` para `pn ≤ 0`. Esse caso é
**insensível ao fator ENBW** (produziria o mesmo valor com a normalização anterior) e, portanto,
**não constitui evidência sobre o P1-001**; ele exercita P1-002, P1-004 e P1-007, que permanecem abertos.

### E.6 Validação A/B — **reprocessamento offline, sem nova aquisição**

> **Natureza do procedimento — registro explícito.**
> **Não foi realizada uma nova caracterização física.** **Não houve nova aquisição da Helix.**
> O procedimento A/B foi feito por **reprocessamento offline** do arquivo `captures/ST09_Capture.wav`
> **extraído diretamente do Evidence Package da caracterização física já existente** (o `.aamp` é um
> container ZIP STORE com WAV IEEE float32 mono).
>
> Os dois binários processaram **exatamente o mesmo arquivo de áudio, byte a byte**. Nenhuma recaptura
> foi realizada; nenhuma condição de hardware foi alterada. **A única variável modificada entre as duas
> medições foi o algoritmo.**

| Estado | Commit | `NoiseFloor_dBFS` |
|---|---|---:|
| Pré-correção | `692a49a` | **−91,457365** |
| Pós-correção | `9222ae0` | **−96,229638** |
| | **Δ** | **−4,772273 dB** |

O deslocamento coincide **exatamente** com `−10·log10(2·ENBW_Hann) = −4,772273 dB`.

*Controle do procedimento:* o binário pré-correção foi verificado como genuinamente anterior (0
ocorrências de `noisePowerSpectrum`; `acc[b] += mag[b]*mag[b]` presente) e, sobre a captura sintética de
referência, devolveu o valor pré-correção já conhecido (−91,027516 dBFS).

*Ferramenta:* modo `noise` de `tools/wav_metric_harness` (ferramenta de regressão **fora do produto**;
commit `9222ae0`), que mapeia o WAV como ST-09 e extrai o escalar oficial via `computeAllMetrics()`.

### E.7 Verificações complementares

**(a) Oráculo independente no domínio do tempo.** O RMS de banda larga calculado diretamente das
amostras (`sqrt(mean(x²))`, sem FFT, janela, *overlap* ou normalização) reproduziu o resultado espectral:

```
NoiseFloor_dBFS (espectral, Welch/Hann, 20 Hz–20 kHz, ENBW) = −96,229638 dBFS
RMS de banda larga (domínio do tempo)                       = −96,2298   dBFS
erro                                                        ≈  0,00016  dB
```

**(b) CC-A confirmado sobre dado físico real.** O valor exibido pelo LabEngine (JS) e o produzido pelo
`AnalysisEngine` (C++) coincidem na precisão apresentada — a verificação C++ ↔ JS deixa de ser apenas
sintética.

**(c) Interface, Evidence Package e áudio bruto coerentes.** Os valores apresentados no Laboratório
foram reproduzidos a partir do áudio bruto preservado no pacote:

| Interface | Reproduzido do WAV | Confere |
|---|---|:--:|
| Piso de ruído −96,2 dBFS | −96,2296 | ✓ |
| Nível RMS −96,2 dBFS | −96,2298 (RMS banda larga) | ✓ |
| Headroom −92,2 dBFS | −92,2387 (pico) | ✓ |
| Crista 4,0 dB | 3,9911 | ✓ |

Confirma a **fidelidade do áudio no Evidence Package** (o WAV extraído regenera os números da tela).

### E.8 Infraestrutura de diagnóstico (validada no Windows)

O canal único `src/common/DiagLog` (commit `8fb6849`) foi validado no ambiente real. Confirmados:
**SessionId**, **CommitFull/CommitShort**, **Branch**, **GitDirty**, **timestamps absolutos**
(`YYYY-MM-DD HH:MM:SS.mmm`, `system_clock`), **timestamps relativos** (`[T+HH:MM:SS.mmm]`,
`steady_clock`), **rotação e arquivamento automático** da sessão anterior em `logs/`, **rodapé de
encerramento** com `Elapsed`/`Exit` e **proveniência completa** do binário. A ausência de rodapé
permanece como evidência de término anormal e não é fabricada pela sessão seguinte.

### E.9 Matriz de rastreabilidade — P1-001

| Requisito / origem | Documento normativo | Implementação | Evidência | Estado |
|---|---|---|---|:--:|
| Grandeza = nível RMS in-band em dBFS | `noise-floor-definition.md` §2/§3 | `noisePowerSpectrum()` + `noiseFloor()` (`2bc5f88`) | E.3, E.6, E.7(a) | ✅ |
| Estimativa energeticamente correta (ENBW/Parseval) | **N1-R1**, Apêndice B | fator `1/(2·ENBW)` | E.3 (0,0037 dB), E.7(a) (0,00016 dB) | ✅ |
| Banda 20 Hz–20 kHz, sem ponderação | `noise-floor-definition.md` §4 | filtro de banda preservado em `noiseFloor()` | E.6, E.7(a) | ✅ |
| Coerência dimensional com MS-09/MS-10 | `noise-floor-definition.md` §7 | escala RMS/dBFS homogênea | E.7(c) | ✅ |
| Sem regressão da calibração de tom | Autorização P1-001 | `magSpectrum`/`fullSpectrum`/`peakBin`/`thdPercent` intactos | diff byte-idêntico; TC-031, TC-032 | ✅ |
| **AR-01** satisfeita (precondição validada) | AR-01 | — | E.3, E.6, E.7 | ✅ |
| **CC-A** — C++ ↔ JS | Plano Mestre | `computeMS08()` espelhado | E.7(b) + `−95,799789` idêntico em ambos | ✅ |
| Regressão permanente | Critério de aceite | TC-038 (`acca002`) | E.2 + controle negativo | ✅ |
| Reprodutibilidade / determinismo | `noise-floor-definition.md` §7 | — | TC-034 | ✅ |
| Proveniência da evidência | — | `DiagLog` (`8fb6849`) | E.8 | ✅ |

**Commits do item:** `2bc5f88` (implementação) · `acca002` (TC-038) · `8fb6849` (infraestrutura de
diagnóstico) · `9222ae0` (ferramenta de reprocessamento A/B).

### E.10 Achados da validação (F*)

Série **F** — achados originados na revisão técnica final do P1-001, distintos dos achados de auditoria
(**V***) e das decisões (**D*/N***).

| ID | Achado | Estado | Bloqueia |
|---|---|:--:|---|
| **F1** | Conteúdo do resíduo do ST-09 e validade do estimador para conteúdo tonal | ✅ **Encerrado** | — |
| **F2** | `Dynamic Range < SNR`, contrariando `dynamic-range-definition.md` §6 | ✅ **Resolvido** (Registro de Decisão — F2: **B** + **D**) | — |
| **F9** | Segmentação temporal do ST-08 não alinhada (derivado de F2) | ✅ **Resolvido e validado** (A2 · `cfb95fa` · TC-041) | — |
| **F3** | Interface declara "métricas normativas não se aplicam" e exibe os valores | 🟡 Aberto | — |
| **F5** | Gráfico de "Captura de silêncio" visualmente enganoso | ✅ **Encerrado** | — |
| **F8** | Divergência de contagem de checks entre plataformas (469 × 461) | 🟡 Aberto | — |

#### F1 — **ENCERRADO** · inclui correção de entendimento

**Correção de entendimento (registro preservado para histórico).** Durante a revisão foi levantada a
ressalva de que o estimador ENBW/Parseval subestimaria conteúdo **tonal** em `10·log10(ENBW) = 1,76 dB`,
por supor que a energia de um tom se concentra em um único *bin*. **Essa ressalva foi revisada e está
incorreta.** Com janela Hann, um tom de amplitude `A` distribui-se pelo lóbulo principal com amplitudes
≈ `A/2, A, A/2`, de modo que `Σ_bins mag² = A²(0,25 + 1 + 0,25) = 1,5·A² = ENBW·A²`; aplicando
`1/(2·ENBW)` obtém-se `A²/2` — exatamente a potência média do tom. O comportamento é o esperado:
**Parseval é independente do tipo de sinal**, e essa independência é justamente a propriedade que a
correção implementa.

**Confirmação empírica:** a captura analisada concentra **96,18 %** da energia in-band em três *bins*
adjacentes, e ainda assim o estimador espectral casou com o RMS temporal em **0,00016 dB** (E.7a).

> **Conclusão de F1:** o estimador ENBW/Parseval é **válido tanto para ruído de banda larga quanto para
> conteúdo tonal**. **Não existe ressalva de domínio de validade para o P1-001.**

**Caracterização da captura (observação, sem atribuição de origem).** O espectro médio do ST-09
analisado apresenta **predominância de componentes discretas próximas de 120 Hz e harmônicos
associados**:

| Freq. (Hz) | Nível (dBFS) | % do total | % acumulado |
|---:|---:|---:|---:|
| 117,2 | −98,47 | 59,67 % | 59,67 % |
| 128,9 | −101,51 | 29,63 % | **89,30 %** |
| 105,5 | −107,86 | 6,88 % | **96,18 %** |
| 58,6 | −115,36 | 1,22 % | 97,40 % |
| 234,4 / 246,1 | −117,6 / −117,8 | 1,42 % | 98,83 % |

Os *bins* 9/10/11 (105,5 / 117,2 / 128,9 Hz) são **adjacentes** — correspondem ao lóbulo principal da
janela Hann sobre **uma única** componente, não a três componentes distintas. Distribuição: 50 % da
energia em **1** *bin* e 90 % em **3**, contra ~852 e ~1534 *bins* esperados para ruído de banda larga
(1705 *bins* na banda). O envelope RMS é **estacionário** (−96,2 ± 0,06 dB ao longo de 10,2 s).

> **A origem física dessas componentes não foi determinada** e **não é atribuída** neste registro.
> Registra-se apenas a observação espectral da cadeia medida. Consequência quantitativa observada:
> excluindo o agrupamento próximo de 120 Hz o restante fica em ≈ −110 dBFS; excluindo todas as
> componentes discretas observadas, em ≈ −121 dBFS. Componentes discretas sob silêncio **pertencem** ao
> Noise Floor conforme `noise-floor-definition.md` §1/§5 (que exclui distorção, não componentes
> discretas) — **nenhuma questão normativa em aberto**.

#### F2 — **RESOLVIDO** pelo Registro de Decisão — F2

- **Observação original:** `Dynamic Range = 80,1 dB` e `SNR = 80,4 dB` na caracterização física,
  contrariando a redação então vigente da `dynamic-range-definition.md` §6.
- **Verificado:** **não** é artefato da divergência entre os dois estimadores de piso — o piso de
  banda larga e o de banda coincidem em **0,0002 dB** nessa captura (E.7a). O **P1-005 não o resolve**.
- **Encerramento:** estudo conceitual concluído; decisões **B** e **D** ratificadas. Registro completo
  em *Registro de Decisão — F2*, abaixo. O achado derivado de segmentação temporal é **F9**.

> ⚠️ **Correção de interpretação — a hipótese registrada anteriormente estava incorreta.**
> A versão anterior deste registro atribuía o desvio a uma diferença de frequência entre o teto e a
> referência, sugerindo que *"a referência é um bloco de 2 s do ST-03 que pode cair sobre uma
> ressonância do modelo"* (ressonância próxima de 95 Hz), e concluía que *"a referência do ST-03 está
> ≥ 0,3 dB **acima** do teto"*.
>
> **Ambas as afirmações estão refutadas.** O `snr()` recorta `[2 s, 4 s)` do ST-03 — o bloco de
> **1 kHz** — e o degrau mais alto do ST-08 é **1 kHz a −6 dBFS**: **mesma frequência, mesmo nível
> nominal**. A ressonância de 95 Hz é **irrelevante** para a referência do SNR. E os dois números
> estimam **o mesmo sinal físico**, de modo que a leitura *"referência acima do teto"* — embora
> aritmeticamente verdadeira — é **fisicamente enganosa**: não descreve um fato sobre a cadeia medida,
> e sim um viés de estimador. A hipótese anterior fica **substituída**, não preservada.

#### F3 — **ABERTO** · Pilar 2 / P1-008

A aba Integridade declara *"Sinal externo — métricas normativas (SNR, consistência) não se aplicam"* e,
no mesmo cabeçalho, exibe `SNR 80,4 dB` e `Consistência 1,5 %`; a aba Ruído as reapresenta como leituras
normativas. Declaração de aplicabilidade e apresentação têm fontes distintas. Relacionado a **V1/V5** e
a **MS-10/D5**. Tratamento previsto **durante o Pilar 2 / P1-008**.

#### F5 — **ENCERRADO** · achado de apresentação (Pilar 2)

O gráfico rotulado "Captura de silêncio" exibe forma de onda de aparência musical, com envelope variável
e decaimento, em eixo rotulado ±0,50 — enquanto o arquivo tem pico de −92,2387 dBFS (2,4 × 10⁻⁵) e
envelope **plano**. Causas identificadas:

1. **Auto-normalização** do gráfico: os rótulos do eixo não correspondem à escala dos dados;
2. **Efeito de moiré na renderização**: 489 568 amostras em ~1300 px ⇒ ~377 amostras por pixel, contra
   400 amostras por período da componente dominante ⇒ **0,94 ciclo por pixel**. Menos de um ciclo por
   pixel produz batimento de fase entre pixels — o padrão pseudoaleatório observado.

> **Dados corretos; representação gráfica potencialmente enganosa.** Nenhum escalar é afetado.
> Achado de **apresentação** → **Pilar 2**.

#### F9 — **RESOLVIDO · VALIDADO INDEPENDENTEMENTE NO WINDOWS** *(derivado de F2)*

**Problema (histórico).** `AnalysisEngine::dynamicRange()` fatiava o ST-08 em 8 janelas fixas de
100 ms **a partir da amostra 0**, sem compensar a latência da cadeia. Cada janela misturava dois
degraus adjacentes; como a progressão é monotônica, o **viés era sistemático e sempre reduzia o
teto**. **A mesma captura já era tratada corretamente pelo MS-13**, que alinha por correlação de
envelopes antes de fatiar — MS-09 e MS-13 liam o ST-08 sob **convenções de alinhamento diferentes**,
incoerência interna que era o núcleo de F9 e existia independentemente de F2.

**Solução — alternativa A2**, escolhida entre as estudadas em
[`temporal-segmentation-impact-study.md`](./temporal-segmentation-impact-study.md), por **menor
escopo**, **precedente existente** e **rastreabilidade**. F9 foi tratado como **item próprio e
pré-requisito** do P1-003, não incorporado a ele.

Registro completo em *Registro de Encerramento — F9*, abaixo.

#### F8 — **ABERTO** · higiene de evidência

Windows reporta **469 checks** e Linux **461**, no mesmo commit. Verificado que **não existe nenhum
`#ifdef`/`_WIN32` em `tests/`**. Enquanto não reconciliado, "a suíte passou" não significa exatamente a
mesma afirmação nos dois ambientes. Não bloqueia nenhum item do Plano Mestre.

---

## Registro de Encerramento — P1-002 (Estados de Qualidade do MS-08) — **NORMATIVO**

> **Estado: ENCERRADO** no escopo autorizado — **cálculo, contrato semântico e exportação**.
> **Escopo do item:** estados de qualidade do MS-08 para silêncio degenerado.
> **Referências normativas:** `noise-floor-definition.md` §7 (Estabilidade, Rastreabilidade) ·
> **N4** · princípio **D4** · Plano Mestre P1-002.
> **Escopo reduzido por decisão:** a detecção de **silêncio sujo (DV-4)** foi **excluída** deste item.

### G.1 Implementação e mapeamento aprovado

Antes: `r.quality = Quality::Normal` **incondicional** e `Quality::Normal` literal em todos os
escalares. Silêncio vazio, curto ou digitalmente nulo era reportado como medição válida de
**−300 dBFS** (o *clamp* `10·log10(1e-30)`) em estado `Normal`.

| Condição | Detecção | `quality` | `valuePresent` |
|---|---|---|:--:|
| ST-09 ausente | `captured.find()` falha | `Error` *(preservado)* | — |
| Nada observado | `fr2 == 0` | **`NotApplicable`** | `false` |
| Piso não-mensurável | `integFull == 0` | **`NotApplicable`** | `false` |
| Observação insuficiente | `fr2 < k::NOISE_AVG_FRAMES` | **`LowConfidence`** | `true` |

**Nenhum limiar novo foi introduzido.** O limite de observação insuficiente reutiliza
`k::NOISE_AVG_FRAMES = 16` (`Constants.h:59`), a observação já dimensionada pelo projeto e usada
pelo próprio estimador. Limiares numéricos permanecem matéria do **Pilar 2** (N4/N5/D4).

`NoiseFloor_frames` permanece **sempre `Normal` e presente**: é o descritor auditável exigido por
§7 (Rastreabilidade) e o **discriminador** entre "nada observado" (`0`) e "observado e nulo" (`>0`),
que compartilham o mesmo *clamp*.

**CC-A / CC-B:** `computeMS08()` do LabEngine espelha a classificação e passa a devolver
`frames`/`quality`/`valuePresent`; a montagem `mr08` deixa de fixar `quality:'Normal'` (escopo
MS-08 apenas). **O estimador do P1-001 não foi tocado** — nenhuma expressão numérica foi alterada.

**Commit:** `18f64abb3db6dcb9505e4ee2aae65075ef5b3c38`.

### G.2 Regressão permanente — TC-039 e controle negativo

`TC-039 — MS-08 estados de qualidade em silencio degenerado` (`tests/test_analysis.cpp`), com um
caso por condição (C0 ausente, C1 vazio, C2 curto, C3 poucos quadros, C4 zero digital) mais duas
não-regressões: valor e estado em captura válida, e o Dynamic Range em silêncio digital (§G.5).

**Controle negativo executado:** com a classificação desativada, **TC-039 falha em 7 checks**;
restaurada, passa. TC-038 permanece passando — o valor do P1-001 não mudou.

### G.3 Validação em ambiente

| Ambiente | Build | Resultado |
|---|---|---|
| Linux (headless) | CMake Release | **481 checks, 0 falhas** *(era 461; +20)* |
| **Windows (Vostro)** | Ninja **Release**, `…\aamp_app_artefacts\Release\AAMP.exe` | suíte aprovada; aplicação executada e validada |

**Convergência C++ ↔ JS verificada nas cinco condições**, com o `computeMS08()` **real extraído do
HTML de produção** — estados e contagem de quadros idênticos em ambas as implementações.

**Exportação verificada sobre CSV real:** `NotApplicable` → `N/A`; `LowConfidence` → valor com
sufixo `*`; `NoiseFloor_frames` sempre presente. **`−300 dBFS` nunca é exportado como resultado
válido.**

### G.4 Gate visual no WebView nativo

Executado com o *fixture* sintético mínimo **`p1002_c4_digital_silence.aamp`**
(SHA-256 `c97e1be1fad6bc821ad59bc143b26119acfdc9f93f2e9d8cfeab0450b7333954`), produzido pelo
`EvidenceSerializer` canônico e **não versionado**: apenas ST-09, 48 kHz, 48 000 amostras
(22 janelas ≥ 16), **todas `0.0` exatamente**. Exercita o caso **C4** sem nova aquisição.

Observado no AAMP nativo (WebView2): importação concluída, interface responsiva, tela Integridade
renderizada, **ausência visual de `NaN`/`Infinity`**, MS-08 apresentado como **PISO −300,0 dBFS**,
sessão rotulada **"Medição confiável"**; SNR e Dynamic Range indisponíveis no pacote mínimo
(sem ST-03/ST-08 as famílias sequer são calculadas). **Evidência direta: capturas de tela.**

> **Nota de método.** A importação de um `.aamp` ocorre **integralmente no frontend**
> (`ingest()` → `fromAamp()` → `LabEngine.run()`) e **não produz eventos no `diag_log.txt`**.
> Nenhuma sessão de log evidencia o gate; o papel do log aqui é **exclusivamente a proveniência do
> binário** (§G.8).

### G.5 A sentinela `−300` permanece temporariamente necessária

O campo numérico de `NoiseFloor_dBFS` **conserva o *clamp* −300 dBFS** mesmo quando
`valuePresent = false`. Isso é **deliberado e temporário**:

`AnalysisEngine::dynamicRange()` localiza `NoiseFloor_dBFS` **por chave** e testa apenas a presença
da chave — **ignora `nf.quality`**. Substituir o campo por outro valor alteraria o Dynamic Range,
o que está **fora do escopo do P1-002**. TC-039 fixa essa não-regressão: em silêncio digital o
MS-09 continua `Normal`, com `DynamicRange_dB > 200`.
*(Guarda **retirada no P1-004**, o item autorizado a alterar o MS-09; a condição passou a ser
protegida normativamente pelo **TC-046**, caso (5) — ver §K.3.)*

> **Condição de remoção:** a sentinela só poderá sair do campo quando o MS-09 passar a honrar
> `nf.quality` — o que pertence a **P1-003 / P1-004**. Até lá, `−300` permanece no contrato interno
> e é neutralizado na exportação por `valuePresent = false`. Relacionado a **DV-5** e a **F6**.
>
> **Atualização (P1-004, 2026-08-08).** A condição está **cumprida**: o MS-09 passou a honrar
> `quality`/`valuePresent` do piso. A sentinela **permanece deliberadamente no campo do MS-08**
> (decisão **D3** do P1-004) — o que mudou é que **nenhum consumidor normativo a interpreta mais
> como medição**. Ver *Registro de Encerramento — P1-004* → §K.1.

### G.6 Divergências — estado final

| ID | Divergência | Estado |
|---|---|:--:|
| **DV-1** | Observação inexistente (`fr2 == 0`) reportada como medição válida | ✅ **Resolvida** |
| **DV-2** | Piso não-mensurável (`integFull == 0`) reportado como valor | ✅ **Resolvida** |
| **DV-3** | Observação insuficiente não sinalizada | ✅ **Resolvida** |
| **DV-4** | **Silêncio sujo não detectado** | 🔴 **Aberto — Pilar 2** |
| **DV-5** | Sentinela `−300` ambígua no campo numérico | ✅ **Resolvida no P1-004** — nenhum consumidor normativo a lê como medição; sentinela conservada no campo por **D3** (§K.1) |
| **DV-6** | **CC-A: ST-09 ausente — C++ emite `Error`, JS omite a família** | 🔴 **Aberto** |
| **DV-7** | CC-B: estado fixo `'Normal'` no caminho da evidência | ✅ **Resolvida** (escopo MS-08) |
| **DV-8** | **Apresentação consome a sentinela e a exibe como valor científico** | 🔴 **Aberto — Pilar 2** |

**DV-4 — Aberto (Pilar 2).** Excluída do P1-002 por decisão: detectar sinal intencional exige um
critério quantitativo, e **N4 atribui limiares ao Pilar 2**. O MS-08 ainda pode reportar um nível de
sinal como "Noise Floor" se o ST-09 estiver contaminado.

**DV-6 — Aberto.** Unificar o contrato faria surgir uma família nova no *payload* da UI, o que a
decisão de escopo (não alterar comportamento de interface) veda. **CC-A está satisfeita nas
condições degeneradas, mas não nesta.**

**DV-8 — Aberto (Pilar 2) · confirmado no WebView nativo.** `LAB.getMetric()` (`html:2647`) e
`val()` (`html:2210`) filtram **apenas por finitude** — `e.value != null && isFinite(e.value)` — e
**não consultam `valuePresent` nem `quality`**. O contrato interno do MS-08 chega íntegro
(`quality = NotApplicable`, `valuePresent = false`), mas a camada de apresentação consome a
**sentinela numérica −300** e a exibe como valor científico válido, inclusive na frase
interpretativa do painel lateral. **Defeito de apresentação; não corrigido.** Relacionado a **V2**.

**V1 / P1-008 — Aberto · confirmado no WebView nativo.** Registrado em Seção D (V1).

### G.7 Matriz de rastreabilidade — P1-002

| Requisito / origem | Documento normativo | Implementação | Evidência | Estado |
|---|---|---|---|:--:|
| Validade condicionada a observação suficiente | `noise-floor-definition.md` §7 (Estabilidade) | `fr2 == 0` → `NotApplicable`; `fr2 < 16` → `LowConfidence` | TC-039 (C1, C2, C3) | ✅ |
| Piso não-mensurável sinalizado | **N4** ("piso não-mensurável") | `integFull == 0` → `NotApplicable` | TC-039 (C4) | ✅ |
| Condição declarada junto ao número | `noise-floor-definition.md` §7 (Rastreabilidade) | `NoiseFloor_frames` sempre válido | TC-039 (C1 × C4) | ✅ |
| Propagação de estados de qualidade | princípio **D4** | `r.quality` deixa de ser incondicional | TC-039 · CSV | ✅ |
| ST-09 ausente → falha | comportamento preexistente | `Error` preservado | TC-039 (C0) | ✅ |
| `NotApplicable` não exportado como medição | decisão de escopo (Q3) | `valuePresent = false` | CSV real → `N/A` | ✅ |
| MS-09 inalterado | decisão de escopo (Q4) | *clamp* conservado no campo | TC-039 (C4) · §G.5 | ✅ |
| **CC-A** — C++ ↔ JS | Plano Mestre | `computeMS08()` espelhado | 5 condições idênticas | ⚠️ parcial (**DV-6**) |
| **CC-B** — estado no caminho da evidência | V2 | fim do `'Normal'` literal | montagem `mr08` | ✅ escopo MS-08 |
| Regressão permanente | critério de aceite | TC-039 | controle negativo (7 falhas) | ✅ |
| Silêncio sujo | **N4** | — | — | 🔴 **DV-4 → Pilar 2** |

### G.8 Proveniência das sessões (rastreabilidade factual)

Duas sessões distintas do **mesmo binário**, ambas com
`CommitFull = 18f64abb3db6dcb9505e4ee2aae65075ef5b3c38`, `GitDirty: false`, `BuildConfig: Release`,
`ExecutablePath = C:\aamp\build\windows-ninja\aamp_app_artefacts\Release\AAMP.exe`, WebView2 ativo
em runtime (Chromium 151 via Edg/151):

| Sessão | Conteúdo | Encerramento |
|---|---|---|
| `20260807_102309-14344-1` | **Nova caracterização física da Helix** com preset completamente limpo (sem blocos de efeito): 16 capturas via ASIO Helix, 10:29:51–10:31:38. Realizada **intencionalmente** para gerar sessão e log atuais desta etapa. | rodapé não capturado (log copiado com a aplicação aberta) |
| `20260807_111503-20508-1` | Sessão de aplicação, 11:15:03–11:16:04 | **`Exit: Normal`**, `Elapsed 00:01:01.603` |

> **Registro explícito, para evitar conflação futura:** a caracterização das 10:29 **não é** o gate
> visual do P1-002 e **não é utilizada** para reabrir ou reinterpretar o P1-001, nem para antecipar
> o P1-003. Ela consta apenas como fato de rastreabilidade desta etapa. A evidência do gate são as
> **capturas de tela**; a do encerramento normal é a sessão `…-20508-1`.

### G.9 Encerramento

**P1-002 APROVADO e CONCLUÍDO** no escopo autorizado de **cálculo, contrato semântico e
exportação**. Permanecem abertos e explicitamente rastreados: **DV-4** e **DV-8** (Pilar 2),
**DV-6** (CC-A), **V1/P1-008** (Trust Ladder) e a sentinela `−300` (§G.5, condicionada a
P1-003/P1-004). Nenhum deles foi corrigido neste item.
*(A pendência da sentinela foi posteriormente fechada pelo **P1-004** — §K.1.)*

---

## Registro de Decisão — F2 (`Dynamic Range` × `SNR`) — **NORMATIVO**

> **Status: DECISÕES RATIFICADAS.** Documentação apenas — nenhum código, teste, estímulo, frontend,
> Evidence Package, limiar ou algoritmo foi alterado.
> **Base:** Estudo Conceitual de F2 · **Documento normativo precisado:**
> [`dynamic-range-definition.md`](./dynamic-range-definition.md) §6.

### H.1 Separação entre evidência, derivação e interpretação

| Camada | Conteúdo |
|---|---|
| **Evidência medida** | Caracterização física Helix / preset AC30 Clean: `NoiseFloor = −96,2296 dBFS`, `DR = 80,1 dB`, `SNR = 80,4 dB` ⇒ `DR − SNR = −0,30 dB`. Piso de banda e piso de banda larga coincidem em **0,0002 dB** (reprocessamento offline do P1-001). |
| **Derivação analítica** | Do gerador: ST-03 `[2 s, 4 s)` = seno de **1 kHz a −6 dBFS de pico** (−9,0103 dBFS RMS); ST-08 degrau 8 = seno de **1 kHz a −6 dBFS de pico**. **Mesmo sinal nominal** ⇒ numa cadeia LTI ideal, `teto = referência` e `DR − SNR = 0`. Com o *offset* de **913 amostras** registrado no `diag_log`, a janela de 100 ms recebe **19,02 %** do degrau anterior (−8 dBFS), produzindo viés de teto de **−0,3161 dB**. |
| **Interpretação normativa** | O desvio observado (−0,30 dB) é **compatível** com o viés derivado (−0,3161 dB); a diferença é **0,016 dB**. Conclui-se que `DR < SNR`, no caso analisado, **não representa fisicamente uma referência acima do teto**, e sim um **viés do estimador de teto**. |

*O nível absoluto cancela na diferença `DR − SNR`, pois os dois pisos coincidem — logo a comparação
não depende do ganho da cadeia nem da resposta em frequência do DUT.*

### H.2 Decisão B — relação `Dynamic Range ≥ SNR`

**Ratificada como propriedade condicional**, decorrente de `referência ≤ teto`. **Não** é invariante
numérica obrigatória. Vedado: `clamp`, `max(DR, SNR)` ou equivalente, e alteração de estímulos para
fabricar a desigualdade. Redação normativa incorporada à §6 da `dynamic-range-definition.md`.

*Fundamento:* com o suíte atual, teto e referência são **o mesmo sinal nominal** — a desigualdade
tem **margem zero de projeto** e qualquer viés de estimador a decide. Elevá-la a obrigação exigiria
alterar o estímulo ou mascarar o viés.

### H.3 Decisão D — teto censurado pelo limite superior do ensaio

**Ratificado o princípio:** quando o ST-08 não alcança saturação/clipping, o maior nível observado
**não demonstra** o início da saturação — sabe-se apenas que a cadeia **suporta ao menos** aquele
nível. O teto está **censurado**, e o Dynamic Range dele derivado é um **limite inferior**.

Constatado na evidência: o pico do ST-08 retornou a **−8,3 dBFS**, com o ST-08 terminando em
−6 dBFS e o gate de clipping em `peak < 1.0` — **nenhum degrau clipou**.

> **Escopo desta etapa:** princípio **registrado**. **Não** foram implementados estado de qualidade,
> alteração de exportação, de interface ou de cálculo.

### H.4 Impactos registrados

| Item | Impacto |
|---|---|
| **P1-003** | Limitar a banda **não corrige** F9 — a contaminação é temporal (F9 foi resolvido à parte, em `cfb95fa`). Restrição que **permanece**: 100 ms = 4800 amostras comporta **uma única** janela de 4096 ⇒ estimativa espectral de um só quadro, sem média. |
| **P1-004** | Teto enviesado ~0,32 dB para baixo aproxima marginalmente a degeneração. A decisão **D**, se um dia implementada, acrescenta um segundo caso (teto censurado). *(Executado: o **núcleo semântico** tratou a degeneração `teto ≤ piso`; a decisão **D** foi **desmembrada** e permanece aberta — §K.6.)* |
| **P1-005** | **Nenhum sobre F2** — os pisos já coincidem em 0,0002 dB. Confirmado empiricamente que F2 **não** é problema de piso. |
| **P1-006** | **Mitiga parcialmente:** isolar a energia do tom remove do numerador a contaminação de 0,95 % e o ruído/distorção ⇒ referência **diminui**, favorecendo `DR ≥ SNR`. |
| **CC-A** | **Preservado.** `computeMS09`/`computeMS10` replicam a mesma segmentação e o mesmo recorte ⇒ **carregam o mesmo viés**. Não há descompasso entre implementações. |
| **Estímulos** | **Nenhuma alteração** (vedada por B). |

### H.5 Encerramento

**F2 encerrado.** Decisões **B** e **D** ratificadas e incorporadas à especificação normativa.
O achado derivado — **F9**, segmentação temporal — foi posteriormente **resolvido pela alternativa
A2** e **validado independentemente no Windows** (ver *Registro de Encerramento — F9*).

---

## Registro de Encerramento — F9 (Segmentação Temporal do MS-09) — **NORMATIVO**

> **Estado: RESOLVIDO · VALIDADO INDEPENDENTEMENTE NO WINDOWS.**
> **Natureza:** item **próprio** e **pré-requisito** do P1-003 — **não** incorporado a ele.
> **Origem:** Estudo Conceitual de **F2**. **Alternativa adotada:** **A2**.

### I.1 Correção implementada — exclusivamente temporal

```
fonte ST-08 ──► extractEnvelope ──┐
                                  ├──► bestLag(maxLag = 0,050·fs) ──► lag
captura ST-08 ──► extractEnvelope ┘
                                        ▼
                    slice(s·seg + lag, (s+1)·seg + lag)
                                        ▼
                        cálculo do teto — INALTERADO
```

Mesma convenção já empregada pelo **MS-13** sobre a mesma captura; nenhuma primitiva nova, nenhum
segundo algoritmo de correlação, nenhuma terceira convenção de alinhamento. `maxLag` reutiliza a
política de latência de 50 ms já vigente em MS-13/MS-14 — **não é limiar novo**.

**Garantias preservadas — verificadas no diff e na validação independente:**

- ✅ **A correção é exclusivamente temporal.** O lag atua **somente sobre índices de leitura**.
- ✅ **Nenhum buffer bruto foi alterado.** Nada é deslocado, filtrado ou reamostrado; o áudio e o
  Evidence Package permanecem intactos.
- ✅ **Nenhuma definição espectral do teto foi antecipada.** Banda, RMS, gate `peak < 1.0`,
  progressão do ST-08, duração dos degraus, Noise Floor, `teto ≤ piso` e teto censurado
  permanecem exatamente como estavam. **P1-003 não foi iniciado.**
- ✅ **MS-13 permaneceu inalterado** (`LevelTransfer_step8_dB` invariante).
- ✅ **`ENABLE_SYNC_ALIGNMENT` continua `false`** e não foi tocado.
- ✅ **ST-08 byte-identicamente inalterado.**

**Fonte ST-08 ausente → `NotApplicable`** com `valuePresent = false`. O fatiamento desalinhado
**não** é usado como *fallback* — é sabidamente enviesado. No caminho **nativo** a fonte é
regenerada deterministicamente pelo `StimulusGenerator`, tornando o ramo inalcançável; no
**reprocessamento/importação** a fonte vem do Evidence Package e pode faltar. A diferença está
documentada no próprio código, sem uniformização artificial.

### I.2 Regressão permanente — TC-041

`TC-041 — MS-09 teto invariante a latencia (F9: alinhamento temporal)`, com **controle positivo** e
**controle negativo** demonstrados separadamente. O controle negativo é um **oráculo independente
implementado no próprio teste** — nenhum código de produção antigo foi preservado para isso.

| Offset | Viés do fatiamento antigo | Pós-F9 |
|---:|---:|---|
| 0 amostras | ~0 dB | invariante (referência) |
| **913** | **−0,3157 dB** | invariante em **0,01 dB** |
| **2400** | **−0,8859 dB** | invariante em **0,01 dB** |

Verifica também que o viés antigo é efetivamente não-nulo para offsets > 0 (impedindo aprovação por
acaso) e a **não-regressão do MS-13**.

### I.3 Validação independente no Windows — **APROVADO**

Executada por agente independente do autor do commit, sobre o repositório remoto como fonte de
verdade.

| Item | Evidência |
|---|---|
| Branch | `claude/aamp-migration-stage-0-fzuot8` |
| HEAD local = remoto | `cfb95fa600f31e7844552f56b30f7ac018757fdb` |
| Plataforma / *toolchain* | Windows · MSVC 14.51.36231 x64 |
| Configuração | `Release` |
| Build | **0 warnings, 0 errors** |
| Executável | `C:\aamp\build\windows-ninja\aamp_app_artefacts\Release\AAMP.exe` |
| SHA-256 | `2145781C1F61742FAAF5D42C94050C05E2956CC83A24ED0CC5DEAE670E4C80AE` |
| `aamp_headless_suite` | **497 checks passed, 0 failed** |
| `aamp_scientific_validation` | **8 checks passed, 0 failed** |
| CTest | **2/2 aprovados** |
| Casos confirmados | **TC-030 · TC-034 · TC-038 · TC-039 · TC-041** |
| Escopo do commit | exclusivamente `AnalysisEngine.cpp`, `aamp_laboratory.html`, `test_analysis.cpp` |
| ST-08 | byte-identicamente inalterado |
| `ENABLE_SYNC_ALIGNMENT` | permanece `false` |
| Árvore Git | limpa quanto a arquivos versionados |

**TC-041 confirmado na execução independente:** offsets `0`, `913` e `2400` amostras; controle
negativo dos vieses antigos **−0,3157 dB** e **−0,8859 dB**; invariância pós-F9 dentro de
**0,01 dB**; ausência de regressão do MS-13.

**Classificação: APROVADO.**

### I.4 Rastreabilidade final

```
F2 (estudo conceitual)
   └─► F9 — segmentação temporal identificada como causa dominante
          └─► temporal-segmentation-impact-study.md — A1 · A2 · A3 · A4a · A4c
                 └─► decisão: A2 (menor escopo · precedente MS-13 · rastreabilidade)
                        └─► item PRÓPRIO, pré-requisito do P1-003 (não incorporado)
                               └─► cfb95fa  (AnalysisEngine.cpp · aamp_laboratory.html · test_analysis.cpp)
                                      └─► TC-041 (controle positivo + controle negativo)
                                             └─► validação independente Windows — APROVADO
                                                    SHA-256 2145781C…C80AE · CTest 2/2
```

### I.5 Encerramento

**F9 RESOLVIDO e VALIDADO INDEPENDENTEMENTE.** A divergência temporal do MS-09 está fechada e o
pré-requisito do **P1-003** está cumprido. **P1-003 permanece não iniciado** e depende de nova
autorização. As decisões pendentes registradas no estudo de impacto que **não** foram alcançadas
por F9 — notadamente a convivência com o **teto censurado** (decisão **D**) — permanecem abertas.

---

## Registro de Encerramento — P1-003 (Banda do Teto do MS-09) — **NORMATIVO**

> **Estado: CONCLUÍDO — validado independentemente no Windows.**
> **Referência normativa:** `dynamic-range-definition.md` §4 (teto e piso na **mesma banda**) e §5
> (DC, subsônicos e ultrassônicos excluídos **também no teto**) · achado **MS09-A2** · decisão **D2**.
> **Pré-requisito cumprido:** **F9** (segmentação temporal) encerrado em `cfb95fa`.

### J.1 Solução técnica

Antes, o teto era o **RMS de banda larga no tempo**, enquanto o piso (MS-08) já era band-limited
20–20 k com normalização ENBW/Parseval — bandas distintas tornavam a subtração `teto − piso`
dimensionalmente incoerente e deixavam energia fora da banda inflar o teto.

| Elemento | Estado após P1-003 |
|---|---|
| Grandeza do teto | **nível RMS in-band 20 Hz–20 kHz** |
| Estimador | **o mesmo do MS-08**: Hann, `N = 4096`, mesma integração de bins, mesma compensação **ENBW/Parseval** |
| Quadro de análise | **4096 amostras centradas** no degrau de 4800 (352 de margem de cada lado) |
| Alinhamento temporal | **F9 aplicado antes** da segmentação — ordem obrigatória preservada |
| Gate de clipping | **preservado no sinal temporal bruto**, antes da estimativa espectral |
| Energia fora da banda | **DC, subsônica e ultrassônica excluídas do teto** |
| C++ ↔ JS | **convergentes** |
| MS-08 | **numericamente preservado** após a extração de `officialBandPower()` |
| MS-13 | **inalterado** |

**Ordem obrigatória:** alinhar (F9) → segmentar o degrau → gate temporal no sinal bruto → só então
estimar o nível in-band. O gate permanece temporal por **necessidade física**: saturação é fenômeno
de amplitude no tempo e **não é detectável num espectro restrito à banda**.

**Coerência metrológica por construção:** mesmo `N`, mesma janela, mesmas bordas de bin e mesma
normalização em teto e piso ⇒ `DR = teto − piso` passa a ser a diferença de dois níveis medidos pelo
**mesmo estimador**, e não uma coincidência numérica.

**Extração da primitiva no frontend.** A matemática Hann + ENBW/Parseval + integração 20–20 k foi
promovida a primitiva científica compartilhada `officialBandPower()`, **extraída mecanicamente** do
`computeMS08` — nenhuma operação, ordem de soma ou constante alterada. Consumidores: **MS-08** (piso,
média de Welch sobre quadros) e **MS-09** (teto, quadro único centrado). **Identidade numérica do
MS-08 provada bit-a-bit** antes/depois em três níveis de ruído (−60 / −95 / −120 dBFS): mesmos
dígitos e mesmos 233 quadros.

**Commit:** `ffa739c04d20d9c4aeccab45cc97556e0500da29`.

### J.2 Resultados quantitativos

Teto ideal do degrau mais alto (seno de 1 kHz a −6 dBFS de pico): **−9,0103 dBFS**.

| Caso | **Teto P1-003** | Oráculo banda larga *(controle negativo)* | Viés do estimador antigo |
|---|---:|---:|---:|
| limpo, offset 0 | **−9,0103** | −9,0103 | — |
| limpo, offset 913 | **−9,0103** | −9,3260 | *(efeito F9)* |
| limpo, offset 2400 | **−9,0103** | −9,8962 | *(efeito F9)* |
| + DC 0,02 | **−9,0103** | −8,9965 | **+0,0138 dB** |
| + 5 Hz a −20 dBFS | **−9,0089** | −8,8408 | **+0,1695 dB** |
| + 22 kHz a −20 dBFS | **−9,0103** | −8,8408 | **+0,1695 dB** |
| ganho 4× → saturação | **−6,9691** | −6,9691 | gate rejeitou os degraus saturados |

> **Em cadeia limpa o valor não muda.** A correção atua **exclusivamente** onde existe energia fora
> da banda — exatamente o que a §5 determina.

**CC-A:** `computeMS09()` do frontend reproduz os **sete** casos com os **mesmos valores** do C++.

### J.3 Proteção permanente — TC-043

`TC-043 — MS-09 teto restrito a banda 20 Hz-20 kHz (P1-003)` cobre:

| Verificação | Conteúdo |
|---|---|
| ST-08 limpo | teto ≈ **−9,0103 dBFS** (tolerância 0,01) |
| Invariância a offsets | **0, 913, 2400** amostras — não-regressão de **F9** |
| Rejeição de **DC** | componente contínua não eleva o teto |
| Rejeição de **5 Hz** | subsônica não eleva o teto |
| Rejeição de **22 kHz** | ultrassônica não eleva o teto |
| **Controle negativo** | oráculo **independente**, implementado no próprio teste, reproduzindo o viés do estimador de banda larga anterior; verifica também que o viés é efetivamente não-nulo (impede aprovação por acaso) |
| Gate temporal de clipping | sob ganho 4×, o degrau saturado continua rejeitado e o teto vem do degrau 5 |
| MS-13 | sem regressão na cadeia limpa |

### J.4 Validação independente no Windows — **APROVADO**

| Item | Evidência |
|---|---|
| Branch | `claude/aamp-migration-stage-0-fzuot8` |
| HEAD local = remoto | `ffa739c04d20d9c4aeccab45cc97556e0500da29` |
| Plataforma / *toolchain* | Windows · MSVC 14.51.36231 x64 |
| Configuração · `GitDirty` | `Release` · **`false`** |
| Build | **0 warnings, 0 erros** |
| Executável | `C:\aamp\build\windows-ninja\aamp_app_artefacts\Release\AAMP.exe` |
| SHA-256 | `DFE1E17F5D95DF470AD03E45FD2DF40787E59BADE6BBE2EE376993B7B569EFD0` |
| `aamp_headless_suite` | **517 checks passed, 0 failed** |
| `aamp_scientific_validation` | **8 checks passed, 0 failed** |
| CTest | **2/2 aprovados** |
| Casos confirmados | **TC-030 · TC-034 · TC-038 · TC-039 · TC-041 · TC-043** |

### J.5 Rastreabilidade histórica dos Evidence Packages

> **Pacotes `.aamp` anteriores continuam integralmente válidos como evidência bruta** — o áudio
> preservado não foi alterado por nenhum item do Pilar 1.
>
> Entretanto, **o reprocessamento científico de um pacote antigo com uma versão posterior do AAMP
> pode produzir um `MS-09` diferente do relatório originalmente exportado**, porque a ciência do teto
> foi corrigida (banda, em P1-003) — assim como o `MS-08` já podia divergir por P1-001 (ENBW) e o
> `MS-09` por F9 (alinhamento). **Isto não é inconsistência da evidência: é evolução do estimador
> sobre a mesma observação.**
>
> **Um resultado histórico deve sempre ser interpretado junto da versão/commit que o produziu.** A
> infraestrutura de diagnóstico (`DiagLog`, commit `8fb6849`) registra `CommitFull`, `BuildConfig` e
> `GitDirty` no cabeçalho de cada sessão exatamente para tornar essa distinção rastreável.

### J.6 Encerramento e itens explicitamente preservados

**P1-003 CONCLUÍDO — validado independentemente no Windows.** Permanecem, sem alteração:

- 🔴 **Decisão D / teto censurado — não implementada.** Nenhum escalar novo foi criado; índice do
  degrau e `algumDegrauClipou` **não** são expostos. A arquitetura dessa representação será decidida
  separadamente, com o **P1-004**.
  *(Situação no P1-004: **desmembrada por fronteira arquitetural** — continua **não resolvida**,
  agora como **item próprio e separado**. Ver §K.6.)*
- 🔴 **DV-5 / sentinela `−300` — aberta.** O MS-09 continua lendo `NoiseFloor_dBFS` por chave,
  ignorando `nf.quality`. *(**Fechada no P1-004** — §K.1.)*
- ⬜ **P1-004 e P1-005 — não iniciados.** *(**P1-004: núcleo semântico concluído** e validado no
  Windows — §K. **P1-005** segue não iniciado.)*

Também inalterados: **MS-10**, **MS-13**, **ST-08**, `ENABLE_SYNC_ALIGNMENT`, Noise Floor,
exportação, Evidence Package e apresentação.

---

## Registro de Encerramento — P1-004 (Núcleo Semântico do MS-09) — **NORMATIVO**

> **Estado: NÚCLEO SEMÂNTICO CONCLUÍDO — validado independentemente no Windows.**
> **Qualificador obrigatório:** este registro encerra **exclusivamente o núcleo semântico** do
> P1-004. A **representação do teto censurado (decisão D) permanece item aberto e separado** —
> ver §K.6. Nenhuma menção a "P1-004 encerrado" pode ser feita sem esse qualificador.
> **Referência normativa:** `dynamic-range-definition.md` §7 (*Tratamento da degeneração*) e §6
> (dependência unidirecional do Noise Floor) · decisão **D3** · divergência **DV-5**.
> **Pré-requisitos cumpridos:** **F9** (`cfb95fa`) e **P1-003** (`ffa739c`).

### K.1 O que o P1-004 resolveu

O Dynamic Range **deriva** do Noise Floor (`DR = teto − piso`). O MS-09 lia o piso **apenas por
chave** e ignorava `quality`/`valuePresent` do escalar do MS-08 — de modo que um piso **não
caracterizável** entregava a sentinela `−300 dBFS` como se fosse medida. O núcleo do P1-004
estabelece:

- o MS-09 **não pode** interpretar o valor sentinela `−300 dBFS` do MS-08 como medição válida;
- piso `NotApplicable` / sem valor → MS-09 `NotApplicable`, `valuePresent = false`;
- piso `LowConfidence` → MS-09 `LowConfidence`, **mantendo presente o valor calculado**;
- ausência de degrau válido → `NotApplicable`, sem valor;
- `teto ≤ piso` → `NotApplicable`, sem valor;
- **nenhum** Dynamic Range negativo é apresentado como medição;
- **nenhum** `abs()`, `clamp` ou substituição artificial por zero;
- a medição normal permanece **numericamente inalterada** (`≈ 86,789489 dB` no banco validado);
- **C++ e JS preservam o mesmo contrato**.

**Contrato implementado (decisão D1, Alternativa II)** — idêntico no Core e no LabEngine:

| Estado do **piso** (MS-08) | Estado do **DR** (MS-09) | `valuePresent` |
|---|:--:|:--:|
| `Error` | `Error` | *(família não emite escalar)* |
| `NotApplicable` / sem valor | `NotApplicable` | `false` |
| `LowConfidence` | `LowConfidence` | **`true`** — medição válida com confiança reduzida |
| *(piso válido)* nenhum degrau válido | `NotApplicable` | `false` |
| *(piso válido)* `teto ≤ piso` | `NotApplicable` | `false` |
| *(piso válido)* medição normal | `Normal` | `true` |

A sentinela `−300 dBFS` **permanece no campo do MS-08** (decisão **D3** desta etapa). O que mudou é
que o MS-09 **nunca mais a interpreta como medição**. A matemática do MS-08 **não** foi tocada;
`MetricResult` e o enum `Quality` permanecem **inalterados**; a **exportação não foi alterada** —
`ExportEngine::formatScalar` já rendia `N/A` para valor ausente e o sufixo `*` para baixa confiança,
o que foi conferido em CSV real.

No LabEngine, a **presença do valor passa a ser decidida pela família** (`valuePresent`) em vez de
inferida de `quality === 'Normal'` — inferência que descartava silenciosamente o valor de uma
medição `LowConfidence`.

### K.2 Caso-limite `teto = piso`

O operador implementado é `<=`, exatamente como a §7 exige (*"inclui o caso-limite `teto = piso`"*).
Registra-se a **observação** de que a igualdade exata **não é construível em ponto flutuante**: ela é
um conjunto de medida nula, e qualquer realização cai de um dos lados por ~`1e-15` dB. O TC-046
delimita o ponto por um **par de casos a ±1e-6 dB** — nove ordens de grandeza acima do ruído
numérico e muito abaixo de qualquer degeneração fisicamente distinguível —, obtidos pela
**homogeneidade exata do estimador** (multiplicar o ST-09 por `g` escala a potência por `g²` e
desloca o piso em exatamente `20·log10(g)` dB). Isto é propriedade da aritmética de precisão dupla,
**não** defeito da regra.

### K.3 Proteção permanente — TC-046

**Identificador:** o `TC-045` autorizado **já estava ocupado** por `tests/e2e/E2E_Specs.cpp`
("TC-045 Tooltip multi-curva"); a reconfirmação exigida imediatamente antes da edição detectou a
colisão e adotou-se o próximo identificador livre da mesma faixa — **TC-046**, mesmo procedimento
aplicado antes em `TC-042 → TC-043`.

Cobre **10 condições** — referência normal (com asserção de não-regressão numérica em
`86,789489 dB`), ST-08 ausente, piso `Error`, piso `NotApplicable` por ausência de observação, piso
`NotApplicable` por silêncio digital bit-exato, piso `LowConfidence`, todos os degraus saturados,
degeneração franca (piso 40 dB acima do teto) e os dois casos-limite a ±1e-6 dB — mais **3
controles negativos independentes**, que varrem os 10 pacotes com critérios que **não dependem** da
classificação esperada de cada caso:

| Controle | O que impede de reintroduzir |
|---|---|
| **CN-1** | nenhum DR **negativo** jamais apresentado como valor (**D6**) |
| **CN-2** | nenhum DR acima de 200 dB — a sentinela `−300` nunca volta a virar medição (**DV-5**) |
| **CN-3** | estado da família e do escalar nunca divergem; valor ausente nunca vem rotulado `Normal` |

**Retirada da guarda Q4 do TC-039.** O TC-039 carregava uma asserção que travava o MS-09 no
comportamento anterior (`Normal`, `> 200 dB`) em silêncio digital. Ela existia **especificamente**
para impedir que o **P1-002** alterasse o MS-09 por efeito colateral — era **proteção temporária de
escopo daquele item**, não propriedade do MS-08. O **P1-004 é justamente o item autorizado a alterar
esse comportamento**, de modo que a retirada está correta; a condição passou a ser protegida
**normativamente pelo TC-046, caso (5)**. Todas as asserções do TC-039 sobre o MS-08 permanecem
intactas, inclusive a de que a sentinela `−300` sobrevive no campo.

### K.4 Convergência C++ ↔ JS (CC-A)

Verificada sobre **entradas bit-idênticas** — o Core exportou os vetores de amostras exatos e as
funções do LabEngine foram executadas sobre eles —, comparando **valor, `quality` e `valuePresent`**:

| Condição | C++ | JS |
|---|---|---|
| normal | `Normal` · presente · `86.789489300` | `Normal` · presente · `86.789489300` |
| piso `LowConfidence` | `LowConfidence` · presente · `86.825010854` | `LowConfidence` · presente · `86.825010854` |
| piso `NotApplicable` (silêncio digital) | `NotApplicable` · ausente | `NotApplicable` · ausente |
| piso `NotApplicable` (ST-09 vazio) | `NotApplicable` · ausente | `NotApplicable` · ausente |
| todos os degraus saturados | `NotApplicable` · ausente | `NotApplicable` · ausente |
| `teto < piso` (−40 dB) | `NotApplicable` · ausente | `NotApplicable` · ausente |
| `piso = teto + 1e-6 dB` | `NotApplicable` · ausente | `NotApplicable` · ausente |
| `piso = teto − 1e-6 dB` | `Normal` · presente · `0.000001000` | `Normal` · presente · `0.000001000` |

Convergência integral nas **oito condições comparáveis**. A nona — **fonte ST-08 ausente** — é a
**assimetria legítima preservada** (decisão **D5**): o Core **regenera** o estímulo
deterministicamente, tornando o ramo inalcançável nativamente (`Normal`), enquanto o LabEngine o lê
do Evidence Package e pode não encontrá-lo (`NotApplicable`). Está documentada no código de ambos os
lados e **não** foi corrigida.

### K.5 Validação independente no Windows — **APROVADO**

| Item | Evidência |
|---|---|
| Branch | `claude/aamp-migration-stage-0-fzuot8` |
| Commit validado | `6ed71e80cc3e5b6859ce60e960f9f769569a1f43` |
| Plataforma / *toolchain* | Windows · MSVC x64 |
| Configuração · `GitDirty` | `Release` · **`false`** |
| Configuração e build | **0 warnings, 0 erros** |
| Executável | `AAMP.exe` · **4.659.712 bytes** |
| SHA-256 | `52EEE03C1E52389182C909860B1989F0C59F7D562AEF2F0228F63E937A814079` |
| `aamp_headless_suite` | **588 checks passed, 0 failed** |
| `aamp_scientific_validation` | **8 checks passed, 0 failed** |
| CTest | **2/2 aprovados** |
| Casos confirmados | **TC-030 · TC-034 · TC-038 · TC-039 · TC-041 · TC-043 · TC-046** |

### K.6 Decisão D / teto censurado — **DESMEMBRADA, NÃO RESOLVIDA**

É essencial distinguir:

| | Estado |
|---|---|
| **P1-004 — núcleo semântico** | ✅ **RESOLVIDO E VALIDADO** |
| **Representação de teto censurado (decisão D)** | 🔴 **NÃO RESOLVIDA — item próprio e aberto** |

Durante a implementação **demonstrou-se uma fronteira arquitetural**:

- `ExportEngine::exportScalarsCSV` itera **todos** os escalares de **todas** as famílias **sem
  filtro** (apenas ignora chave vazia) ⇒ **qualquer escalar novo é exportado automaticamente**;
- `MetricResult` possui somente `id`, `name`, `quality`, `scalars`, `curves` — **não há superfície
  para metadados internos**.

Logo, representar a decisão **D** sem alterar a exportação ou o contrato estrutural **não é possível
na arquitetura atual**. A decisão **D foi desmembrada por fronteira arquitetural — não abandonada,
nem implicitamente implementada**. A verificação foi feita mecanicamente sobre o próprio *diff* de
`6ed71e8`: **nenhuma chave de escalar nova**, **nenhuma ocorrência** de termo de censura/limite
inferior, **nenhum arquivo de exportação, contrato ou cabeçalho alterado**.

> **Premissa REFINADA (P1-009, 2026-08-08) — o texto acima é preservado, não corrigido por
> substituição.** A conclusão permanece **correta para a alternativa então considerada**, que era
> acrescentar um **escalar**. A investigação arquitetural posterior examinou uma superfície não
> considerada nesta etapa — o próprio `Scalar` — e estabeleceu que **um membro novo de `Scalar` NÃO é
> exportado automaticamente** pelo `ExportEngine` atual, enquanto **um escalar novo é**. Existe,
> portanto, caminho para transportar a natureza da estimativa sem alterar CSV, PDF ou dashboard.
> **A fronteira é mais estreita do que se registrou aqui.** Ver §L.5.

Sua solução futura exigirá **decisão explícita** entre pelo menos:

1. **evolução do contrato/exportação**;
2. **extensão estrutural de `MetricResult`**;
3. **superfície própria de metadados**.

> **Resolvido em P1-009 (2026-08-08).** A decisão foi tomada e ratificada: **qualificador tipado no
> nível de `Scalar`** (estágio 1, arquitetura **D**), com os fatos primários preservados como
> proposta futura (estágio 2, arquitetura **A**). Nenhuma das três opções acima foi adotada na forma
> em que estavam enunciadas. Ver §L.

Restrições já fixadas para esse item: **não criar novo valor no enum `Quality`**; **não recomputar
alinhamento, segmentação ou espectro posteriormente apenas para redescobrir a censura** — a solução
deverá **preservar os fatos primários já disponíveis durante o MS-09** e decidir como transportá-los
**sem ambiguidade**.

### K.7 Rastreabilidade histórica dos Evidence Packages

> Pacotes `.aamp` anteriores **continuam válidos como evidência bruta** da captura original — nenhum
> item do Pilar 1 alterou o áudio preservado, e **o formato do Evidence Package não foi alterado**.
>
> Entretanto, **o reprocessamento de um pacote antigo por versões posteriores do AAMP pode produzir
> um `MS-09` diferente daquele originalmente calculado/exportado**, por três causas cumulativas: as
> correções **científicas** **F9** (alinhamento temporal) e **P1-003** (banda do teto), e agora as
> correções **semânticas** do **P1-004** — que podem converter um número outrora reportado como
> `Normal` em `NotApplicable`, ou rebaixá-lo a `LowConfidence`. O `MS-08` já podia divergir por
> **P1-001** (ENBW).
>
> **Isto não é inconsistência da evidência: é evolução do estimador e da semântica sobre a mesma
> observação.** Todo resultado histórico deve ser interpretado **junto da versão/commit que o
> produziu**, o que a infraestrutura de diagnóstico (`CommitFull`, `BuildConfig`, `GitDirty`) torna
> rastreável.

### K.8 Achados que permanecem abertos — registrados, **não corrigidos**

| ID | Achado | Estado |
|---|---|:--:|
| **D / teto censurado** | Representação do teto censurado pelo limite superior do ensaio | ✅ **Estágio 1 CONCLUÍDO e validado no Windows** (§M); **Estágio 2 aberto** |
| **F10** | **NaN atravessa MS-08 e MS-09 como `Normal`** — uma captura contendo `NaN` propaga-se pelo estimador e chega ao contrato rotulada como medição válida | 🔴 **Aberto** (decisão **D4**: registrar, não corrigir) |
| **DV-9** | **Assimetria legítima da fonte ST-08** entre Core (regenera ⇒ ramo inalcançável) e reprocessamento (lê do pacote ⇒ pode faltar) | 🟡 **Legítima e preservada** (decisão **D5**) |
| **DV-8** | Apresentação (`LAB.getMetric`, `val`) filtra apenas por `isFinite`, ignorando `valuePresent`/`quality`, e exibe a sentinela `−300` | 🔴 **Aberto — Pilar 2** |
| **V1 / P1-008** | Impacto na **Trust Ladder**: a propagação de `LowConfidence` do MS-08 alcança agora o contrato do MS-09; o efeito na escada de confiança e na apresentação **não foi avaliado nem alterado** | 🔴 **Aberto — P1-008** |
| **F11** | **Comentário residual** em `AnalysisEngine.cpp:790-792`, que ainda afirma que a sentinela permanece "para que o MS-09 mantenha comportamento idêntico" e que honrar `nf.quality` "pertence a P1-003/P1-004" — **inconsistência textual sem efeito funcional** | 🟡 **Registrado, não alterado** — corrigi-lo modificaria o código já validado |

### K.9 Encerramento

**P1-004 — NÚCLEO SEMÂNTICO CONCLUÍDO e validado independentemente no Windows** (`6ed71e8`).
A **representação de teto censurado permanece item aberto e separado**. Permanecem inalterados:
**MS-08** (matemática), **MS-10**, **MS-13**, **ST-08**, `ENABLE_SYNC_ALIGNMENT`, `MetricResult`,
enum `Quality`, exportação, Evidence Package e apresentação.

---

## Registro de Decisão — P1-009 (Natureza da Estimativa / Limites Unilaterais) — **NORMATIVO**

> **Status: DECISÕES RATIFICADAS.** Documentação apenas — **nenhum** código, teste, estímulo,
> frontend, exportação, Evidence Package, contrato (`MetricResult`/`Scalar`) ou `Quality` foi
> alterado. Nenhuma nova caracterização física foi realizada.
> **Situação de implementação (2026-08-08):** o **Estágio 1** desta decisão está **CONCLUÍDO e
> validado independentemente no Windows** — ver *Registro de Encerramento — P1-009, Estágio 1* (§M).
> O **Estágio 2 continua aberto**.
> **Origem:** decisão **D** do MS-09 (teto censurado), desmembrada do **P1-004** por fronteira
> arquitetural, mais a necessidade transversal evidenciada pelo **MS-10 / P1-007**.
> **Base:** [`censored-ceiling-architecture-study.md`](./censored-ceiling-architecture-study.md),
> aprovado como base da decisão. O texto pré-decisão daquele estudo está preservado; o desfecho
> normativo está isolado na §15 dele.

### L.1 Arquitetura aprovada — **D + A em dois estágios**

**Estágio 1 — D: natureza da estimativa.** Qualificador **tipado no nível de `Scalar`**, ortogonal a
`Quality` e a `valuePresent`. Taxonomia inicial aprovada:

| Valor | Significado |
|---|---|
| **`Point`** | estimativa pontual — o valor é a grandeza |
| **`LowerBound`** | o valor é um **limite inferior** — a grandeza é *pelo menos* isso |
| **`UpperBound`** | o valor é um **limite superior** — a grandeza é *no máximo* isso |

**`Point` é o padrão**, para preservar integralmente o comportamento das demais métricas.

O teto censurado do MS-09 passa a ser representado como:

```
quality      = Normal
valuePresent = true
estimate     = LowerBound
```

**Vedado criar `Quality::LowerBound`. Vedado usar `LowConfidence` para representar
unilateralidade** — confiança e unilateralidade são dimensões distintas.

**Estágio 2 — A: fatos primários auditáveis.** `DynamicRange_stepIndex` e
`DynamicRange_firstClippedIndex` ficam **preservados como proposta futura**. **Não implementar** e
**não autorizar sua exportação agora.** O estágio 2 dependerá de decisão explícita sobre
contrato/exportação e **pode ser diferido ao Pilar 3**.

### L.2 Regra formal dos casos do MS-09

Sejam `V` o conjunto de degraus com estimativa in-band válida, `C` o dos rejeitados por saturação
(`peak ≥ 1.0`), `s*` o degrau selecionado e `min(C)` o primeiro saturado.

| # | Condição | Natureza | Fundamento |
|:--:|---|:--:|---|
| 1 | `C ≠ ∅` **e** `min(C) = s* + 1` | **`Point`** | **teto observado** — a saturação foi demonstrada imediatamente após o maior degrau válido, o que permite tratar o valor como estimativa pontual segundo o contrato atual |
| 2 | `C = ∅` **e** `s* = último degrau` | **`LowerBound`** | **teto censurado pelo limite superior do ensaio** — representação científica `Dynamic Range ≥ X dB`, **não** `= X dB` |
| 3 | `C ≠ ∅` **e** `min(C) > s* + 1` | **`LowerBound`** | saturação observada porém **não encaixotada** — ver L.3 |
| 3b | `C = ∅` **e** `s* <` último degrau | **`LowerBound`** | degraus superiores inválidos por outra causa (captura truncada / sem energia in-band). *Arranjo não enumerado na ratificação original, identificado durante a implementação e resolvido por aplicação direta do princípio: sem saturação demonstrada não há estimativa pontual. Ratificado e coberto por TC-047 caso (1b) — ver §M.4.* |
| 4 | `V = ∅` | *(irrelevante)* | `quality = NotApplicable`, `valuePresent = false`. **Ausência de valor nunca vira `LowerBound`** |

### L.3 Terceiro caso — decisão explícita

Para `C ≠ ∅` e `min(C) > s* + 1` — existe saturação observada, mas **não** adjacente ao maior degrau
válido — a estimativa é classificada como **`LowerBound`**. Fundamento:

- sabe-se que o DUT **suporta ao menos** o nível associado a `s*`;
- portanto `DR ≥ X` permanece uma afirmação **verdadeira**;
- **não** há evidência suficiente para afirmar `DR = X`;
- **não** é `NotApplicable`, pois existe informação quantitativa válida;
- **não** é `LowConfidence`, pois unilateralidade e confiança são dimensões diferentes.

Registrado que este caso **poderá** futuramente admitir representação mais rica — por exemplo um
**intervalo** —, mas **essa taxonomia não é introduzida agora**, por falta de necessidade
demonstrada.

### L.4 O regime censurado é o NORMAL, não a exceção

O ST-08 é uma escada de 8 degraus cujo pico máximo é **−6 dBFS**. Uma cadeia de ganho unitário
**nunca satura**, logo recai sempre no caso 2. Observação registrada (sonda descartável fora do
produto, sem alterar código):

```
ganho x1     s*=7  C=∅              -> TETO CENSURADO   (LowerBound)
ganho x2     s*=6  min(C)=7          -> teto observado   (Point)
ganho x4     s*=4  min(C)=5          -> teto observado   (Point)
ganho x100   V=∅   min(C)=0          -> sem valor        (NotApplicable)
```

**Fica explicitamente registrado que o teto censurado é o regime normal para cadeias de ganho
unitário ou inferior a +6 dB com o ST-08 atual — e não um caso raro.** O valor `86,789489 dB`
preservado como não-regressão do P1-004 é, fisicamente, `DR ≥ 86,789489 dB`.

Isto também completa a explicação de **F2** (`DR = 80,1` < `SNR = 80,4`): é exatamente o regime que a
decisão **D** previu, e a inversão observada era o sintoma esperado de teto censurado — **não** viés
de estimador.

### L.5 Correção formal da fronteira arquitetural registrada no P1-004

| Afirmação | Estado |
|---|---|
| Novo **escalar** em `MetricResult::scalars` → **exportado automaticamente** | ✅ **Confirmada** |
| Novo **membro de `Scalar`** → **não** é exportado automaticamente pelo `ExportEngine` atual | ✅ **Estabelecida pela investigação arquitetural** |

Prova registrada:

- um membro acrescentado **ao final** de `Scalar`, com inicializador padrão, **mantém válidas as 76
  inicializações agregadas existentes** em `AnalysisEngine.cpp` — verificado por compilação com
  `-Wall -Wextra -Werror`, C++17, sobre cópia do cabeçalho **fora do repositório**;
- os consumidores atuais (`exportScalarsCSV`, `buildReportModel`, `DashboardModel`,
  `SessionSerializer`) leem campos **explicitamente nomeados** e **ignoram** o membro novo até
  optarem por utilizá-lo;
- portanto **existe caminho para transportar a natureza da estimativa sem alterar CSV, PDF ou
  dashboard na primeira etapa**.

> **Preservação do histórico.** A conclusão registrada no P1-004 (§K.6) — *"representar a decisão D
> sem alterar a exportação ou o contrato estrutural não é possível na arquitetura atual"* — **não é
> apagada**. Ela permanece **correta para a alternativa então considerada**, que era acrescentar um
> **escalar**. É aqui **refinada** por investigação arquitetural posterior, que examinou uma
> superfície não considerada na ocasião: o próprio `Scalar`. **Premissa refinada, não invalidada.**

### L.6 Escopo do P1-009 — estágio 1 (registro de escopo futuro)

> **EXECUTADO** (2026-08-08). O escopo abaixo foi implementado integralmente e sem excedentes;
> a verificação item a item está no *Registro de Encerramento — P1-009, Estágio 1* (§M).

Quando autorizado, o estágio 1 deverá conter **exatamente**:

- extensão **tipada** de `Scalar` com a natureza da estimativa, padrão `Point`;
- MS-09 produzindo `Point` ou `LowerBound` conforme **L.2**;
- espelhamento equivalente no **LabEngine JS** (**CC-A**);
- **derivação por índices/estado do laço**, nunca por comparação de valores `double` (ver L.8).

E **nenhuma** das seguintes: mudança numérica em `DynamicRange_dB`; mudança de `Quality`; mudança de
`valuePresent`; mudança de CSV, PDF ou dashboard; mudança do Evidence Package; mudança do ST-08;
**nenhum novo escalar público**.

### L.7 MS-10 / P1-007 — dependência conceitual

Registrada **apenas** a dependência: `snrDb()` retornando `200 dB` quando `pn <= 0`
(`AnalysisEngine.cpp:112`) é **candidato a `LowerBound`**, e não necessariamente a `LowConfidence`
nem a um novo valor de `Quality`. **O comportamento do MS-10 não é decidido nem implementado agora.**

**P1-007 deverá reutilizar a taxonomia criada por P1-009, não inventar uma segunda solução.**

### L.8 F10 (NaN) e achados latentes

**F10 permanece fora do P1-009.** Registrado como **requisito de implementação** do estágio 1: a
natureza do MS-09 deve ser derivada de **índices e estado do laço**, nunca de comparação de valores
`double` — um classificador numérico herdaria a propriedade de `NaN` de tornar toda comparação falsa
e degradaria em silêncio para `Point`.

Achados **latentes** levantados pelo estudo, registrados **sem correção e sem expansão de escopo**:
`crosstalk` com `1e-30` nos dois lados; `SettlingTime_ms` em que "nunca estabilizou" e "estabilizou
instantaneamente" produzem ambos `0 ms`; `bestLag` saturando no limite de busca de 50 ms. Nenhum foi
verificado a fundo; nenhum será tratado por P1-009.

### L.9 Exportação futura — registro, sem implementação

O canal de máquina **deve preservar o número como número**. Conceito preferencial futuro:

```
DynamicRange_dB=86.789
DynamicRange_dB_nature=LowerBound
```

**Não usar** `DynamicRange_dB=>=86.789` — quebra consumidores numéricos. Interfaces **humanas**
poderão futuramente renderizar `≥ 86.8 dB` com explicação apropriada. **Nada disso é autorizado
nesta etapa.**

### L.10 F11 — comentário residual

O comentário em `AnalysisEngine.cpp:790-792` **não é alterado agora**. Permanece como **dívida
textual**, a ser corrigida quando uma intervenção **funcional** futura tocar a região correspondente.
**Nunca em commit isolado** — alterar código validado sem gate funcional é churn sobre binário
auditado.

---

## Registro de Encerramento — P1-009, Estágio 1 (Natureza da Estimativa) — **NORMATIVO**

> **Estado: ESTÁGIO 1 CONCLUÍDO — validado independentemente no Windows.**
> **Qualificador obrigatório:** este registro encerra **exclusivamente o Estágio 1**. O **Estágio 2**
> (exposição/auditabilidade dos fatos primários) **continua aberto** — ver §M.7. Nenhuma menção a
> "P1-009 encerrado" pode ser feita sem esse qualificador.
> **Referência normativa:** `dynamic-range-definition.md` §6 (decisão **D** / natureza da estimativa)
> · *Registro de Decisão — P1-009* (§L).
> **Pré-requisitos cumpridos:** **P1-004 — núcleo semântico** (`6ed71e8`).

### M.1 A solução do Estágio 1

`Scalar` passa a possuir uma **natureza tipada da estimativa**, **ortogonal** a `Quality` e a
`valuePresent` — o **terceiro eixo** do contrato:

| Eixo | Responde a |
|---|---|
| `Quality` | a medição é **válida / confiável**? |
| `valuePresent` | **existe** um valor numericamente interpretável? |
| `EstimateNature` | o que o valor **afirma** sobre a grandeza? |

Taxonomia: **`Point`** (`= X`) · **`LowerBound`** (`≥ X`) · **`UpperBound`** (`≤ X`).

**`Point` é o padrão** e **preserva integralmente o comportamento das demais métricas**. O membro foi
acrescentado **ao final** de `Scalar` com inicializador padrão: nenhuma das inicializações agregadas
existentes precisou ser editada, e **toda métrica que não se pronuncia continua sendo, por definição,
uma estimativa pontual**.

### M.2 Regra do MS-09 — ratificada e implementada

Com `V` = degraus de estimativa válida, `C` = degraus rejeitados por saturação, `s*` = selecionado e
`min(C)` = primeiro saturado:

| Condição | Resultado |
|---|---|
| teto observado / **encaixotado** — `C ≠ ∅` e `min(C) = s* + 1` | **`Point`** |
| teto **não atingido** — `C = ∅` e `s* = último degrau` | **`LowerBound`** |
| saturação observada mas **não encaixotada** — `C ≠ ∅` e `min(C) > s* + 1` | **`LowerBound`** |
| `C = ∅` e `s* <` último degrau | **`LowerBound`** |
| sem valor caracterizável — `V = ∅` | `NotApplicable` · `valuePresent = false` |

### M.3 Combinações válidas e intencionais dos três eixos

Registra-se **explicitamente** que

```
quality      = Normal
valuePresent = true
estimate     = LowerBound
```

é uma **combinação válida e intencional**: um teto censurado é uma medição **plenamente confiável**,
com valor **existente**; apenas a *quantificação lógica* é unilateral.

Registra-se também que

```
quality      = LowConfidence
valuePresent = true
estimate     = LowerBound
```

é **representável** — teto censurado sobre piso de observação insuficiente —, o que **comprova a
independência dos três eixos**. Permanece **vedado** representar unilateralidade por `LowConfidence`
ou por um valor novo de `Quality`.

### M.4 Evidência que fechou a lacuna do quarto ramo

Durante a implementação foi sinalizado um arranjo **não enumerado** na autorização original:
`C = ∅` **e** `s* <` último degrau — degraus superiores inválidos por captura truncada ou ausência de
energia in-band. Ele foi classificado como **`LowerBound`** por aplicação direta do princípio
ratificado (sem saturação demonstrada não há estimativa pontual), e a regra passou a constar da §M.2.

O commit de teste **`a1751011db5f83c5e2258374adee713642bf24f9`** fecha essa lacuna de forma
**direta e específica**. O TC-047, caso (1b), demonstra:

- preservação dos degraus **0..3** do loopback unitário;
- **truncamento antes do degrau 4** (`resize(4·seg)`), verificado por asserção de tamanho;
- **`peak < 1.0`** em todas as amostras presentes ⇒ **`C = ∅`**, verificado por asserção;
- degraus **4..7 inexistentes**, logo impedidos de integrar `V`;
- **`s* = 3`**, confirmado pela diferença de **≈ 16 dB** contra o caso (1) — exatamente a distância
  entre o degrau 3 (−22 dBFS) e o degrau 7 (−6 dBFS);
- resultado **`Normal` + `valuePresent = true` + `LowerBound`**.

**Controle discriminante** registrado no mesmo teste, caso (2): ganho ×2 produz `C ≠ ∅` com
`min(C) = s* + 1` ⇒ **`Point`**. Sem ele, o caso (1b) poderia passar por um classificador que
devolvesse `LowerBound` incondicionalmente.

### M.5 Validação independente no Windows — **APROVADO**

| Item | Evidência |
|---|---|
| Branch | `claude/aamp-migration-stage-0-fzuot8` |
| HEAD local = remoto | `a1751011db5f83c5e2258374adee713642bf24f9` |
| Plataforma / configuração | Windows · **`Release`** · `dirty = false` |
| Build | **0 warnings, 0 erros** |
| `aamp_headless_suite` | **773 checks passed, 0 failed** |
| `aamp_scientific_validation` | **8 checks passed, 0 failed** |
| CTest | **2/2 aprovados** |
| Casos confirmados | **TC-030 · TC-034 · TC-038 · TC-039 · TC-041 · TC-043 · TC-046 · TC-047** |

### M.6 Não-regressões verificadas

| Item | Estado |
|---|---|
| `DynamicRange_dB` na cadeia padrão | **≈ 86,789489300 dB — numericamente inalterado** |
| CSV escalar | **invariável** — dois bundles que diferem apenas na natureza produzem saída **byte-idêntica** |
| `ReportModel` (PDF) | **invariável** |
| `DashboardModel` | **invariável** |
| Evidence Package | **inalterado** |
| Escalares públicos | **nenhum novo** |
| `Quality` | **inalterado** |
| Demais métricas | continuam **`Point` por padrão**, sem edição alguma |
| **F10 / NaN** | **permanece ABERTO e não corrigido** — a classificação é derivada de índices, nunca de comparação de `double`, de modo que o problema **não foi ampliado** |

### M.7 Estágio 2 — **CONTINUA ABERTO**

| | Estado |
|---|---|
| **P1-009 — Estágio 1** | ✅ **CONCLUÍDO E VALIDADO INDEPENDENTEMENTE NO WINDOWS** |
| **P1-009 — Estágio 2** | 🔴 **ABERTO** — evolução futura |

O Estágio 2 permanece como evolução futura para **exposição/auditabilidade dos fatos primários** e
**eventual integração com exportação/UI**. `DynamicRange_stepIndex` e
`DynamicRange_firstClippedIndex` **não foram implementados** e **não estão autorizados**.

### M.8 Sequenciamento confirmado

- **P1-009 Estágio 1 — concluído.**
- **P1-009 Estágio 1 continua sendo pré-requisito CUMPRIDO de P1-007.**
- **P1-005 e P1-006 permanecem independentes** desta representação e **podem prosseguir**.
- **P1-007 não deve iniciar** antes de **reutilizar a taxonomia de `EstimateNature`** — é vedado
  inventar uma segunda solução para o mesmo fenômeno (`snrDb()` devolvendo `200 dB` quando `pn ≤ 0`
  é candidato a `LowerBound`; ver §L.7).

---

## Registro de Encerramento — P1-005 (Denominador do MS-10) — **NORMATIVO**

> **Estado: CONCLUÍDO — validado independentemente no Windows.**
> **Referência normativa:** *Registro de Decisão — MS-10* → **D1** (ruído *band-limited*, alinhado ao
> MS-08) · `noise-floor-definition.md` §6 (o Noise Floor é o **ruído de referência** do SNR, **na
> mesma banda**).
> **Pré-requisitos cumpridos:** **AR-01** / **P1-001** (normalização energética) e **P1-002**
> (estados semânticos do MS-08).

### N.1 A solução científica

O MS-10 **deixa de usar**

```
pn = meanSquare(ST-09 inteiro, 0 .. Nyquist)        ← banda LARGA
```

e passa a **consumir o Noise Floor oficial do MS-08**, já limitado à banda normativa
**20 Hz–20 kHz**. A composição passa a ser:

```
pn      = 10^(NoiseFloor_dBFS / 10)
SNR_dB  = 10 * log10(ps / pn)
```

com **`ps` inalterado nesta etapa**.

**O MS-08 é a fonte única de verdade do denominador.** O MS-10 **não recomputa o ST-09** e **nenhuma
segunda estimativa de Welch foi criada** — consome o resultado já publicado pela família que detém a
definição do piso. A conversão é direta, sem fator de correção, porque as duas grandezas estão na
**mesma escala absoluta**: somado sobre a banda inteira, o estimador do MS-08 reproduz `meanSquare`
com erro de **0,0022 dB**, idêntico a −95 e a −60 dBFS. A única diferença sistemática é a **banda**
(fração `(20k−20)/24k = 0,8325` → **−0,7962 dB** teórico contra **−0,7998 dB** medido).

Antes do P1-005, as duas famílias **discordavam sobre o mesmo silêncio** — em até **52,8 dB**.

### N.2 Resultados quantitativos de fechamento

| Condição do ST-09 | SNR antes | SNR depois |
|---|---:|---:|
| ruído branco | ≈ **85,9897 dB** | ≈ **86,7895 dB** |
| + 22 kHz a −40 dBFS | ≈ **34,0000 dB** | ≈ **86,7895 dB** |
| + 5 Hz a −40 dBFS | ≈ **34,0000 dB** | ≈ **52,9384 dB** |
| + 1 kHz *in-band* a −40 dBFS | ≈ **34,0000 dB** | ≈ **34,0000 dB** |

O caso de **22 kHz** é o resultado central: energia ultrassônica não pertence à banda normativa, e o
SNR volta a ser numericamente idêntico ao da cadeia limpa. O caso de **1 kHz** é a contraprova
obrigatória: ruído **dentro** da banda continua degradando o SNR exatamente como antes — a correção
**não** é rejeição indiscriminada de ruído.

**O caso de 5 Hz é comportamento ESPERADO do estimador oficial do MS-08, não divergência
remanescente do P1-005.** O componente está a 0,43 bin da resolução de 11,72 Hz e o lóbulo principal
de Hann atravessa a borda de 20 Hz, de modo que a rejeição é **parcial (18,94 dB)** e não total. Essa
é a característica de resolução do piso oficial, **herdada por construção** ao exigir alinhamento ao
MS-08. Hann, Welch, tamanho de quadro, resolução espectral, banda e regras de inclusão de bins
permanecem **intactos**.

### N.3 Propagação semântica

| MS-08 | MS-10 | `valuePresent` |
|---|:--:|:--:|
| `Normal` | `Normal` | `true` |
| `LowConfidence` | `LowConfidence` | `true` |
| `NotApplicable` / valor ausente | `NotApplicable` | `false` |
| `Error` | `Error` | `false` / sem escalar válido |

**A sentinela `−300 dBFS` não pode mais reaparecer como SNR artificial de ≈ 291 dB.** Ler
`NoiseFloor_dBFS` por chave sem honrar `quality`/`valuePresent` produziria exatamente esse valor —
a **DV-5 reencarnada no MS-10**. O contrato do operando, já consolidado no P1-004 para o MS-09,
impede-o por construção.

### N.4 CC-A — C++ ↔ JS

A primeira validação independente do P1-005 (`09a9edc`) reprovou por **uma** divergência de
montagem: com o ST-09 ausente o Core publicava `MS-10` em `Error`, enquanto o LabEngine **omitia a
família**. O commit corretivo **`4825fcc`** fecha essa divergência, sem reabrir o cálculo —
`computeMS10()` permaneceu **byte-idêntica** (2050 bytes) e `AnalysisEngine.cpp` não foi tocado.

**Contrato final**, tanto no Core quanto semanticamente no LabEngine:

```
ST-03 ausente OU ST-09 ausente
    → MS-10 EXISTE
    → quality = Error
    → valuePresent = false
```

A precedência foi levantada empiricamente e reproduzida integralmente — **qualquer** ausência de
entrada requerida produz `Error`, e não apenas a do ST-09.

**Diferença representacional PERMITIDA e explicitamente preservada:**

| | `Error` |
|---|---|
| **Core** | família presente, **sem escalar** |
| **JS** | `SNR_dB = null`, `valuePresent = false` |

Para qualquer consumidor que verifique `valuePresent` o contrato é equivalente; é a mesma assimetria
já registrada para o MS-09 desde o P1-004.

Convergência verificada sobre entradas equivalentes em sete condições, comparando `SNR_dB`,
`quality` e `valuePresent`, com coincidência até a **nona casa decimal**.

### N.5 Proteção permanente — TC-048

O **TC-048** protege o P1-005 e cobre:

| Caso | Verifica |
|---|---|
| ruído branco | `Normal`, presente, ≈ 86,7895 dB |
| 22 kHz | invariância em relação à cadeia limpa |
| 5 Hz | rejeição **parcial** asserida explicitamente, sem exigir eliminação total |
| 1 kHz *in-band* | controle contra rejeição indiscriminada — antigo e novo coincidem |
| piso `LowConfidence` | propagação com valor preservado |
| piso `NotApplicable` | sem valor; **e nunca ≈ 291 dB** |
| ST-03 ausente | família existe, `Error`, sem escalar, bundle com 15 famílias |
| ST-09 ausente | idem |
| ambos ausentes | idem |
| **controle negativo** | oráculo **independente** que reimplementa o `meanSquare` antigo, sem usar nenhuma função do produto, demonstrando a queda para ≈ 34 dB |
| coerência | `SNR_dB = 10·log10(ps) − NoiseFloor_dBFS` no **mesmo bundle** — prova de fonte única |

### N.6 Fronteiras preservadas

- **`snrDb()` permaneceu inalterada** e continua sendo primitiva pública.
- **TC-033** continua cobrindo a primitiva crua, sem uma linha alterada.
- O **numerador `ps` permanece inalterado** e pertence ao **P1-006**. *(Resolvido depois: o P1-006 o
  substituiu pela potência da fundamental retornada — ver §O. Este registro é preservado como o
  estado correto no encerramento do P1-005.)*
- **`EstimateNature` do MS-10 permanece `Point`** — o P1-005 não cria `LowerBound`.
- **`pn ≤ 0 → 200` não foi reinterpretado**: o ramo foi reproduzido literalmente após a validação do
  operando. *Fato registrado:* com o `pn` derivado do MS-08 ele é **estruturalmente inalcançável**
  para operando válido (`NoiseFloor_dBFS` tem mínimo −300 dBFS ⇒ `pn ≥ 1e-30 > 0`, e `integFull == 0`
  já retorna `NotApplicable` antes). **Nenhum caso artificial foi inventado** para exercitá-lo.
- A **semântica de limite inferior continua reservada ao P1-007**.
- **MS-08, MS-09, MS-13, ST-03 e ST-09 não foram alterados.**
- **Exportação e Evidence Package permanecem estruturalmente inalterados.**

### N.7 Validação independente no Windows — **APROVADO**

| Item | Evidência |
|---|---|
| Branch | `claude/aamp-migration-stage-0-fzuot8` |
| HEAD local = remoto | `4825fcc960475154acc89356801d0fc6e414ca64` |
| Plataforma / configuração | Windows · **`Release`** · `GitDirty = false` |
| Configuração e build | **0 warnings, 0 erros** |
| `aamp_headless_suite` | **817 checks passed, 0 failed** |
| `aamp_scientific_validation` | **8 checks passed, 0 failed** |
| CTest | **2/2 aprovados** |
| Casos confirmados | **TC-030 · TC-033 · TC-034 · TC-038 · TC-039 · TC-041 · TC-043 · TC-046 · TC-047 · TC-048** |

### N.8 Rastreabilidade histórica dos Evidence Packages

> Pacotes `.aamp` anteriores **continuam válidos como evidência bruta** da captura original — o
> formato não foi alterado e nenhum áudio preservado foi tocado.
>
> Entretanto, **o reprocessamento por versões posteriores pode produzir `SNR` diferente do
> originalmente exportado**, devido à **correção científica do denominador** introduzida pelo
> P1-005. Soma-se às causas já registradas para `MS-08` (P1-001) e `MS-09` (F9, P1-003, P1-004).
> **Isto não é inconsistência da evidência: é evolução do estimador sobre a mesma observação.**
> Todo resultado histórico deve ser interpretado **junto da versão/commit que o produziu**.

### N.9 Achados que permanecem abertos — registrados, **não corrigidos**

| ID | Achado | Estado |
|---|---|:--:|
| **DV-6** | ST-09 ausente → o Core publica **MS-08** em `Error`, o LabEngine **omite** a família. Mesma classe da divergência corrigida no MS-10, em família fora do escopo autorizado | 🔴 **Aberto** |
| ~~**P1-006**~~ ✅ | Numerador do SNR — `S/N` estrito (D2/V8) | ✅ **CONCLUÍDO depois** (`a702c0f`) — ver §O |
| **P1-007** | `pn ≈ 0` / limite inferior (D4) | ⬜ Não iniciado — deve **reutilizar `EstimateNature`** |
| **P1-008** | Trust Ladder — porta inferior | ⬜ Não iniciado |
| **F10** | NaN atravessa MS-08/MS-09 como `Normal` | 🔴 **Aberto** — não corrigido; o P1-005 não o amplia |

### N.10 Encerramento

**P1-005 CONCLUÍDO e validado independentemente no Windows** (`4825fcc`). O denominador do SNR passou
a ser o Noise Floor *band-limited* oficial do MS-08, com fonte única de verdade e propagação
semântica completa. Permanecem inalterados **`snrDb()`**, o **numerador**, **`EstimateNature`**,
**MS-08**, **MS-09**, **MS-13**, os **estímulos**, a **exportação** e o **Evidence Package**.

---

## Registro de Encerramento — P1-006 (Numerador do MS-10) — **NORMATIVO**

> **Estado: CONCLUÍDO — validado independentemente no Windows.**
> **Referência normativa:** *Registro de Decisão — MS-10* → **D2/V8** (`S/N` estrito) e **D3**
> (referência *output-referred*) · `noise-floor-definition.md` §6.
> **Pré-requisito cumprido:** **P1-005** (denominador *band-limited*, fonte única de verdade).

### O.1 A solução científica

O MS-10 **deixa de usar**

```
ps = meanSquare(bloco de 1 kHz do ST-03, INTEIRO)        ← S + D + N
```

e passa a medir a **potência espectral absoluta da fundamental de 1 kHz retornada pelo DUT**:

```
ps      = potência da fundamental retornada (captura ST-03)
pn      = 10^(NoiseFloor_dBFS / 10)                       ← P1-005, MS-08 oficial
SNR_dB  = 10 * log10(ps / pn)
```

**Por que o numerador antigo estava errado.** Vale exatamente

```
ps_total / ps_fund = 1 + (THD+N / 100)²
```

ou seja, o valor publicado como "SNR" carregava **informação de THD+N**: num DUT **mais distorcido** o
número **melhorava**. Isso contraria **D2/V8** e invade a responsabilidade de **MS-05/MS-06**, que
**não** foram alteradas.

**Estimador — o mesmo do piso.** Nenhuma máquina espectral nova foi criada:

| Parâmetro | Valor |
|---|---|
| Fonte do valor | **captura do ST-03** (a fonte emitida **não** entra no valor — **D3**, *output-referred*) |
| Bloco | nominal de 1 kHz, recorte `[2 s, 4 s)` |
| Quadro | **único, CENTRADO** no bloco, `N = 4096` |
| Janela | **Hann** |
| Normalização | **ENBW / Parseval** (potência absoluta) |
| Frequência nominal | **fixa em 1000 Hz** — sem *peak tracking* |
| Bin central | `k0 = round(1000·N/fs)` |
| Para 48 kHz / N=4096 | `k0 = 85` → bins **83, 84, 85, 86, 87** |
| Integração | **`k0 ± 2`** |

`inBandPower` passou a aceitar `loHz`/`hiHz` com **os limites normativos como padrão**: o único
chamador existente (**MS-09**) não passa os parâmetros e permanece **numericamente idêntico**. Os
limites são expressos como `(k0∓2)·fs/N` — exatamente a expressão com que o próprio estimador calcula
a frequência de cada bin —, de modo que a comparação de borda é **igualdade bit-a-bit** e o conjunto
de bins é **idêntico em C++ e JS**.

### O.2 Alinhamento temporal — decisão final

> [!warning] **Nenhum alinhamento temporal foi introduzido no P1-006.**
> Não há `bestLag`, correlação cruzada ou dependência da **fonte ST-03 emitida** no caminho do MS-10.
> A invariância observada **não vem de alinhamento — vem do ENQUADRAMENTO**: um quadro de 4096
> amostras **centrado** num bloco de 2 s tolera deslocamentos muito superiores ao domínio operacional
> do projeto.

| Deslocamento do ST-03 | SNR estrito |
|---|---:|
| offset 0 | **86,788268656 dB** |
| offset 913 | **86,788268656 dB** |
| offset 2400 *(= 50 ms, o limite exato da política)* | **86,788268656 dB** |
| offset 4800 *(= 100 ms, o dobro da política)* | **86,788268656 dB** |

**A justificativa é CONDICIONADA às premissas atuais:**

1. bloco de 1 kHz com **2 s**;
2. quadro de **4096** amostras, **centralizado**;
3. política de latência do projeto **≤ 50 ms**.

> **Se qualquer uma dessas premissas for alterada, a necessidade de alinhamento deve ser
> reavaliada.** Este registro **não** declara que o MS-10 "não precisa de alinhamento" em qualquer
> arquitetura futura — declara que, **nesta geometria**, o alinhamento é desnecessário e por isso não
> foi introduzido. Em consequência, também **não** foi criada política nova para fonte ST-03 ausente,
> e o **F9 não foi reutilizado** onde não há necessidade demonstrada.

### O.3 Resultados quantitativos de fechamento

| Caso | SNR estrito | `meanSquare` antigo | Inflação indevida |
|---|---:|---:|---:|
| limpo | **86,788268656 dB** | 86,789489300 dB | — |
| harmônicas (2 kHz + 3 kHz) | **86,788268650 dB** | 86,847879565 dB | **+0,0596 dB** |
| DC | **86,788268656 dB** | 86,875088036 dB | **+0,0866 dB** |
| 22 kHz | **86,788268656 dB** | 86,832499557 dB | **+0,0443 dB** |
| ruído −60 dBFS | **86,787547890 dB** | 86,789442134 dB | — |
| ganho ×1,5 *(sem clipping)* | **90,310093837 dB** | 90,311314481 dB | — |
| clipping ×3 | **94,161086680 dB** | 94,263156660 dB | **+0,1021 dB** |
| offset 913 | **86,788268656 dB** | — | — |
| offset 2400 | **86,788268656 dB** | — | — |

**O método antigo `meanSquare` incorporava indevidamente:**

- **harmônicas** — produtos de distorção do DUT;
- **DC** — componente de offset, fora da banda normativa;
- **energia ultrassônica** — conteúdo acima de 20 kHz;
- **energia adicional produzida por clipping** — as harmônicas geradas pela própria saturação.

Isso fazia o "SNR" **carregar informação de THD+N**, contrariando **D2/V8**.

Dois casos merecem leitura explícita:

- **ganho ×1,5 sem clipping** — o numerador estrito escala **exatamente** `20·log10(1,5)` em relação
  ao caso limpo: o estimador é linear no nível quando não há saturação.
- **clipping ×3** — o numerador estrito fica **abaixo** do antigo porque a fundamental é
  **comprimida** pela saturação e as harmônicas geradas **não** são contadas. Essa é a informação
  física correta; contá-las era precisamente o defeito.

### O.4 Viés de integração — propriedade conhecida do estimador

> [!info] **Viés determinístico de ≈ −0,001220644 dB**
> Medido contra a potência analítica `A²/2` de um seno exato de −6 dBFS. Decorre de o tom de 1 kHz
> **não cair exatamente no centro de um bin** (em 48 kHz / N=4096 ele cai no bin **85,333**) somado à
> integração **ratificada** em **±2 bins**.

Classificação: **propriedade conhecida do estimador adotado**, **não** defeito pendente do P1-006. O
truncamento só pode **subestimar**, nunca superestimar. **A decisão de ±2 bins não é reaberta neste
encerramento** e a largura **não** foi ampliada para ±3 ou ±4 para reduzir o viés — os testes fixam o
**valor esperado do novo contrato**, não o valor ideal de um seno infinitamente resolvido.

### O.5 Fonte única e coerência com o P1-005

A composição final do MS-10:

```
ps  = potência da FUNDAMENTAL retornada          ← P1-006 (este registro)
pn  = potência derivada do NoiseFloor_dBFS oficial do MS-08   ← P1-005
SNR = 10 * log10(ps / pn)
```

| Camada | Item | Registro |
|---|---|---|
| **Denominador** | Noise Floor *band-limited* oficial do MS-08 | **P1-005** (§N) |
| **Numerador** | potência da fundamental retornada | **P1-006** (§O) |
| **Distorção** | THD / THD+N / IMD | **MS-05 / MS-06** — inalteradas |

**Ambos os lados da razão usam a MESMA máquina espectral absoluta** — Hann + ENBW/Parseval —, o que
torna a razão dimensionalmente coerente e auditável por construção.

### O.6 Proteção permanente — TC-049

O **TC-049** é a regressão permanente do P1-006 e cobre:

| Caso | Verifica |
|---|---|
| seno puro | numerador = potência da fundamental; **viés ±2 bins fixado como contrato** |
| harmônicas | estrito **invariante**; o antigo infla |
| ruído | in-band afeta o estrito; os métodos divergem |
| DC | estrito **invariante**; o antigo infla |
| 22 kHz | estrito **invariante**; o antigo infla |
| ganho ×1,5 | escala **exata** de `20·log10(1,5)`, sem clipping (pré-condição asserida) |
| clipping ×3 | fundamental comprimida; antigo **acima** do estrito (pré-condição asserida) |
| offsets 0 / 913 / 2400 *(+ margem a 4800)* | **invariância temporal do quadro centrado** — a cláusula é do **enquadramento**, não de alinhamento |
| ST-03 ausente | família **existe**, `Error`, sem escalar, bundle com 15 famílias |
| **CC-A** | pontos de referência normativos: `k0 = 85`, bordas `972,65625 Hz` / `1019,53125 Hz`, expressão inteira do quadro centrado |
| **controle negativo** | oráculo **independente** que reimplementa o `meanSquare` antigo, sem usar nenhuma função do produto, provando que ele incorpora harmônicas, DC e 22 kHz e **se comporta de forma diferente** do numerador estrito |
| não-regressão do denominador | identidade `SNR_dB = 10·log10(ps_fund) − NoiseFloor_dBFS` no mesmo bundle; `LowConfidence`, `NotApplicable`, sentinela `−300` |

> **Registro metodológico.** Deliberadamente **não** se exige do controle negativo uma "falha por
> latência": o `meanSquare` **não discrimina frequência** e, justamente por isso, **mascarava** o
> problema em vez de expô-lo.

### O.7 TC-048 — atualização delimitada

O **TC-048 foi atualizado somente onde as asserções antigas fixavam o numerador pré-P1-006**:

| Ponto | Antes | Depois | Motivo |
|---|---:|---:|---|
| (a) ruído branco | 86,7895 dB | **86,7883 dB** | viés de −0,001221 dB |
| (b) 22 kHz | 86,7895 dB | **86,7883 dB** | idem |
| (f) piso `LowConfidence` | 86,8250 dB | **86,8238 dB** | idem |
| (j) identidade de coerência | `ps = meanSquare` | **`ps` estrito**, por oráculo independente | o numerador mudou |

**As proteções do P1-005 permaneceram integralmente:**

- denominador oficial do MS-08 como fonte única;
- 22 kHz no ST-09 (invariância fora de banda);
- 5 Hz — rejeição parcial asserida explicitamente;
- 1 kHz *in-band* — controle contra rejeição indiscriminada;
- piso `LowConfidence` — propagação com valor preservado;
- piso `NotApplicable` — sem valor;
- ausência estrutural (ST-03, ST-09, ambos) → `Error`;
- **vedação da sentinela `−300 dBFS`** — nunca ≈ 291 dB;
- identidade de coerência, **agora expressa com o numerador estrito**.

Nenhum caso foi removido e nenhuma tolerância foi afrouxada.

### O.8 CC-A — C++ ↔ JS

Convergência verificada sobre **entradas bit-idênticas** (blocos de tom exportados do Core e
consumidos pelo LabEngine), executando o **bloco real de montagem** do MS-10, em **12 condições**:

| Eixo comparado | Resultado |
|---|---|
| `SNR_dB` | **0 divergências** — maior `|Δ|` **exatamente 0,0 dB** (identidade bit-a-bit em todos os casos com valor) |
| `quality` | coincidente em todos os casos |
| `valuePresent` | coincidente em todos os casos |
| `EstimateNature` | **`Point`** nos dois motores — o P1-006 **não** cria `LowerBound` |

Cobertos os casos científicos (limpo, harmônicas, DC, 22 kHz, ruído, ganho, clipping, offsets) **e**
os estados **`LowConfidence`**, **`NotApplicable`** e **`Error`**. Preservada a diferença
representacional já registrada desde o P1-004: em `Error` o Core **não emite escalar**, o JS emite
`SNR_dB = null` com `valuePresent = false`.

### O.9 Não-regressões e fronteiras preservadas

| Item | Estado |
|---|---|
| **MS-08** | ≈ **−95,799789257 dBFS** — numericamente inalterado |
| **MS-09** | ≈ **86,789489300 dB** (`LowerBound`) — numericamente inalterado |

Permaneceram **intactos**: **`snrDb()`** (e portanto **TC-033**, sem uma linha alterada), **MS-05**,
**MS-06**, **MS-08**, **MS-09**, **MS-13**, **ST-03**, **ST-09**, **`EstimateNature`**, **`Quality`**,
a **exportação** e o **Evidence Package**. `MetricResult`, `Scalar`, os **estímulos** e o **CMake**
também não foram tocados.

### O.10 Validação independente no Windows — **APROVADO**

| Item | Evidência |
|---|---|
| Branch | `claude/aamp-migration-stage-0-fzuot8` |
| HEAD local = remoto | `a702c0f1a6ff0a8a4cd0982f919fa7edca7e2eb9` |
| Plataforma / configuração | Windows · **`Release`** · `GitDirty = false` |
| Build | **`--clean-first`** · **0 warnings, 0 erros** |
| Executável | `build\windows-ninja\aamp_app_artefacts\Release\AAMP.exe` · **4.660.736 bytes** |
| SHA-256 | `1488CD919F25F93448050C0B254B374F9069AE68E025D7DD337DA308B3243306` |
| `aamp_headless_suite` | **874 checks passed, 0 failed** |
| `aamp_scientific_validation` | **8 checks passed, 0 failed** |
| CTest | **2/2 Passed** |
| Casos confirmados | **TC-030 · TC-033 · TC-034 · TC-038 · TC-039 · TC-041 · TC-043 · TC-046 · TC-047 · TC-048 · TC-049** |

### O.11 Rastreabilidade histórica dos Evidence Packages

> Pacotes `.aamp` anteriores **continuam válidos como evidência bruta** da captura original — o
> formato não foi alterado e nenhum áudio preservado foi tocado.
>
> Entretanto, **o reprocessamento por versões posteriores pode produzir `SNR` diferente do
> originalmente exportado**, devido às correções **acumuladas** do **P1-005** (denominador
> *band-limited*) e do **P1-006** (numerador estrito). Soma-se às causas já registradas para `MS-08`
> (P1-001) e `MS-09` (F9, P1-003, P1-004). **Isto não é inconsistência da evidência: é evolução do
> estimador sobre a mesma observação.** Todo resultado histórico deve ser interpretado **junto da
> versão/commit que o produziu**.

### O.12 Achados que permanecem abertos — registrados, **não corrigidos**

| ID | Achado | Estado |
|---|---|:--:|
| **P1-007** | Semântica de `pn ≈ 0` / limite inferior do SNR (D4) | ⬜ **Liberado para análise** — deve **reutilizar `EstimateNature`** (P1-009 estágio 1) |
| **P1-008** | Trust Ladder — porta inferior | ⬜ **Não iniciado** |
| **DV-6** | ST-09 ausente → o Core publica **MS-08** em `Error`, o LabEngine **omite** a família | 🔴 **Aberto** — corrigido apenas para o MS-10 |
| **F10** | NaN atravessa MS-08/MS-09 como `Normal` | 🔴 **Aberto** — o P1-006 não o amplia |
| **P1-009 — estágio 2** | Fatos primários (`DynamicRange_stepIndex`, `DynamicRange_firstClippedIndex`) e integração com exportação/UI | 🔴 **Aberto — não iniciado** |

### O.13 Encerramento

**P1-006 CONCLUÍDO e validado independentemente no Windows** (`a702c0f`). O numerador do SNR passou a
ser a **potência da fundamental retornada**, fechando **D2/V8**: o MS-10 é hoje **S/N estrito**, com
denominador e numerador medidos pela **mesma máquina espectral absoluta** e com a distorção
integralmente sob responsabilidade de **MS-05/MS-06**. Nenhum alinhamento temporal foi introduzido, e
a dispensa está **condicionada às premissas geométricas registradas em §O.2**.

---

## Encadeamento com os pilares

1. **Pilar 1 (validar implementação):** endereça **V1** (porta inferior da confiança), **V2**
   (propagação de `Quality`), **V3** (semântica da Consistência), **V4** (Reproducibility), **V7** (DBG).
   Pré-decisão de especificação a resolver antes de tocar código/teste: **V8** (numerador do MS-10:
   `S/N` vs `(S+D+N)/N`) — ✅ **resolvida e implementada** pelo **P1-006** (§O). Restrição de ordem:
   **AR-01** — corrigir e validar a normalização do MS-08 antes de implementar D1 no MS-10.
2. **Pilar 2 (apresentação normativa):** define e expõe as faixas da **Seção C** (V5) de forma uniforme.
3. **Pilar 3 (exportação):** congela o formato só depois que Seções A–C estiverem estáveis.

---

## Roadmap de revisão conceitual

Fluxo metodológico por métrica (o mesmo empregado no MS-10 e no MS-08): **(1)** especificação normativa
(o que medir) → **(2)** auditoria da implementação (como mede) → **(3)** análise de conformidade →
**(4)** registro de decisão → **(5)** só então implementação.

| Métrica | Estado |
|---|---|
| **MS-10 — SNR** | ✅ **Encerrado** (Registro de Decisão normativo). **D1 implementada no P1-005** (§N) e **D2/V8 no P1-006** (§O) — denominador e numerador convergidos. Item aberto `pn≈0` (D4) rastreado no **P1-007** |
| **MS-08 — Noise Floor** | ✅ **Encerrado** (definição normativa ratificada — N1; `noise-floor-definition.md`) |
| **MS-09 — Dynamic Range** | ✅ **Encerrado** (Registro de Decisão D1–D3; `dynamic-range-definition.md` APROVADA; alinhamento de implementação → Pilar 1, sob AR-01) |

Nenhuma implementação (Pilar 1) inicia enquanto a arquitetura normativa não estiver consolidada; **AR-01**
condiciona qualquer reutilização normativa do pipeline espectral.

---

# Apêndice A — Estudo N1: Definição Normativa de Noise Floor

> Embasamento técnico da decisão **N1 (RATIFICADA)** — conforme
> [`docs/laboratory/noise-floor-definition.md`](../laboratory/noise-floor-definition.md). Estudo
> comparativo das definições candidatas. **Não** contém decisão de implementação.

> **⬛ Resultado fundamental.** Após aplicar Parseval, **potência integrada in-band** e **RMS in-band**
> são a **mesma grandeza física** (mesmo valor e unidade, dBFS):
> `10·log10(Σ_banda P̄) = 20·log10(RMS_banda)`. Logo existem apenas **duas famílias** de definição de
> Noise Floor:
> 1. **Nível integrado em banda** (RMS / Potência) — grandeza *extensiva*, escala com a banda;
> 2. **Densidade espectral** (PSD, dBFS/Hz) — grandeza *intensiva*, independe da banda.
> Isso elimina uma falsa terceira opção e simplifica toda a discussão futura.

### A.1 Comparação das grandezas

| Critério | **(1) Nível de banda (integrado / RMS in-band)** | **(2) PSD média (densidade)** |
|---|---|---|
| Significado físico | Energia **total** de ruído em 20 Hz–20 kHz que o sinal precisa superar | "Altura do tapete" de ruído **por Hz**; caracteriza a fonte, independente da banda medida |
| Unidade | **dBFS** (nível único) | **dBFS/Hz** (ou dBFS/bin se por bin) |
| Dependência de FFT | Independente de N se ENBW-normalizado (Parseval conserva a potência total) | Independente de N **por Hz**; **dependente de N por bin** (piso cai ~3 dB/oitava de N) |
| Comparabilidade entre instrumentos | Comparável **se a banda e a ponderação forem declaradas e iguais**; é o número único de "noise" das fichas técnicas | Comparável **mesmo com bandas diferentes** (densidade); ideal p/ qualidade intrínseca de conversores |
| Aderência à literatura / analisadores pro | **Padrão** de spec de ruído (AES17, IEC 61606; Audio Precision APx, R&S): RMS em banda declarada | Padrão de **densidade** ("−140 dBFS/Hz", "nV/√Hz"); é o que o display FFT mostra por bin |

### A.2 Coerência metrológica **dentro do AAMP** (4º eixo)

Como cada grandeza se integra às demais métricas já existentes:

| Componente do AAMP | **(1) Nível de banda** | **(2) PSD (densidade)** |
|---|---|---|
| **MS-09 Dynamic Range** (`DR = lmaxDb − NoiseFloor`) | **Consistente** — subtração de dois níveis dBFS na mesma base | **Inconsistente de unidade** — `dBFS − dBFS/Hz` não é dB; exigiria integrar a PSD na banda (= virar (1)) |
| **MS-10 SNR** (D1: sinal − ruído, mesma banda) | **Consistente** — o `pn` do SNR é exatamente este nível | **Inconsistente** — `nível − densidade`; mesma exigência de integração |
| **Exportação** | **Coerente** — um escalar dBFS, alinhado às demais métricas de nível | Exige declarar banda/normalização; mistura unidades no pacote |
| **Trust Ladder** (porta inferior / V1) | **Coerente** — compara nível de sinal × nível de ruído diretamente | Densidade não entra na comparação sinal×ruído sem integrar |

> **Leitura do 4º eixo:** MS-09, MS-10 e a Trust Ladder **já assumem subtração de níveis em dBFS**.
> O **nível integrado (1)** é metrologicamente coerente com todo o AAMP; a **PSD (2)** é superior como
> *descritor de densidade*, mas quebra a coerência de subtração a menos que seja integrada de volta
> (tornando-se (1)). Este eixo é tão decisivo quanto a aderência à literatura.

### A.3 N7 — Portabilidade normativa (expectativa do estimador)

| Grandeza | FFT (N) | Taxa de amostragem (fs) | Largura de banda | Duração da observação |
|---|---|---|---|---|
| **(1) Nível de banda** | Invariante com ENBW-correção (Parseval); com escala atual → viés **constante** de janela (ver N1-R1 / Apêndice B) | Invariante (banda fixa 20–20 k; basta `fs>40 k`) | ⚠️ **Escala com a banda** — comparável só com banda declarada | Média invariante; variância ↓ com mais frames |
| **(2) PSD por Hz** | Invariante | Invariante | **Invariante** (densidade) | Média invariante; variância ↓ com duração |
| **(2′) PSD por bin** | ❌ Depende de N (~3 dB/oitava) | ❌ Depende via Δf | n/a | Invariante |

**Conclusões:** (i) nunca definir Noise Floor como potência **por bin** (2′) — menos portável; (ii) o
**nível integrado (1)** é portável em FFT/fs/duração, dependente só da banda (fixa no AAMP); (iii) a
**PSD/Hz (2)** é a mais portável em todos os eixos, mas é densidade e não serve diretamente ao SNR.

### A.4 Recomendação — **RATIFICADA (N1 APROVADO)**

Grandeza-título "Noise Floor" = **(1) nível integrado em banda (RMS / potência in-band, dBFS, 20–20 k,
flat)** — padrão de ficha técnica, serve diretamente ao SNR (D1) e é **metrologicamente coerente com
MS-09/MS-10/Trust/Exportação** (A.2). **PSD/Hz** como **descritor complementar** (a curva por bin já
existe). Precondição obrigatória: **N1-R1** (normalização ENBW). **N1 APROVADO** — ratificado em
[`noise-floor-definition.md`](../laboratory/noise-floor-definition.md) (ver Registro de Decisão do MS-08).

---

# Apêndice B — Investigação da Normalização Espectral do MS-08

> Objetivo (a pedido): determinar **exclusivamente** se a implementação atual do MS-08 produz uma
> estimativa **energeticamente correta para ruído de banda larga** (ENBW / Parseval) ou apenas para
> **tons discretos** (ganho coerente) — independentemente de qual definição N1 for adotada.
> **Sem propor alterações.**

### B.1 O que o código faz (fatos)
- `windowCoherentGain(win) = Σw` — soma dos coeficientes da janela (`Windows.cpp:46-50`); o header a
  declara explicitamente "para normalização de **magnitude**" (`Windows.h:14`). É o **ganho coerente**.
- `magSpectrum` normaliza `mag[i] = |X[i]| / (Σw) · 2` (`AnalysisEngine.cpp:24-26`). Essa escala faz um
  **tom on-bin de amplitude A** ler `mag = A` (calibração de **amplitude de tom**).
- MS-08 acumula `acc[b] += mag[b]²`, faz média sobre frames (Welch, Hann 4096, 50% overlap) e **soma**
  sobre os bins da banda → `NoiseFloor_dBFS = 10·log10(Σ_banda P̄)` (`cpp:684-746`).
- **Não existe** nenhuma função de **ENBW / ganho de ruído / Σw²** no código (busca: só `CoherentGain`).

### B.2 Verificação por Parseval (por que isso importa para ruído)
Para ruído com variância σ² (mean-square real = σ²), pela relação de Parseval e `E[Σ(x·w)²] = σ²·Σw²`:
`E[Σ_onesided mag²] = 2·σ²·ENBW_bins`, onde `ENBW_bins = N·Σw²/(Σw)²`.
- Para um **tom**, a mesma convenção devolve a amplitude correta (`mag = A`) — **calibração exata**.
- Para **ruído**, a estimativa carrega o fator **ENBW** da janela. Para **Hann, ENBW = 1.5 bins**
  (`10·log10(1.5) ≈ 1.76 dB`): o nível absoluto de ruído fica **enviesado por um offset constante,
  determinado pela janela**, porque a normalização por ganho coerente pressupõe energia concentrada em
  **um** bin (linha espectral), enquanto ruído de banda larga espalha energia por ~ENBW bins.

### B.3 Veredito
- **Tons discretos:** a estimativa é **energeticamente correta** (calibração de amplitude por ganho coerente).
- **Ruído de banda larga:** a estimativa **NÃO é energeticamente correta** — falta a normalização ENBW/Σw².
  O nível absoluto de ruído tem **viés constante de janela** (Hann ⇒ ~1.76 dB), pois mede uma grandeza
  **calibrada para tom**, não a potência real de ruído.
- **Natureza do erro:** é um **offset constante** (a janela é sempre Hann) e **independente de N**
  (ENBW é propriedade da janela; Parseval conserva a potência integrada). Portanto:
  - **forma espectral / relativa:** correta;
  - **comparabilidade entre sessões (mesma janela):** preservada (o offset cancela);
  - **correção energética absoluta do ruído:** **não** — o dBFS reportado não é a potência real de ruído em banda.

### B.4 Escopo
O viés ENBW afeta **níveis absolutos de banda larga** (o piso de ruído do MS-08 e, por D1, o `pn` do
SNR). Ele **cancela em razões estritamente de banda estreita computadas com a mesma primitiva** (ex.:
THD = harmônicos/fundamental, ambos tom-like). Métricas que incluem energia de banda larga no numerador
(THD+N, ruído) merecem a mesma verificação **quando forem revisadas** — fora do escopo desta investigação,
que se limitou ao MS-08. Consolidado em **N1-R1**.

### B.5 — Matriz de impacto arquitetural da correção ENBW

Impacto esperado de uma futura correção ENBW, por métrica. A coluna **Confiança** indica quão demonstrada
está a *classificação* (não a magnitude do impacto): **Alta** = praticamente demonstrada pelo código /
Parseval; **Média** = expectativa arquitetural a confirmar na revisão específica da métrica.

| Métrica | Usa `magSpectrum`? | Natureza | Classe | **Confiança** | Justificativa (código) |
|---|---|---|:--:|:--:|---|
| **MS-08** Noise Floor | Sim | Ruído banda larga, nível absoluto | **(2)** offset constante | **Alta** | Derivação Parseval direta; fator ENBW único (Hann=1.5 ⇒ ~1.76 dB), N-independente (`cpp:735`) |
| **MS-09** Dynamic Range | Não (herda MS-08) | Nível − nível | **(2)** offset constante | **Alta** | `lmaxDb` é domínio do tempo (`cpp:764`, intacto); herda o offset do MS-08 |
| **MS-10** SNR *(atual)* | **Não** | Potência tempo / tempo | **(1)** nenhum | **Alta** | `ps`,`pn` = `meanSquare` no tempo (`cpp:72-73, 785-786`); não toca `magSpectrum` |
| **THD** (MS-05) | Sim | Razão linha/linha | **(1)** nenhum | **Média** | Ver observação conservadora abaixo |
| **THD+N** (MS-06) | Sim | Razão banda-larga/linha | **(4)** investigar | **Alta** | Numerador `E_band − A1` mistura ruído (escalável) e harmônicos (não); efeito conteúdo-dependente (`cpp:354-366`) |
| **IMD** (MS-07) | Sim | Razão linha/linha | **(1)** nenhum | **Média** | Ver observação conservadora abaixo |

**Observação conservadora (THD / IMD).** A **expectativa arquitetural é de cancelamento do fator de
normalização em razões entre componentes discretas** (linha/linha). Essa **hipótese** será **confirmada
durante a revisão específica do MS-05** (THD) e do **MS-07** (IMD) — por isso a confiança é **Média**, não
Alta. Não se afirma "imunidade" antes dessa verificação.

**Observação normativa — ordem de implementação (MS-08 → D1/MS-10).** O MS-10 mede hoje a potência de
ruído no **domínio do tempo** e, portanto, **não herda** o viés espectral do MS-08 (é, hoje,
energeticamente correto para ruído). Após **D1**, o SNR passará a **reutilizar** a definição do MS-08.
Logo a ordem de implementação **deixa de ser arbitrária** e passa a ser obrigatória (formalizada em
**AR-01**):
1. **corrigir** a normalização do MS-08;
2. **validar** o MS-08;
3. **só então** implementar D1 no MS-10.

**Diagrama de dependência (duas famílias de impacto):**
```
Normalização FFT (ENBW)
        │
        ▼
      MS-08  ──►  MS-09  ──►  D1 (MS-10)        [família COM impacto: offset constante ~1.76 dB]

Normalização FFT (ENBW)
        ├── THD    (hipótese: sem efeito → confirmar no MS-05)
        ├── IMD    (hipótese: sem efeito → confirmar no MS-07)
        └── THD+N  (investigar — efeito conteúdo-dependente)
```

**Síntese.** Impacto **contido**: offset constante (~1.76 dB) em MS-08→MS-09 (e no MS-10 apenas **após**
D1), cancelamento **esperado** (a confirmar) nas razões de linha, e **um único ponto** (THD+N) a
investigar. Nenhuma métrica sofre alteração proporcional imprevisível. Regra geral consolidada em
**AR-01**.

---

# Apêndice C — Auditoria de Implementação do MS-09 (Dynamic Range)

> **Passo 2** do fluxo do MS-09 — **exclusivamente descritivo**. Verifica qual definição operacional a
> implementação atual usa e a compara com a proposta normativa
> [`dynamic-range-definition.md`](./dynamic-range-definition.md). **Sem** decisões, **sem** recomendações
> de correção, **sem** alteração de código/documentação normativa. Evidência por `arquivo:linha`.

### C.1 Pergunta central — resposta

**Qual é o critério operacional de "teto"?** → O teto é o **maior nível RMS entre os oito degraus de
100 ms do ST-08 cujo pico permaneceu estritamente abaixo do fundo de escala digital** (`|amostra| < 1.0`
= 0 dBFS). O evento que delimita o teto é o **clipping por pico em plena escala (saturação numérica)** —
**não** um limiar de distorção nem de THD. Determinável com **alta confiança** (lógica explícita em ambas
as implementações).

### C.2 Como o teto é determinado
- **Rotina:** `AnalysisEngine::dynamicRange` (`cpp:749-776`); reimplementação paralela `computeMS09`
  (`html:947-966`).
- **Variável do teto:** `lmaxRms` → `lmaxDb = 20·log10(lmaxRms)` (`cpp:760,772`; `html:953,964`).
- **Origem:** ST-08 (`ST08_DynamicStep`), fatiado em **8 segmentos de 100 ms** (`seg = 0.100·fs`;
  `s = 0..7`) (`cpp:759-762`; `html:952-956`).
- **Condição que encerra a busca:** **não há término antecipado.** Varre os 8 segmentos e toma o
  **máximo RMS** entre os aceitos. O teto é "o maior degrau que não clipou", **não** "o nível exato de
  início da saturação" (`cpp:761-766`; `html:954-962`).

### C.3 Qual evento define o teto
- Aceitação de um degrau: `peak(part) < 1.0` (`cpp:765`; `html:961`), com `peak(x) = max|v|`
  (`StimulusGenerator.cpp:14-18`) e `1.0 = FULL_SCALE` (`Constants.h:13`).
- Portanto o evento é o **pico atingir o fundo de escala digital (clipping / saturação numérica)**: um
  degrau é **descartado** se `max|amostra| ≥ 1.0`.
- **Não** é: limiar de distorção; **não** é limiar de THD; **não** é limiar de linearidade.

### C.4 Dependências
**Reais:**
- **ST-08** (fonte do teto) — `cpp:751`.
- **MS-08 Noise Floor** (piso), lido pela chave `NoiseFloor_dBFS` — `cpp:752,756-757`; `html:949`.
  MS-08 depende de **ST-09**.
- Primitivas: `peak()` (`StimulusGenerator.cpp:14`), `meanSquare()` (`cpp:30-35`), `slice()`.
- Constantes: `fs`; `FULL_SCALE = 1.0` (`Constants.h:13`).

**Não-dependências (registradas por relevância):**
- **Headroom** — **não** participa do cálculo do DR (é métrica de Integridade, separada).
- **FFT / espectro** — o **teto** não usa FFT (é RMS no domínio do tempo). O **piso** usa, mas via MS-08.
- **Flags de qualidade de outras métricas** — não consumidas; o DR produz `Quality` própria.
- **Herança AR-01:** o DR consome `NoiseFloor_dBFS`, que carrega o viés ENBW (Apêndice B); impacto já
  mapeado em **B.5 (classe (2), offset constante)**. Registrado, não reaberto.

### C.5 Grandezas utilizadas

| Variável | Significado físico | Unidade | Origem | Natureza |
|---|---|---|---|---|
| `lmaxRms` | RMS do maior degrau não-clipado (**domínio do tempo, banda larga**) | fração FS (linear) | ST-08 | nível (linear) |
| `lmaxDb` | Nível do teto = `20·log10(lmaxRms)` | dBFS | derivado | **nível absoluto** |
| `lnf` (`NoiseFloor_dBFS`) | Nível do piso (**banda 20–20 k**) | dBFS | MS-08 / ST-09 | **nível absoluto** |
| `peak(part)` | Pico do degrau (gate de clipping) | fração FS | ST-08 | nível (gate) |
| `seg` | Comprimento do degrau (100 ms) | amostras | fs | — |
| `DynamicRange_dB` | `lmaxDb − lnf` | dB | derivado | **diferença de níveis** |

Nota dimensional: `lmaxDb` (20·log10 RMS) e `lnf` (10·log10 potência = 20·log10 RMS) são **ambos
referenciados a RMS** → subtração homogênea (dB). **Porém** `lmaxRms` é **banda larga / tempo** e `lnf` é
**band-limited 20–20 k** → bandas distintas (ver C.7/C.8).

### C.6 Fluxo operacional
```
ST-08 ──► 8 degraus de 100 ms
             │  por degrau:  rms = √meanSquare(part) ;  pk = max|v|
             ▼
      gate de clipping:  aceita se  pk < 1.0  (fundo de escala)
             ▼
      lmaxRms = maior RMS entre degraus NÃO-clipados
             ▼
      lmaxDb = 20·log10(lmaxRms)          (teto — dBFS, banda larga/tempo)

ST-09 ──► MS-08 ──► NoiseFloor_dBFS = lnf  (piso — dBFS, banda 20–20 k)
             ▼
      DynamicRange_dB = lmaxDb − lnf       (dB)

  degeneração: nenhum degrau válido (todos clipados/vazios) → lmaxRms ≤ 0 → NotApplicable
```

### C.7 Conformidade com a proposta normativa (`dynamic-range-definition.md`)

| Item normativo | Estado | Evidência |
|---|---|---|
| Grandeza = diferença de níveis (teto − piso) | **Conforme** | `lmaxDb − lnf` (`cpp:772-773`) |
| Unidade = **dB** (não dBFS) | **Conforme** | escalar `"dB"` (`cpp:773`) |
| Teto por **saturação/clipping** (não distorção) | **Conforme** | gate `peak<1.0` (`cpp:765`); nenhum THD |
| Teto e piso na **mesma banda** (§4) | **Divergente** | teto = RMS tempo banda larga (`cpp:764`); piso = 20–20 k (MS-08) |
| Exclusão de energia fora de banda **no teto** (§5) | **Divergente** | `meanSquare` de amostras cruas inclui DC/fora de banda (`cpp:764`) |
| Piso medido em **silêncio** (§5) | **Conforme** | via ST-09 / MS-08 (`cpp:752`) |
| Sem ponderação | **Conforme** | nenhuma ponderação aplicada |
| MS-08 como **limite inferior** (§6) | **Conforme** | consome `NoiseFloor_dBFS` (`cpp:756-757`) |
| Coerência dimensional (§7) | **Conforme** | ambos RMS-referenced dBFS |
| Tratamento da **degeneração teto ≤ piso** (§7) | **Parcialmente conforme** | trata "sem degrau válido" → NotApplicable (`cpp:767-771`); DR **negativo** (teto<piso) é reportado como `Normal` (`cpp:772-773`) |
| Relações Exportação / Trust Ladder (§6) | **Não avaliado** | camadas de exportação/UI — fora do escopo desta auditoria |

### C.8 Achados (padrão MS-08)

| ID | Descrição | Evidência | Impacto normativo | Confiança |
|---|---|---|---|---|
| **MS09-A1** | Teto = maior RMS entre degraus com `peak < 1.0`; evento = clipping por pico em plena escala | `cpp:761-765`; `html:954-961`; `StimulusGenerator.cpp:14`; `Constants.h:13` | **Conforme** ao critério de teto proposto (saturação) | Alta |
| **MS09-A2** | Teto medido como RMS **banda larga no domínio do tempo** (sem band-limiting), enquanto o piso é 20–20 k | `cpp:764` vs `cpp:752` (MS-08) | **Divergente** de §4/§5 (teto e piso em bandas distintas) | Alta |
| **MS09-A3** | Teto por **degraus de 100 ms** (máximo RMS válido); sem busca do "onset" de saturação | `cpp:759-766`; `html:952-962` | Descritivo — teto = "maior degrau não-clipado", granularidade de degrau; conceitualmente Conforme | Alta |
| **MS09-A4** | Degeneração **teto ≤ piso** não sinalizada: DR negativo reportado como `Normal` (apenas "sem degrau válido" vira NotApplicable) | `cpp:767-773`; `html:963-965` | **Parcialmente conforme** a §7 (tratamento da degeneração) | Alta |
| **MS09-A5** | DR herda `NoiseFloor_dBFS` do MS-08 e, com ele, o viés ENBW (offset constante) | `cpp:752-757`; Apêndice B.5 | Normativo — valor absoluto do DR carrega o offset do piso até AR-01 | Alta |
| **MS09-A6** | Dupla implementação C++ (`dynamicRange`) × JS (`computeMS09`) — **convergentes** (mesmo critério e fórmula) | `cpp:749-776` × `html:947-966` | Consistentes entre si; risco estrutural V6 (duplicidade) | Alta |
| **MS09-A7** | Estados de qualidade limitados a `Error` / `NotApplicable` / `Normal`; sem `LowConfidence` | `cpp:758,768-774` | Paralelo a V2 (propagação de qualidade) | Alta |

> **Estado pós-Registro de Decisão MS-09:** **MS09-A1 → resolvido por D1** (critério de teto confirmado);
> **MS09-A2 → resolvido por D2** e **implementado no P1-003** (`ffa739c`), com validação independente
> no Windows;
> **MS09-A4 → resolvido por D3** (caracterização decidida; comportamento → Pilar 1). A3, A5, A6, A7
> permanecem como registro descritivo (fora do escopo das decisões D1–D3).
