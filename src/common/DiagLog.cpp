// AAMP - src/common/DiagLog.cpp
// Implementacao do canal unico de diagnostico. Ver DiagLog.h.
#include "DiagLog.h"
#include "AampBuildInfo.h"   // gerado por cmake/GenerateBuildInfo.cmake a cada build

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

#if defined(_WIN32)
  #include <windows.h>
  #include <process.h>
#else
  #include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace aamp {
namespace diag {
namespace {

// ---- estado da sessao (protegido por g_mx) ----
std::mutex    g_mx;
bool          g_begun = false;
bool          g_ended = false;
std::ofstream g_out;
std::string   g_path;
std::string   g_sessionId;
std::int64_t  g_t0Mono = 0;

// ---- injecao de teste ----
std::string   g_hookDir;
std::int64_t (*g_hookWall)() = nullptr;
std::int64_t (*g_hookMono)() = nullptr;

// Distingue sessoes abertas no mesmo processo (mesmo PID, possivelmente mesmo milissegundo).
std::atomic<unsigned> g_sessionSeq{0};

// ------------------------------- tempo -------------------------------
std::int64_t wallEpochMs() {
    if (g_hookWall != nullptr) return g_hookWall();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch()).count();
}

std::int64_t monotonicMs() {
    if (g_hookMono != nullptr) return g_hookMono();
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch()).count();
}

// "YYYY-MM-DD HH:MM:SS.mmm" (hora local).
std::string fmtWall(std::int64_t ms) {
    if (ms < 0) ms = 0;
    const std::time_t secs = static_cast<std::time_t>(ms / 1000);
    const int rem = static_cast<int>(ms % 1000);
    std::tm t{};
#if defined(_WIN32)
    localtime_s(&t, &secs);
#else
    localtime_r(&secs, &t);
#endif
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d.%03d",
                  t.tm_year + 1900, t.tm_mon + 1, t.tm_mday,
                  t.tm_hour, t.tm_min, t.tm_sec, rem);
    return std::string(buf);
}

// "YYYYMMDD_HHMMSS" (hora local) -- para nomes de arquivo e SessionId.
std::string fmtCompact(std::int64_t ms) {
    if (ms < 0) ms = 0;
    const std::time_t secs = static_cast<std::time_t>(ms / 1000);
    std::tm t{};
#if defined(_WIN32)
    localtime_s(&t, &secs);
#else
    localtime_r(&secs, &t);
#endif
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d%02d%02d_%02d%02d%02d",
                  t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, t.tm_hour, t.tm_min, t.tm_sec);
    return std::string(buf);
}

// "HH:MM:SS.mmm" -- sem estouro em sessoes longas (horas acumulam no primeiro campo).
std::string fmtElapsed(std::int64_t ms) {
    if (ms < 0) ms = 0;
    const long long h = static_cast<long long>(ms / 3600000); ms %= 3600000;
    const long long m = static_cast<long long>(ms / 60000);   ms %= 60000;
    const long long s = static_cast<long long>(ms / 1000);
    const long long k = static_cast<long long>(ms % 1000);
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%02lld:%02lld:%02lld.%03lld", h, m, s, k);
    return std::string(buf);
}

// ------------------------------- processo -------------------------------
long long currentPid() {
#if defined(_WIN32)
    return static_cast<long long>(::GetCurrentProcessId());
#else
    return static_cast<long long>(::getpid());
#endif
}

std::string executablePath() {
#if defined(_WIN32)
    char buf[1024] = {0};
    const DWORD n = ::GetModuleFileNameA(nullptr, buf, static_cast<DWORD>(sizeof(buf)));
    if (n > 0 && n < sizeof(buf)) return std::string(buf, n);
    return "unknown";
#else
    std::error_code ec;
    const fs::path p = fs::read_symlink("/proc/self/exe", ec);
    if (!ec) return p.string();
    return "unknown";
#endif
}

