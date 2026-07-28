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
| **MS-08** | Noise Floor | Piso de ruído integrado | `10·log10(Σ_banda P̄(f))`, `P̄` média de frames Hann 4096 (16 frames / ou todos), 50% overlap | dBFS | ST-09 Silence | `cpp:684-746`; 16 frames = `Constants.h:59` | *a definir* | Todas |
| **MS-09** | Dynamic Range | Faixa dinâmica útil | `20·log10(RMSmax_degrau_não-clipado) − NoiseFloor_dBFS` | dB | ST-08 + ST-09 | `cpp:749-776` | `NotApplicable` se todos os degraus clipados | Todas |
| **MS-10** | **SNR** | Relação sinal-ruído | `10·log10(ps/pn)`, `ps`=meanSquare(tom 1 kHz), `pn`=meanSquare(silêncio); satura em **200** se `pn=0` | dB | ST-03 + ST-09 | `cpp:71-76, 779-791` | gauge exibido **0–120 dB** (`html:2721`); piso de retorno na captura **20 dB acima do floor** (`MIN_SNR_OVER_FLOOR_DB`, `Constants.h:47`) | Todas |
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
- **V8 — MS-10 (SNR): numerador `S/N` vs `(S+D+N)/N` — decisão de especificação PENDENTE.**
  *(Não é um defeito; é uma escolha a ser feita conscientemente antes de qualquer alteração de código ou teste.)*
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

- **D1 — Definição de ruído.** A definição normativa de ruído do MS-10 passa a ser **band-limited,
  alinhada ao MS-08** (mesma banda 20 Hz–20 kHz). Uma verificação **independente** de energia fora da
  banda (DC, subsônicos, hum) permanece na **camada de Integridade** (não no `pn` do SNR). *Racional:
  consistência interna entre SNR e Noise Floor + comparabilidade normativa, sem perder detecção de
  anomalias fora de banda.*
- **D2 / V8 — Numerador.** O MS-10 é formalizado como **S/N estrito** (numerador = energia do tom de
  referência isolada, não o bloco inteiro). **THD, THD+N e IMD** continuam responsáveis pela distorção.
  O rótulo **"SNR" deve corresponder ao comportamento da métrica**. *Isto encerra a decisão pendente do
  V8 na direção (a): alinhado ao que o TC-033 já verifica.*
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
| **D1** | Ruído band-limited (alinhado ao MS-08) + checagem fora-de-banda na Integridade | B | **Altera algoritmo** · **Altera testes** · **Altera UI** · **Altera documentação** | — |
| **D2/V8** | S/N estrito; distorção fica com MS-05/06/07 | A | **Altera algoritmo** · **Altera testes** · **Altera documentação** | — |
| **D3** | Manter output-referred | A | **Sem impacto de código** · **Altera documentação** | — |
| **D4** | Propagar estados de qualidade; SNR baixo reduz confiança | B (princípio) | **Altera algoritmo** · **Altera UI** · **Altera testes** · **Altera documentação** | **Sim** — estado de `pn≈0` (→ P1, no máx. antes do P2); limiares → P2 |
| **D5** | Conceito de apresentação aprovado | B | **Altera UI** · **Altera documentação** (concretização → P2) | Faixas/saturação → P2 |

### Matriz de Rastreabilidade — MS-10

| Decisão | Arquivos afetados | Testes afetados | Achados relacionados | Pilar |
|---------|-------------------|-----------------|----------------------|-------|
| **D1** | `src/analysis/AnalysisEngine.cpp` (`snr`/`snrDb`); `resources/aamp_laboratory.html` (checagem fora-de-banda na Integridade) | `tests/test_analysis.cpp` (TC-033) | V8, V1, V6 | Pilar 1 |
| **D2/V8** | `src/analysis/AnalysisEngine.cpp` (`snr`); `src/analysis/AnalysisEngine.h` (contrato) | `tests/test_analysis.cpp` (TC-033 + novo caso S/N estrito) | V8, V2 | Pilar 1 |
| **D3** | `docs/laboratory/metrics-audit.md` (somente documentação) | — | — | Pilar 1 (doc) |
| **D4** | `src/analysis/AnalysisEngine.cpp` (qualidade do `snr`); `resources/aamp_laboratory.html` (porta inferior da confiança `:2226`, `normalizeMetrics`) | `tests/test_analysis.cpp` (novos casos de estado) | V1, V2 | Pilar 1 (princípio) / Pilar 2 (limiares) |
| **D5** | `resources/aamp_laboratory.html` (gauge/chip `:2713,:2721`) | — | V5 | Pilar 2 |

