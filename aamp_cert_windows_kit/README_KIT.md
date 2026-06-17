# AAMP — Kit de Execução da Certificação Windows (RC-Windows-1)

**Suíte de referência:** AAMP v2.0.0 · **Autoridade:** DOC-00 (prevalece em conflito).
**Baseline de entrada:** AAMP_HANDOFF, AAMP_README, AAMP_release_preconditions (Fases 1–4).
**Origem dos critérios:** RPT-RC-WIN-1, Plano de Testes §3/§7/§9, Matriz Req→Impl→Teste.

---

## O que este kit é

Um pacote de execução **pronto para rodar em uma máquina Windows real** (Windows 10/11 +
Visual Studio 2022 / MSVC v143 + JUCE + hardware de áudio + driver ASIO/WASAPI). Ele fecha,
**por execução real na máquina-alvo**, os portões pendentes identificados no RPT-RC-WIN-1:
**P-03, P-04, P-05, P-07 e o §9 (Pacote de Evidências)**.

## O que este kit NÃO faz

- **Não gera evidência fictícia.** Os scripts apenas executam comandos reais e *capturam* a
  saída real. Nenhum log de exemplo neste kit é evidência — exemplos de formato estão
  marcados como `EXEMPLO DE FORMATO`.
- **Não marca portões como CONCLUÍDO.** O `acceptance_checklist.md` chega com todos os
  itens em `TODO`. A passagem para `OK` é uma decisão humana, tomada contra os critérios
  objetivos, com base na evidência real coletada.
- **Não executa certificação simulada.** Sem a máquina-alvo, nada é certificado.

## Conteúdo

```
aamp_cert_windows_kit/
  README_KIT.md                 (este arquivo)
  RUNBOOK_RC_WINDOWS.md         checklist operacional, sequência de portões, comandos,
                                formato de logs, critérios pass/fail, coleta, emissão
  scripts/
    lib_common.ps1              helpers (tee, exit-code, contagem de warnings, dimensão PNG)
    00_check_prereqs.ps1        verifica toolchain/ambiente
    10_build_msvc_release.ps1   P-07 — build MSVC Release /W4 /WX
    20_run_unit_integration.ps1 re-verifica núcleo no Windows (ctest)
    30_run_e2e_windows.ps1      TC-040..045, TC-053(render), TC-054 (AAMP_E2E)
    40_run_hardware_suite.ps1   P-03 — suíte automática em hardware real (TC-001/002/005/060)
    50_measure_slas.ps1         SLAs DOC-09 §6/§7 (DSP/captura/memória)
    60_collect_evidence.ps1     monta rc_evidence/ a partir das saídas reais
  rc_evidence_template/         estrutura §9.6 com campos a preencher
    coverage/
    test_list.csv
    test_output_windows.log
    tc_fr_ac_map.csv
    acceptance_checklist.md
    release_preconditions.md
```

## Pré-requisito que o kit assume (não incluído — está no aamp_core.zip)

O **código-fonte AAMP** (repositório com `src/`, `tests/`, `CMakePresets.json`,
`.github/workflows/ci.yml` e o scaffolding turnkey: `JuceDeviceBackend.h`,
`JuceRenderWriters.h`, `DashboardComponent.h`, `tests/e2e/E2E_Specs.cpp`).
Coloque este kit na **raiz do repositório** clonado na máquina Windows e siga o RUNBOOK.

> Os nomes de preset/target/exe usados nos scripts seguem os documentados na baseline
> (`windows-release`, `AAMP_E2E`). Ajuste-os caso o `CMakePresets.json`/`CMakeLists.txt`
> do repositório use nomes diferentes — os pontos de ajuste estão sinalizados com `# AJUSTE:`.
