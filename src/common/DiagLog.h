// AAMP - src/common/DiagLog.h
// Canal UNICO de diagnostico (diag_log.txt). Infraestrutura de logging: nao participa de
// nenhum calculo cientifico, nao altera DSP, metricas, estimulos nem exportacao.
//
// Antes desta abstracao existiam QUATRO caminhos de escrita independentes (JuceCaptureDevice::
// diagLog, CaptureEngine::m04log, MeasurementOrchestrator::evidBaselineLog e a ponte web), dois
// deles com o caminho do arquivo duplicado literalmente e nenhum com carimbo de tempo. Todos
// passam a encaminhar para write() -- os wrappers originais foram preservados, de modo que as
// 39 chamadas existentes nao mudaram.
//
// Plain C++17: aamp_core continua compilando SEM JUCE (suite headless).
#pragma once
#include <string>
#include <cstdint>

namespace aamp {
namespace diag {

// Abre a sessao: arquiva o diag_log.txt anterior em logs/ e escreve o cabecalho.
// IDEMPOTENTE -- chamadas repetidas nao produzem um segundo cabecalho.
void begin();

// UNICO ponto de escrita. Carimba a linha com data/hora absoluta (system_clock) e tempo
// relativo ao inicio da sessao (steady_clock), e serializa o acesso ao arquivo.
//
// RESTRICAO DE TEMPO REAL: nao chamar do caminho de audio por bloco/amostra
// (audioDeviceIOCallback). Formata, aloca e faz I/O.
void write(const std::string& line);

// Escreve o rodape (End/Elapsed/Exit). IDEMPOTENTE -- nunca duplica.
// Quando o processo termina de forma anormal o rodape simplesmente nao existe: essa ausencia
// e a evidencia de interrupcao e NAO deve ser fabricada pela sessao seguinte.
void end(const std::string& exitReason = "Normal");

// Caminho do arquivo da sessao corrente; "" se o logging estiver indisponivel.
std::string currentPath();

// Identificador unico da sessao corrente; "" se a sessao ainda nao foi aberta.
std::string currentSessionId();

// ---------------------------------------------------------------------------------------
// INTERNO -- injecao de relogio e diretorio para tornar os testes deterministicos.
// NAO faz parte da API de produto do AAMP e nao e usado por nenhum caminho de producao.
// ---------------------------------------------------------------------------------------
namespace test_detail {

struct Hooks {
    std::string baseDir;                        // substitui %APPDATA%\AAMP
    std::int64_t (*wallEpochMs)()  = nullptr;   // substitui system_clock
    std::int64_t (*monotonicMs)()  = nullptr;   // substitui steady_clock
};

void installHooks(const Hooks& h);
void clearHooks();
void closeSessionForTest();   // encerra o estado da sessao, permitindo novo begin() no processo

} // namespace test_detail

} // namespace diag
} // namespace aamp
