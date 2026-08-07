# Estudo de Impacto — Segmentação Temporal do ST-08 (achado F9)

> **Natureza:** estudo **pré-decisão**. Não escolhe solução, não recomenda implementação e não
> altera código, testes, estímulos ou Evidence Package.
> **Origem:** Estudo Conceitual de **F2** (`Dynamic Range < SNR`), que identificou a segmentação
> temporal não alinhada como causa dominante do desvio observado.
> **Documento de referência:** [`metrics-audit.md`](./metrics-audit.md) → *Registro de Decisão — F2*.

---

## 1. O achado (F9)

`AnalysisEngine::dynamicRange()` fatia a captura do ST-08 em **8 janelas fixas de 100 ms a partir
da amostra 0**, sem compensar a latência *round-trip* da cadeia:

```cpp
const int seg = static_cast<int>(0.100 * m_fs + 0.5);
std::vector<double> part = slice(step->second, s * seg, (s + 1) * seg);
```

`ENABLE_SYNC_ALIGNMENT = false` (`CaptureEngine.cpp:17`) — decisão deliberada e documentada
(*"FASE DE MEDICAO: mantido FALSE → NENHUM buffer e alterado"*). O buffer entregue à análise
conserva o atraso da cadeia; o `diag_log` registra `onset ≈ 913 samples` e
`Offset aplicado = 0 samples` em todas as capturas.

**Consequência:** cada janela de 100 ms mistura dois degraus adjacentes.

| Grandeza | Valor |
|---|---:|
| Janela 8 analisada | `[33600, 38400)` |
| Degrau 8 real (−6 dBFS) | `[34513, 39313)` |
| Fração contaminada pelo degrau anterior (−8 dBFS) | **19,02 %** (913/4800) |
| **Viés do teto** | **−0,3161 dB** |
| `DR − SNR` observado na Helix AC30 Clean | **−0,30 dB** |

O viés é **unilateral e sistemático**: sempre reduz o teto, porque cada degrau é precedido por um
degrau mais baixo (progressão monotônica −40 → −6 dBFS).

### 1.1 O problema já é conhecido e já foi resolvido — em outra família

`AnalysisEngine::dynamicBehaviour()` (MS-13) consome **a mesma captura do ST-08** e **alinha antes
de fatiar**, com um comentário que descreve exatamente este mecanismo:

```cpp
// Compensa a latencia estrutural antes de fatiar: alinha a saida a entrada pelo lag de
// correlacao das envelopes; senao cada janela de 100 ms mistura degraus adjacentes (rampa).
const int maxLag = static_cast<int>(0.050 * m_fs);
const int lag = bestLag(extractEnvelope(in, m_fs), extractEnvelope(step->second, m_fs), maxLag);
```

> **MS-09 e MS-13 leem a mesma captura sob convenções de alinhamento diferentes.** O MS-13 está
> alinhado; o MS-09 não. Essa **incoerência interna** é o núcleo de F9, e é independente de F2.

`bestLag()` e `extractEnvelope()` existem nas **duas** implementações (`AnalysisEngine.cpp:876` e
`aamp_laboratory.html:1047`, este último documentado como reprodução fiel da primitiva do Core).
`maxLag = 0,050·f_s = 2400 amostras` — o *offset* observado (≈913) está confortavelmente dentro.

---

## 2. Restrição estrutural que condiciona todas as alternativas

A ordem de aquisição é fixa (`StimulusGenerator::generateSuite()`):

```
A: ST-01 ST-02 ST-03 ST-04 ST-05 ST-06 ST-16 │ B: ST-07 ST-08 ST-09 │ C: ST-10 … ST-15
                                  ▲
                       o offset só é MEDIDO aqui (ST-06)
```

