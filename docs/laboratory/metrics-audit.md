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

---

## Encadeamento com os pilares

1. **Pilar 1 (validar implementação):** endereça **V1** (porta inferior da confiança), **V2**
   (propagação de `Quality`), **V3** (semântica da Consistência), **V4** (Reproducibility), **V7** (DBG).
2. **Pilar 2 (apresentação normativa):** define e expõe as faixas da **Seção C** (V5) de forma uniforme.
3. **Pilar 3 (exportação):** congela o formato só depois que Seções A–C estiverem estáveis.