// Diretorio base. Preserva EXATAMENTE a resolucao anterior (%APPDATA%\AAMP): quando APPDATA
// nao existe -- caso do ambiente headless de testes -- o logging fica inativo, como antes.
std::string baseDir() {
    if (!g_hookDir.empty()) return g_hookDir;
#if defined(_WIN32)
    char* base = nullptr; std::size_t len = 0;
    if (_dupenv_s(&base, &len, "APPDATA") != 0 || base == nullptr) return std::string();
    const std::string dir = std::string(base) + "\\AAMP";
    std::free(base);
    return dir;
#else
    const char* base = std::getenv("APPDATA");
    if (base == nullptr) return std::string();
    return std::string(base) + "/AAMP";
#endif
}

// Nome de arquivo para arquivar a sessao ANTERIOR. Le o cabecalho que nos mesmos escrevemos
// (Start/PID) para que o arquivo mantenha a identidade da sessao a que pertence. Se o cabecalho
// nao existir (log legado, anterior a esta infraestrutura) usa um nome explicitamente marcado
// como "legacy" -- nunca atribui a sessao antiga a identidade da nova.
std::string archiveNameFor(const fs::path& existing) {
    std::ifstream in(existing);
    std::string line, stamp, pid;
    for (int n = 0; n < 40 && std::getline(in, line); ++n) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (stamp.empty() && line.rfind("Start: ", 0) == 0) {
            const std::string v = line.substr(7);
            if (v.size() >= 19)
                stamp = v.substr(0, 4) + v.substr(5, 2) + v.substr(8, 2) + "_"
                      + v.substr(11, 2) + v.substr(14, 2) + v.substr(17, 2);
        } else if (pid.empty() && line.rfind("PID: ", 0) == 0) {
            pid = line.substr(5);
            while (!pid.empty() && (pid.front() == ' ')) pid.erase(pid.begin());
            while (!pid.empty() && (pid.back() == ' ')) pid.pop_back();
        }
        if (!stamp.empty() && !pid.empty()) break;
    }
    if (!stamp.empty() && !pid.empty()) return "diag_" + stamp + "_" + pid + ".txt";
    return "diag_legacy_" + fmtCompact(wallEpochMs()) + "_" + std::to_string(currentPid()) + ".txt";
}

// Arquiva o log anterior. Devolve "" em sucesso (ou quando nao havia nada a arquivar) e uma
// descricao do erro quando o arquivo anterior NAO pode ser movido -- caso em que ele permanece
// intacto no disco e a nova sessao usa um nome alternativo.
std::string archivePrevious(const fs::path& live, bool& outUseAlternate) {
    outUseAlternate = false;
    std::error_code ec;
    if (!fs::exists(live, ec)) return std::string();

    const fs::path logs = live.parent_path() / "logs";
    fs::create_directories(logs, ec);

    const std::string name = archiveNameFor(live);
    fs::path target = logs / name;
    for (int n = 1; n < 1000 && fs::exists(target, ec); ++n) {
        const std::string stem = name.substr(0, name.size() - 4);   // sem ".txt"
        target = logs / (stem + "_" + std::to_string(n) + ".txt");
    }

    std::error_code renameEc;
    fs::rename(live, target, renameEc);
    if (!renameEc) return std::string();

    // rename falhou (p.ex. volumes distintos, ou arquivo em uso): tenta copiar e so remove
    // o original se a copia for integralmente bem-sucedida.
    std::error_code copyEc;
    fs::copy_file(live, target, fs::copy_options::overwrite_existing, copyEc);
    if (!copyEc) {
        std::error_code rmEc;
        fs::remove(live, rmEc);
        if (!rmEc) return std::string();
    }

    // Nao foi possivel arquivar. O log anterior permanece intacto; a nova sessao vai para
    // um arquivo alternativo para nao se misturar a ele.
    outUseAlternate = true;
    return "ArchiveError: " + renameEc.message()
         + " (log anterior PRESERVADO em " + live.string() + ")";
}

const char* kRule = "================================================================================";