`m_syncMeasured` só se torna verdadeiro **após o ST-06**. Portanto, mesmo com
`ENABLE_SYNC_ALIGNMENT = true`, os estímulos **ST-01 a ST-05 nunca seriam deslocados** — e as
famílias **MS-01…MS-07 permaneceriam intactas**. Além disso `ST-06` e `ST-09` são **excluídos por
construção** (`excluded`, `CaptureEngine.cpp:130`), preservando MS-12 e MS-08.

**As famílias efetivamente expostas a um alinhamento global são: MS-09, MS-13, MS-14, MS-15.**

---

## 3. Alternativas

### A1 — Alinhamento global (`ENABLE_SYNC_ALIGNMENT = true`)

Ativa o deslocamento do buffer capturado em `CaptureEngine::capture()`.

| Eixo | Impacto |
|---|---|
| **Científico** | Corrige F9 na origem. MS-13/14/15 já aplicam `bestLag` **por cima** — passariam a encontrar `lag ≈ 0`, resultado equivalente. MS-01…MS-07 e MS-08/MS-12 não são atingidos (§2). |
| **C++ / JS** | C++: uma constante. **JS: sem contrapartida** — o LabEngine recebe o WAV do Evidence Package, já alinhado ou não. A paridade CC-A passaria a depender de *como o pacote foi gravado*, não do código. |
| **Estímulos** | Nenhum. |
| **Evidence Package** | ⚠️ **Impacto alto.** O áudio gravado passaria a ser o buffer **deslocado**, não o observado. Colide com o princípio de que o pacote preserva **áudio bruto** (verificado no P1-001: o WAV extraído reproduz os valores da tela). Reprocessar pacotes antigos e novos deixaria de ser comparável. |
| **Regressões** | O deslocamento faz *zero-pad* no fim e preserva o tamanho — muda índices absolutos. TC-020 (sincronização), TC-037 (group delay) e MS-12 precisam de reverificação. |
| **Testes** | Reverificar TC-020, TC-030, TC-034, TC-036, TC-037; novos casos para MS-09/MS-13 alinhados. |
| **Risco** | **Alto** — atravessa a fronteira aquisição↔análise e toca a evidência bruta. A decisão que o manteve `false` ("primeiro provamos que o atraso é estrutural") **não foi revogada**. |

### A2 — Localização dos degraus do ST-08 (`bestLag`), no próprio MS-09

Replicar no `dynamicRange()` o que o MS-13 já faz: estimar o *lag* por correlação de envelopes
contra o ST-08 emitido e fatiar em `s·seg + lag`.

| Eixo | Impacto |
|---|---|
| **Científico** | Corrige F9 exatamente onde ele ocorre. **Elimina a incoerência MS-09 × MS-13** (§1.1). Precedente idêntico já validado em produção. |
| **C++ / JS** | C++: `bestLag`/`extractEnvelope` já disponíveis; a fonte vem de `StimulusGenerator::getStimulus()`. JS: primitivas já existem; a fonte vem de `session.stimuli` (`src('ST-08')`), como o `computeMS13` já faz. **Exige alterar a assinatura de `computeMS09()`** para receber o ST-08 emitido. |
| **Estímulos** | Nenhum. |
| **Evidence Package** | Nenhum na gravação. ⚠️ **Cria uma dependência nova:** o MS-09 passaria a exigir a **fonte** do ST-08 no pacote. Pacotes sem `stimuli/ST08_Source.wav` degradariam — precisa de política definida (hoje o MS-09 depende só da captura). |
| **Regressões** | Confinadas ao MS-09. Nenhuma outra família é tocada. |
| **Testes** | Caso com *offset* conhecido injetado → teto deve ficar invariante; não-regressão do MS-13; CC-A sobre as duas implementações. |
| **Risco** | **Baixo/Moderado** — o moderado vem só da nova dependência da fonte. |

### A3 — Intervalo de guarda / região estável dentro de cada degrau

Analisar apenas a porção central de cada janela (p.ex. descartar as bordas), sem estimar *lag*.