### Encerramento

**MS-10 encerrado formalmente** como especificação aprovada, com **um único item em aberto**
explicitamente rastreado: o estado de qualidade para `pn≈0` (D4). A implementação (Pilar 1) do MS-10
deve seguir este registro como referência normativa.

---

## MS-08 (Noise Floor) — decisão N1 em revisão

**N1 — Definição normativa de "Noise Floor": SUSPENSA.**
- **Decisão pendente:** qual grandeza física o AAMP chamará de "Noise Floor" — **nível integrado em
  banda** (RMS / potência in-band, dBFS) **ou** **densidade espectral** (PSD, dBFS/Hz). *(Após Parseval,
  estas são as únicas duas famílias — ver Apêndice A.)*
- **Motivo da suspensão:** a dúvida deixou de ser sobre a **grandeza física** e migrou para a
  **metrologia da implementação**. A pergunta anterior a responder é: *"a implementação atual mede
  corretamente essa grandeza?"* — ver **N1-R1** e o **Apêndice B**.
- **Referência:** **Apêndice A** (estudo comparativo das definições) e **Apêndice B** (investigação da
  normalização espectral).
- **N2–N6** permanecem **em revisão**, aguardando aprovação; não serão registrados como decididos até lá.

**N1-R1 — Precondição de normalização (ENBW).** A independência em relação à FFT pressupõe normalização
adequada para **ruído** (ENBW / Parseval). A implementação atual utiliza correção de **ganho coerente**
(`windowCoherentGain = Σw`, `Windows.cpp:46`), apropriada para **tons discretos**. Esta observação **não
invalida nenhuma definição normativa**, mas deverá ser considerada quando o **Pilar 1** entrar na fase de
implementação. **Verificado (Apêndice B):** a estimativa atual é energeticamente correta para **tons**,
**não** para ruído de banda larga (falta o fator ENBW / Σw²). *Provável reaparecimento ao revisar MS-09,
espectro, Dynamic Range e exportação.*

---

## Encadeamento com os pilares

1. **Pilar 1 (validar implementação):** endereça **V1** (porta inferior da confiança), **V2**
   (propagação de `Quality`), **V3** (semântica da Consistência), **V4** (Reproducibility), **V7** (DBG).
   Pré-decisão de especificação a resolver antes de tocar código/teste: **V8** (numerador do MS-10:
   `S/N` vs `(S+D+N)/N`).
2. **Pilar 2 (apresentação normativa):** define e expõe as faixas da **Seção C** (V5) de forma uniforme.
3. **Pilar 3 (exportação):** congela o formato só depois que Seções A–C estiverem estáveis.

---

# Apêndice A — Estudo N1: Definição Normativa de Noise Floor

> Embasamento técnico da decisão **N1 (SUSPENSA)**. Estudo comparativo das definições candidatas.
> **Não** contém decisão de implementação. N1 permanece em aberto.

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

### A.4 Recomendação (não-vinculante — N1 permanece suspensa)

Grandeza-título "Noise Floor" = **(1) nível integrado em banda (RMS / potência in-band, dBFS, 20–20 k,
flat)** — padrão de ficha técnica, serve diretamente ao SNR (D1) e é **metrologicamente coerente com
MS-09/MS-10/Trust/Exportação** (A.2). **PSD/Hz** como **descritor complementar** (a curva por bin já
existe). Precondição obrigatória: **N1-R1** (normalização ENBW). N1 **não** está decidida.

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
