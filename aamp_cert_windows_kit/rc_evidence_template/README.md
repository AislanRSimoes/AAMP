# rc_evidence/ — Pacote de Evidências do RC-Windows-1 (Plano de Testes §9.6)

Estrutura exigida:
- coverage/                HTML + resumo (>=80% Analysis/Stimulus/Export)
- test_list.csv            todos os TC-001..063 com nivel/ambiente/status
- test_output_windows.log  saida bruta concatenada por build/execucao
- tc_fr_ac_map.csv          TC -> FR -> AC -> estimulos/metricas -> evidencia
- acceptance_checklist.md   AC-* e P-01..P-09 com status e ponteiro
- release_preconditions.md  atualizado para o estado Windows

Logs por execucao (gerados pelos scripts): build_msvc_release.log, test_output_windows_unit.log,
test_output_windows_e2e.log, hw_suite_run.log, slas_run.log, render/.

> Sem este pacote completo e com evidencia real, a entrega e status de progresso, nao RC.
