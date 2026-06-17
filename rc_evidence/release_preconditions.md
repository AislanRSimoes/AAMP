# AAMP — Pré-Condições de Release P-01..P-09 (DOC-12 §3)

> **Definição de RC da própria suíte** (Plano de Testes §7, Plano de Implementação §11):
> o RC só ocorre quando **P-01..P-09 estiverem cumpridas no ambiente que a suíte exige**
> (Windows + macOS + hardware real). P-03, P-07(MSVC/AppleClang), SLAs e E2E são, por
> definição da suíte, cumpridos na **máquina-alvo** — não neste ambiente headless Linux.

| # | Pré-condição | Ambiente | Estado (Fases 1–4: A,B,C,D,G-headless,E-lógica) |
|---|---|---|---|
| P-01 | 100% dos MUST implementados e verificados | CI + E2E | **Parcial** — M-03, M-05, M-04, M-08 e M-07(CSV/WAV) verificados (FR-014..027, FR-037..040, FR-018..023, FR-032/033/036); lógica de M-01/M-02 verificada (TC-003/004 + routing); M-06, OS real (TC-002/005) e PDF/PNG pendentes |
| P-02 | Nenhum CAT-1/3/4 em aberto | Issue tracker | OK no escopo entregue (clipping/ER-020, parcial/ER-024, retry 3/1/1, degradação/ER-032/033 cobertos) |
| P-03 | Suíte automática em hardware real, ambas plataformas | **Máquina-alvo** | ⏳ Fluxo automático completo verificado em loopback (TC-060); hardware real pendente |
| P-04 | 15 famílias calculadas e exibidas no dashboard | Integração + Sistema | **Parcial** — 15 famílias **calculadas** na orquestração completa (TC-060); exibição (M-06) pendente |
| P-05 | Exportação válida nos 4 formatos com N/A correto | Unit + Sistema | **Parcial** — CSV+WAV válidos e N/A correto verificados (TC-050/051/052/055); PDF (modelo verificado) e PNG render na máquina-alvo |
| P-06 | DOC-13 sem pendências | Revisão da matriz | ✅ Matriz Req→Impl→Teste fechada |
| P-07 | Build Release sem warnings em src/ (Win e macOS) | **CI máquina-alvo** | **Parcial** — todo o núcleo compila com `-Wall -Wextra -Wpedantic` **zero warnings** (g++ 13.3); MSVC/AppleClang pendentes |
| P-08 | Nada fora do escopo DOC-01 §3 | Auditoria | ✅ 0 ocorrências (marca de HW, profiling, neural, classificação, DB, histórico) |
| P-09 | Sem reconhecimento/classificação/profiling | Auditoria | ✅ Verificado; métricas descrevem o sistema sob teste |

## Auditoria de escopo (P-08/P-09) — varredura de `src/`
- 0 referências a fabricante/modelo (Helix/Kemper/Quad Cortex/Tonex).
- 0 referências a profiling/neural/classificação/reconhecimento/inferência/banco de dados/histórico.
- Modelo automático: `runSuite()` é o único comando; nenhuma seleção de estímulo/métrica/modo/método.
- Sem persistência: o núcleo entrega `ResultBundle`; não há histórico nem comparação entre sessões.

## Conclusão honesta
Esta entrega **continua NÃO sendo um Release Candidate** pela definição da própria suíte.
É o **núcleo headless determinístico das Fases 1–4**: M-03, M-05, M-04, M-08, M-07(CSV/WAV) e a lógica de M-01/M-02,
com **314 checks / 41 casos** executados de fato (0 falhas) e cobertura medida por gcov
(Analysis+Stimulus ~97%). Restam: caminhos de OS real de M-01/M-02 (hot-plug/desconexão), Bloco F (M-06 GUI),
render PDF/PNG de M-07 (máquina-alvo) e a Fase 5
(P-03/P-07-MSVC/AppleClang/E2E/SLAs na máquina-alvo Windows+macOS+hardware).
