// AAMP - tests/test_diaglog.cpp
// Infraestrutura de diagnostico (src/common/DiagLog): carimbo de tempo, cabecalho de sessao,
// arquivamento e serializacao entre threads. NAO exercita DSP, metricas nem exportacao.
//
// Todos os casos usam a injecao interna de relogio e diretorio (diag::test_detail), de modo que
// os resultados sao deterministicos e nada e escrito em %APPDATA%.
#include "TestRunner.h"
#include "../src/common/DiagLog.h"
#include "AampBuildInfo.h"
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>
#include <set>
#include <cctype>

namespace fs = std::filesystem;
using namespace aamp;

namespace {

// ---- relogios falsos (ponteiros de funcao exigidos por diag::test_detail::Hooks) ----
std::int64_t g_wall = 0;
std::int64_t g_mono = 0;
std::int64_t fakeWall() { return g_wall; }
std::int64_t fakeMono() { return g_mono; }

// 2026-08-04 10:15:32.451 UTC -> valor fixo qualquer; o teste nao depende do fuso.
const std::int64_t kWall0 = 1785838532451LL;

fs::path freshDir(const std::string& name) {
    const fs::path d = fs::temp_directory_path() / ("aamp_diaglog_" + name);
    std::error_code ec;
    fs::remove_all(d, ec);
    fs::create_directories(d, ec);
    return d;
}

void openSession(const fs::path& dir) {
    diag::test_detail::closeSessionForTest();
    g_wall = kWall0; g_mono = 0;
    diag::test_detail::Hooks h;
    h.baseDir     = dir.string();
    h.wallEpochMs = &fakeWall;
    h.monotonicMs = &fakeMono;
    diag::test_detail::installHooks(h);
}

void closeSession() {
    diag::test_detail::closeSessionForTest();
    diag::test_detail::clearHooks();
}

std::vector<std::string> readLines(const fs::path& p) {
    std::vector<std::string> out;
    std::ifstream in(p);
    std::string l;
    while (std::getline(in, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        out.push_back(l);
    }
    return out;
}

int countLines(const std::vector<std::string>& v, const std::string& needle) {
    int n = 0;
    for (const std::string& l : v) if (l.find(needle) != std::string::npos) ++n;
    return n;
}

// "YYYY-MM-DD HH:MM:SS.mmm " -> 24 caracteres com digitos e separadores nas posicoes fixas.
bool hasValidAbsoluteStamp(const std::string& l) {
    if (l.size() < 24) return false;
    const char* sep = "----0-0--0-0:0:0.000";   // mapa posicional (0 = digito)
    static const int digits[] = {0,1,2,3,5,6,8,9,11,12,14,15,17,18,20,21,22};
    (void) sep;
    for (int i : digits) if (!std::isdigit(static_cast<unsigned char>(l[static_cast<std::size_t>(i)]))) return false;
    return l[4] == '-' && l[7] == '-' && l[10] == ' ' && l[13] == ':' && l[16] == ':' && l[19] == '.';
}

// Extrai os milissegundos de "[T+HH:MM:SS.mmm]"; -1 se ausente/malformado.
long long relativeMs(const std::string& l) {
    const std::size_t a = l.find("[T+");
    if (a == std::string::npos) return -1;
    const std::size_t b = l.find(']', a);
    if (b == std::string::npos || b - a != 15) return -1;         // "[T+HH:MM:SS.mmm]" = 16 chars
    const std::string v = l.substr(a + 3, 12);                    // HH:MM:SS.mmm
    if (v.size() != 12 || v[2] != ':' || v[5] != ':' || v[8] != '.') return -1;
    return std::stoll(v.substr(0, 2)) * 3600000LL
         + std::stoll(v.substr(3, 2)) * 60000LL
         + std::stoll(v.substr(6, 2)) * 1000LL
         + std::stoll(v.substr(9, 3));
}

} // namespace

// (1) e (7): o logger funciona sem JUCE (este binario nao linka JUCE) e carimba a data absoluta.
TEST_CASE("DiagLog: linha carimbada com data absoluta valida (headless, sem JUCE)") {
    const fs::path d = freshDir("abs");
    openSession(d);
    diag::write("linha de teste");
    const fs::path log = fs::path(diag::currentPath());
    CHECK(!log.empty() && fs::exists(log));
    CHECK(log.filename().string() == "diag_log.txt");
    const std::vector<std::string> ls = readLines(log);
    bool achou = false;
    for (const std::string& l : ls)
        if (l.find("linha de teste") != std::string::npos) { achou = true; CHECK(hasValidAbsoluteStamp(l)); }
    CHECK(achou);
    closeSession();
}

// (2): o tempo relativo e monotonico e reflete o avanco do steady_clock injetado.
TEST_CASE("DiagLog: tempo relativo monotonico e proporcional ao relogio") {
    const fs::path d = freshDir("rel");
    openSession(d);
    diag::write("t0");
    g_mono += 371;  g_wall += 371;  diag::write("t1");
    g_mono += 8000; g_wall += 8000; diag::write("t2");
    g_mono += 143665 - 8371; g_wall += 143665 - 8371; diag::write("t3");

    long long prev = -1; int vistos = 0;
    for (const std::string& l : readLines(fs::path(diag::currentPath()))) {
        if (l.find("\tX") != std::string::npos) continue;
        if (l.find(" t0") == std::string::npos && l.find(" t1") == std::string::npos
         && l.find(" t2") == std::string::npos && l.find(" t3") == std::string::npos) continue;
        const long long r = relativeMs(l);
        CHECK(r >= 0);
        CHECK(r >= prev);                 // monotonico
        prev = r; ++vistos;
    }
    CHECK(vistos == 4);
    CHECK(prev == 143665);                // 00:02:23.665 -- formato sem estouro em sessao longa
    closeSession();
}

// (3): sessoes distintas recebem identificadores distintos (mesmo PID, mesmo relogio).
TEST_CASE("DiagLog: duas sessoes possuem SessionId distintos") {
    const fs::path d = freshDir("sid");
    openSession(d);
    diag::write("s1");
    const std::string id1 = diag::currentSessionId();
    diag::end("Normal");

    openSession(d);                        // mesmo diretorio, mesmo relogio falso
    diag::write("s2");
    const std::string id2 = diag::currentSessionId();
    CHECK(!id1.empty() && !id2.empty());
    CHECK(id1 != id2);
    closeSession();
}

// (4): begin() e idempotente -- nunca escreve um segundo cabecalho.
TEST_CASE("DiagLog: begin() idempotente (cabecalho unico)") {
    const fs::path d = freshDir("begin");
    openSession(d);
    diag::begin(); diag::begin(); diag::begin();
    diag::write("apos begins");
    const std::vector<std::string> ls = readLines(fs::path(diag::currentPath()));
    CHECK(countLines(ls, "AAMP DIAGNOSTIC SESSION BEGIN") == 1);
    closeSession();
}

// (5): end() e idempotente -- nunca duplica o rodape.
TEST_CASE("DiagLog: end() idempotente (rodape unico, com Elapsed)") {
    const fs::path d = freshDir("end");
    openSession(d);
    diag::write("corpo");
    g_mono += 143665; g_wall += 143665;
    diag::end("Normal"); diag::end("Normal"); diag::end("Abnormal");
    const std::vector<std::string> ls = readLines(fs::path(diag::currentPath()));
    CHECK(countLines(ls, "AAMP DIAGNOSTIC SESSION END") == 1);
    CHECK(countLines(ls, "Elapsed: 00:02:23.665") == 1);
    CHECK(countLines(ls, "Exit: Normal") == 1);
    CHECK(countLines(ls, "Exit: Abnormal") == 0);   // a 1a razao prevalece; nada e sobrescrito
    closeSession();
}

// (6): escritas concorrentes nao intercalam e nenhuma linha se perde.
TEST_CASE("DiagLog: multiplas threads nao produzem linhas intercaladas") {
    const fs::path d = freshDir("mt");
    openSession(d);
    const int kThreads = 8, kPerThread = 60;
    std::vector<std::thread> ts;
    for (int t = 0; t < kThreads; ++t)
        ts.emplace_back([t, kPerThread] {
            for (int i = 0; i < kPerThread; ++i)
                diag::write("MT thread=" + std::to_string(t) + " seq=" + std::to_string(i));
        });
    for (std::thread& th : ts) th.join();

    std::set<std::string> vistos;
    int malformadas = 0;
    for (const std::string& l : readLines(fs::path(diag::currentPath()))) {
        const std::size_t p = l.find("MT thread=");
        if (p == std::string::npos) continue;
        // A carga util precisa comecar exatamente apos o prefixo carimbado: qualquer intercalacao
        // deixaria texto residual entre o "]" e o "MT".
        if (!hasValidAbsoluteStamp(l) || relativeMs(l) < 0) { ++malformadas; continue; }
        const std::size_t fim = l.find("] ");
        if (fim == std::string::npos || fim + 2 != p) { ++malformadas; continue; }
        vistos.insert(l.substr(p));
    }
    CHECK(malformadas == 0);
    CHECK(static_cast<int>(vistos.size()) == kThreads * kPerThread);   // nenhuma linha perdida
    closeSession();
}

// (8): ao abrir uma nova sessao, a anterior e preservada em logs/ -- nunca apagada.
TEST_CASE("DiagLog: arquivamento preserva integralmente a sessao anterior") {
    const fs::path d = freshDir("arch");
    openSession(d);
    diag::write("CONTEUDO DA SESSAO ANTERIOR");
    const std::string id1 = diag::currentSessionId();
    diag::end("Normal");

    openSession(d);                         // nova execucao: deve arquivar a anterior
    diag::write("CONTEUDO DA SESSAO NOVA");

    const fs::path live = fs::path(diag::currentPath());
    const std::vector<std::string> novo = readLines(live);
    CHECK(countLines(novo, "CONTEUDO DA SESSAO NOVA") == 1);
    CHECK(countLines(novo, "CONTEUDO DA SESSAO ANTERIOR") == 0);   // sessoes nao se misturam

    // O historico existe e carrega o conteudo (e o SessionId) da sessao arquivada.
    std::error_code ec;
    CHECK(fs::exists(d / "logs", ec));
    int arquivos = 0, comConteudo = 0, comId1 = 0;
    for (const fs::directory_entry& e : fs::directory_iterator(d / "logs", ec)) {
        ++arquivos;
        const std::vector<std::string> ls = readLines(e.path());
        if (countLines(ls, "CONTEUDO DA SESSAO ANTERIOR") == 1) ++comConteudo;
        if (countLines(ls, "SessionId: " + id1) == 1) ++comId1;
    }
    CHECK(arquivos == 1);
    CHECK(comConteudo == 1);
    CHECK(comId1 == 1);
    closeSession();
}

// (9): o cabecalho carrega a proveniencia do binario (commit, configuracao, versao).
TEST_CASE("DiagLog: cabecalho contem commit, configuracao e versao do build") {
    const fs::path d = freshDir("hdr");
    openSession(d);
    diag::write("qualquer");
    const std::vector<std::string> ls = readLines(fs::path(diag::currentPath()));
    CHECK(countLines(ls, std::string("CommitFull: ")  + AAMP_BUILD_COMMIT_FULL)  == 1);
    CHECK(countLines(ls, std::string("CommitShort: ") + AAMP_BUILD_COMMIT_SHORT) == 1);
    CHECK(countLines(ls, std::string("Branch: ")      + AAMP_BUILD_BRANCH)       == 1);
    CHECK(countLines(ls, std::string("GitDirty: ")    + AAMP_BUILD_GIT_DIRTY)    == 1);
    CHECK(countLines(ls, std::string("BuildConfig: ") + AAMP_BUILD_CONFIG)       == 1);
    CHECK(countLines(ls, std::string("Version: ")     + AAMP_BUILD_VERSION)      == 1);
    CHECK(countLines(ls, std::string("BuildTime: ")   + AAMP_BUILD_TIME)         == 1);
    CHECK(countLines(ls, "SessionId: " + diag::currentSessionId())               == 1);
    CHECK(countLines(ls, "PID: ")            == 1);
    CHECK(countLines(ls, "ExecutablePath: ") == 1);
    // O commit precisa ser um SHA completo (40 hex) -- "unknown" so seria aceitavel fora de um
    // repositorio git, o que nao e o caso do build oficial.
    const std::string sha = AAMP_BUILD_COMMIT_FULL;
    CHECK(sha.size() == 40);
    bool hex = true;
    for (char c : sha) if (!std::isxdigit(static_cast<unsigned char>(c))) hex = false;
    CHECK(hex);
    closeSession();
}
