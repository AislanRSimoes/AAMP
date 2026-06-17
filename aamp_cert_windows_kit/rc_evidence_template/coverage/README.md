# Cobertura — RC-Windows-1 (PREENCHER)

Gerar na máquina-alvo e depositar aqui:
- HTML navegável (OpenCppCoverage no Windows; ou lcov) + resumo por arquivo/módulo.
- Meta (DOC-11 §4): >= 80% de linhas em Analysis, Stimulus e Export.

Referência da baseline headless (gcov): Analysis+Stimulus agregado ~97%; M-07 97%, M-01 80%,
M-02 93%, M-04 100%, M-08 81%. Reconfirmar no Windows.

Sugestão (OpenCppCoverage):
  OpenCppCoverage --sources src --export_type html:rc_evidence\coverage\html -- ^
    build\windows-debug\Debug\aamp_tests.exe        # AJUSTE: nome do exe de teste