// Abre a sessao. Chamador ja detem g_mx.
void ensureBegunLocked() {
    if (g_begun) return;
    g_begun = true;   // marcado antes de qualquer I/O: uma falha nao pode gerar retentativas

    const std::string dir = baseDir();
    const std::int64_t wall = wallEpochMs();
    g_t0Mono   = monotonicMs();
    g_sessionId = fmtCompact(wall) + "-" + std::to_string(currentPid())
                + "-" + std::to_string(g_sessionSeq.fetch_add(1) + 1);

    if (dir.empty()) return;   // logging indisponivel (comportamento anterior preservado)

    std::error_code ec;
    fs::create_directories(dir, ec);

    const fs::path live = fs::path(dir) / "diag_log.txt";
    bool useAlternate = false;
    const std::string archiveNote = archivePrevious(live, useAlternate);

    fs::path target = live;
    if (useAlternate)
        target = fs::path(dir) / ("diag_log_" + fmtCompact(wall) + "_"
                                  + std::to_string(currentPid()) + ".txt");

    g_out.open(target, std::ios::app);
    if (!g_out.is_open()) return;
    g_path = target.string();

    g_out << kRule << "\n"
          << "AAMP DIAGNOSTIC SESSION BEGIN" << "\n"
          << "SessionId: "      << g_sessionId              << "\n"
          << "Start: "          << fmtWall(wall)            << "\n"
          << "PID: "            << currentPid()             << "\n"
          << "ExecutablePath: " << executablePath()         << "\n"
          << "Version: "        << AAMP_BUILD_VERSION       << "\n"
          << "BuildConfig: "    << AAMP_BUILD_CONFIG        << "\n"
          << "BuildTime: "      << AAMP_BUILD_TIME          << "\n"
          << "CommitFull: "     << AAMP_BUILD_COMMIT_FULL   << "\n"
          << "CommitShort: "    << AAMP_BUILD_COMMIT_SHORT  << "\n"
          << "Branch: "         << AAMP_BUILD_BRANCH        << "\n"
          << "GitDirty: "       << AAMP_BUILD_GIT_DIRTY     << "\n";
    if (!archiveNote.empty()) g_out << archiveNote << "\n";
    g_out << kRule << "\n";
    g_out.flush();
}

} // namespace

// ------------------------------- API -------------------------------
void begin() {
    std::lock_guard<std::mutex> lk(g_mx);
    ensureBegunLocked();
}

void write(const std::string& line) {
    std::lock_guard<std::mutex> lk(g_mx);
    ensureBegunLocked();                       // nunca produz linha orfa, sem cabecalho
    if (!g_out.is_open()) return;
    // Uma unica insercao no stream, sob o mutex: linhas de threads distintas nao intercalam.
    g_out << fmtWall(wallEpochMs()) << " [T+" << fmtElapsed(monotonicMs() - g_t0Mono) << "] "
          << line << "\n";
    g_out.flush();                             // o rodape ausente e evidencia de interrupcao:
                                               // as linhas ja emitidas precisam estar em disco
}

void end(const std::string& exitReason) {
    std::lock_guard<std::mutex> lk(g_mx);
    if (!g_begun || g_ended) return;           // idempotente; nunca duplica o rodape
    g_ended = true;
    if (!g_out.is_open()) return;
    const std::int64_t wall = wallEpochMs();
    g_out << kRule << "\n"
          << "AAMP DIAGNOSTIC SESSION END" << "\n"
          << "End: "     << fmtWall(wall)                          << "\n"
          << "Elapsed: " << fmtElapsed(monotonicMs() - g_t0Mono)   << "\n"
          << "Exit: "    << exitReason                             << "\n"
          << kRule << "\n";
    g_out.flush();
}

std::string currentPath() {
    std::lock_guard<std::mutex> lk(g_mx);
    return g_path;
}

std::string currentSessionId() {
    std::lock_guard<std::mutex> lk(g_mx);
    return g_sessionId;
}

namespace test_detail {

void installHooks(const Hooks& h) {
    std::lock_guard<std::mutex> lk(g_mx);
    g_hookDir  = h.baseDir;
    g_hookWall = h.wallEpochMs;
    g_hookMono = h.monotonicMs;
}

void clearHooks() {
    std::lock_guard<std::mutex> lk(g_mx);
    g_hookDir.clear();
    g_hookWall = nullptr;
    g_hookMono = nullptr;
}

void closeSessionForTest() {
    std::lock_guard<std::mutex> lk(g_mx);
    if (g_out.is_open()) g_out.close();
    g_begun = false;
    g_ended = false;
    g_path.clear();
    g_sessionId.clear();
    g_t0Mono = 0;
}

} // namespace test_detail

} // namespace diag
} // namespace aamp
