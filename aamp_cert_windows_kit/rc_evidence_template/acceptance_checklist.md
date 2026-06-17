# AAMP — Checklist de Aceite e Pré-Condições (RC-Windows-1)

> Todos os itens chegam em **TODO**. Marque **OK** somente com evidência real coletada na
> máquina-alvo, contra os critérios do RUNBOOK. **Não** marque sem evidência. DOC-00 prevalece.

## Pré-condições de release (DOC-12 §3)

| Portão | Conteúdo | Status | Ambiente de verificação | Artefato comprobatório |
|---|---|---|---|---|
| P-01 | 100% MUST verificados (incl. E2E) | TODO | CI + E2E Windows | test_output_windows.log |
| P-02 | Sem CAT-1/3/4 em aberto | TODO | E2E (ER-001/004) + tracker | hw_suite_run.log / tracker |
| P-03 | Suíte automática em hardware real | TODO | Windows HW | hw_suite_run.log |
| P-04 | 15 famílias exibidas no dashboard | TODO | Windows GUI | test_output_windows_e2e.log + capturas |
| P-05 | Exportação válida nos 4 formatos + N/A | TODO | Unit + Windows render | render/*.pdf, render/*.png, CSV/WAV |
| P-06 | DOC-13 sem pendências | OK (baseline) | Revisão da matriz | Matriz Req->Impl->Teste (40/40, 16/16, 15/15) |
| P-07 | Build Release /W4 /WX sem warnings em src/ (Win) | TODO | CI Windows | build_msvc_release.log |
| P-08 | Nada fora do escopo (DOC-01 §3) | OK (baseline) | Auditoria | scope_audit (baseline) |
| P-09 | Sem reconhecimento/classificação/profiling | OK (baseline) | Auditoria | scope_audit (baseline) |
| SLAs | DSP<=15s, captura<=110s, memória<=2GB | TODO | Máquina de referência | slas_run.log |

## Critérios de aceite por módulo (DOC-12 §2) — AC-M01-01 … AC-M08-04

> Liste cada AC com status e ponteiro de TC/evidência. Os AC sustentados por TCs já verdes em
> headless (M-03/04/05/07-CSV-WAV/08, lógica M-01/M-02) podem ser marcados OK com a fonte
> "baseline"; os de M-06 e de render/HW ficam TODO até o run Windows.

| AC | Sustentado por | Status | Evidência |
|---|---|---|---|
| AC-M01-01 | TC-001/002/006 | TODO | |
| AC-M01-02 | TC-003 | OK (baseline) | |
| AC-M01-03 | TC-004 | OK (baseline) | |
| AC-M01-04 | TC-005 | TODO | |
| AC-M02-01 | TC-060 | OK (baseline, loopback) | |
| AC-M02-02 | TC-005 (ER-022) | TODO | |
| AC-M02-03 | TC-005 (ER-025) | OK (baseline, lógica) | |
| AC-M03-01..04 | TC-010..013 | OK (baseline) | |
| AC-M04-01..05 | TC-020..024 | OK (baseline) | |
| AC-M05-01..05 | TC-030..037 | OK (baseline) | |
| AC-M06-01 | TC-040 | TODO | |
| AC-M06-02 | TC-041/045 | TODO | |
| AC-M06-03 | TC-042/043 | TODO | |
| AC-M06-04 | TC-044 | TODO | |
| AC-M07-01 | TC-050/051/055 | OK (baseline) | |
| AC-M07-02 | TC-052 | OK (baseline) | |
| AC-M07-03 | TC-053 | TODO (render) | |
| AC-M07-04 | TC-054 | TODO | |
| AC-M08-01..04 | TC-060..063 | OK (baseline, loopback) | |

## Tabelas de rastreabilidade
- [ ] Estímulo -> Métrica (16 -> 15) confirmada sem pendências (DOC-13 §3).
- [ ] Métrica -> Teste (15 famílias) confirmada sem pendências (DOC-13 §4A).