| Eixo | Impacto |
|---|---|
| **Científico** | **Mitiga sem corrigir.** Reduz a contaminação proporcionalmente à guarda, mas o viés persiste enquanto a guarda não cobrir o *offset* real, que **é desconhecido pelo estimador**. Uma guarda fixa é um limiar implícito — e limiares pertencem ao **Pilar 2**. Também reduz o número de amostras: 100 ms já comporta **uma única** janela de 4096 (§5), e a guarda inviabilizaria a versão *band-limited*. |
| **C++ / JS** | Simétrico e trivial nas duas. |
| **Estímulos / Evidence** | Nenhum. |
| **Regressões** | Confinadas ao MS-09; muda o valor do teto em toda medição, inclusive as corretas. |
| **Testes** | Caso de invariância ao *offset* **falharia** para *offsets* acima da guarda. |
| **Risco** | **Baixo**, mas **não resolve** o achado e introduz um limiar novo. |

### A4 — Alternativas conservadoras

**A4a — Reutilizar o resultado já alinhado do MS-13.** O `LevelTransfer_step8_dB` do MS-13 é o
ganho do degrau 8 **medido com alinhamento**. O teto seria derivado dele.
*Impacto:* nenhuma primitiva nova; corrige o viés. ⚠️ Cria **dependência entre famílias**
(MS-09 → MS-13), invertendo a hierarquia atual e violando a autonomia científica das famílias.
Se o ST-07 faltar, o MS-13 inteiro não é calculado e o MS-09 cairia junto. **Risco: moderado.**

**A4b — Descartar o último degrau.** Usar o degrau 7 (−8 dBFS) como teto.
*Impacto:* elimina a contaminação do degrau 8, mas **rebaixa o teto em 2 dB** deliberadamente e
agrava o teto censurado (§4). **Não recomendável como correção.**

**A4c — Não corrigir; declarar a incerteza.** Manter o estimador e registrar que o teto carrega
incerteza da ordem de `10·log10[(1−f)+f·10^(Δ/10)]`, com `f` a fração desalinhada e `Δ` o degrau.
*Impacto:* zero em código. Transfere o problema para a apresentação (**Pilar 2**) e mantém a
incoerência MS-09 × MS-13. **Risco: nulo; valor científico: baixo.**

---

## 4. Interação com o teto censurado

O ST-08 termina em **−6 dBFS**. Quando nenhum degrau clipa, o teto observado é o **topo do
ensaio**, não o início da saturação (decisão **D**). Nesse regime:

- o viés de −0,32 dB incide sobre um valor que **já é um limite inferior**;
- corrigir a segmentação **não** transforma o teto censurado em teto normativo;
- A4b agrava a censura.

**F9 e o teto censurado são independentes e nenhum dos dois resolve o outro.**

---

## 5. Restrição a registrar para o P1-003

Um degrau de 100 ms = **4800 amostras**. Uma janela de 4096 cabe **uma única vez**. Portanto uma
estimativa *band-limited* do teto (P1-003/D2) sobre a segmentação atual dispõe de **um quadro
apenas**, sem média — variância alta e sem margem para *overlap* ou intervalo de guarda.

> **Isto condiciona o desenho do P1-003 independentemente de F9.**

---

## 6. Matriz comparativa

| | A1 global | **A2 `bestLag`** | A3 guarda | A4a via MS-13 | A4c não corrigir |
|---|:--:|:--:|:--:|:--:|:--:|
| Corrige o viés | ✅ | ✅ | ⚠️ parcial | ✅ | ❌ |
| Resolve MS-09 × MS-13 | ✅ | ✅ | ❌ | ✅ | ❌ |
| Famílias afetadas | 4 | 1 | 1 | 1 | 0 |
| Toca o áudio bruto | ⚠️ **sim** | não | não | não | não |
| Dependência nova | — | fonte ST-08 | — | ⚠️ família | — |
| Introduz limiar | não | não | ⚠️ **sim** | não | não |
| Precedente no código | — | ✅ **MS-13/14** | — | — | — |
| Risco | **Alto** | **Baixo/Mod.** | Baixo | Moderado | Nulo |

---

## 7. Posição de F9 em relação ao P1-003

Três enquadramentos possíveis, **sem escolha nesta etapa**:

**(i) Item próprio anterior ao P1-003.** F9 é *alinhamento temporal*; P1-003 é *coerência
espectral do teto*. São problemas distintos, com causas e escopos distintos. Tratado antes, o
P1-003 mediria um teto já livre do viés.

**(ii) Pré-condição formal do P1-003.** Análogo a **AR-01** no P1-001: o P1-003 só inicia após F9
estar corrigido e validado. Mantém a separação conceitual e impõe a ordem.

**(iii) Incorporado ao P1-003.** Um único item toca o teto uma só vez. Risco: mistura duas causas
independentes num item, contra a rastreabilidade um-a-um adotada no Pilar 1.

**Argumento técnico registrado, sem decidir:** (i) e (ii) preservam a rastreabilidade que o Pilar 1
vem praticando — um item, uma causa, uma evidência. A escolha entre elas depende de o P1-003 dever
ou não aguardar a validação de F9, o que é decisão de sequenciamento e cabe ao autor do plano.

---

## 8. Pontos que exigem decisão

1. **Alternativa** (A1 / A2 / A3 / A4a / A4c) — nenhuma escolhida.
2. **Posição de F9** (item próprio / pré-condição / incorporado).
3. **Se A2**: política quando o pacote não contém a fonte do ST-08.
4. **Se A1**: revogação explícita da decisão que mantém `ENABLE_SYNC_ALIGNMENT = false`, e posição
   sobre o áudio bruto no Evidence Package.
5. **Convivência com o teto censurado** — corrigir F9 não o resolve.

---

## 9. Encerramento do estudo *(acrescentado após a decisão)*

> O texto acima é preservado **como estava no momento da decisão** — inclusive a frase original
> "nenhuma alternativa foi escolhida" —, para que o registro histórico permaneça auditável. Esta
> seção apenas consigna o desfecho.

**Alternativa adotada: A2** — alinhamento temporal local no MS-09 por `extractEnvelope`/`bestLag`,
mesma convenção já empregada pelo MS-13. Justificativa: **menor escopo** (uma função em cada
implementação, nenhuma outra família tocada, áudio bruto e Evidence Package intactos), **precedente
existente** (a técnica já validada em produção sobre a mesma captura) e **rastreabilidade** (resolve
F9 onde F9 ocorre e elimina a incoerência MS-09 × MS-13).

**Posição adotada: item próprio e pré-requisito do P1-003** — enquadramento (i)/(ii) da §7. **Não**
incorporado ao P1-003.

**Decisões da §8 resolvidas:** (1) alternativa → **A2**; (2) posição → **item próprio, pré-requisito**;
(3) fonte ST-08 ausente → **`NotApplicable`**, sem *fallback* desalinhado; (4) não se aplica (A1 não
foi adotada — `ENABLE_SYNC_ALIGNMENT` permanece `false`).
**Permanece aberta:** (5) a convivência com o **teto censurado** (decisão **D**) — F9 não a resolve,
como o próprio estudo antecipou na §4.

**Implementação:** `cfb95fa600f31e7844552f56b30f7ac018757fdb` · **regressão permanente:** TC-041 ·
**validação independente no Windows: APROVADO** (CTest 2/2; `aamp_headless_suite` 497 checks,
`aamp_scientific_validation` 8 checks; SHA-256 do `AAMP.exe`
`2145781C1F61742FAAF5D42C94050C05E2956CC83A24ED0CC5DEAE670E4C80AE`).

Registro completo em [`metrics-audit.md`](./metrics-audit.md) → *Registro de Encerramento — F9*.
