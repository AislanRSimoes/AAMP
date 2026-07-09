// AAMP - src/bridge/AampWebBridge.cpp
// SCAFFOLDING DE MAQUINA-ALVO (requer JUCE 8; NAO compilado/verificado neste ambiente headless).
//
// Unica diferenca frente a versao com BinaryData: a ORIGEM do HTML. Aqui o WebBrowserComponent
// carrega um arquivo EXTERNO (dev), permitindo editar HTML/CSS/JS sem recompilar. Toda a
// arquitetura in-process e o contrato de comandos/eventos permanecem inalterados.
#if defined(AAMP_WITH_JUCE)
#include "AampWebBridge.h"
#include "SessionSerializer.h"                 // ResultBundle -> Session JSON (inalterado)
#include "../hardware/JuceDeviceBackend.h"     // JuceDeviceEnumerator, JuceAudioDevice (L-04)
#include "../common/Constants.h"               // REF_SAMPLE_RATE
#include "../export/ExportEngine.h"            // M-07 (DOC-10): CSV/WAV/PDF/PNG
#include "../export/render/JuceRenderWriters.h"// JucePdfWriter/JucePngWriter (maquina-alvo)
#include <algorithm>                           // std::find_if

namespace aamp {

// ===== INSTRUMENTACAO TEMPORARIA — BOOT-01 (remover apos diagnostico) =====
// Grava no MESMO diag_log.txt do Main.cpp, sem depender do diagLog() estatico (outro TU).
static void bridgeDiag(const juce::String& line) {
    juce::File f = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                       .getChildFile("AAMP").getChildFile("diag_log.txt");
    f.getParentDirectory().createDirectory();
    f.appendText(line + "\n");
}
static juce::String bootTs() { return juce::Time::getCurrentTime().toString(true, true, true, true); }
static double s_bootT0        = 0.0;     // instante (ms hi-res) do inicio do ctor; base dos deltas
static int    s_htmlLoadCount = 0;       // nº de navegacoes (goToURL) no bootstrap
static bool   s_firstCbLogged = false;   // guarda p/ registrar o 1º callback do WebView uma vez
static void bootMark(const juce::String& label) {
    const double t = juce::Time::getMillisecondCounterHiRes();
    bridgeDiag("[BOOT-01] " + label + " | wall=" + bootTs()
        + " | t=+" + juce::String(s_bootT0 > 0.0 ? t - s_bootT0 : 0.0, 1) + " ms");
}
// ===== FIM INSTRUMENTACAO TEMPORARIA =====

// ------------------------- helpers de device (PORTADOS de Main.cpp, a implementacao de
// referencia) -------------------------
// O bridge referenciava juce_hw_detail::findType/bestInputFor, mas esse namespace nao existe no
// codigo (era suposicao de scaffolding, nunca compilada). Definimos os helpers localmente, com a
// MESMA logica de Main.cpp (cleanDeviceName/bestInputFor), para o TU ser autossuficiente.
static juce::String bridgeCleanDeviceName(const juce::String& raw) {
    juce::String s = raw.trim();
    if (s.startsWithIgnoreCase("ASIO ")) s = s.substring(5).trim();
    int d = 0;
    while (d < s.length() && s[d] >= '0' && s[d] <= '9') ++d;
    if (d > 0 && d < s.length() && s[d] == '-') s = s.substring(d + 1).trim();
    const int a = s.indexOfChar('('), b = s.lastIndexOfChar(')');
    if (a >= 0 && b > a) s = s.substring(a + 1, b).trim();
    return s;
}
// Resolve o AudioIODeviceType pelo nome do backend (fiel aos loops de Main.cpp).
static juce::AudioIODeviceType* findType(juce::AudioDeviceManager& dm, const juce::String& typeName) {
    for (auto* t : dm.getAvailableDeviceTypes())
        if (t != nullptr && t->getTypeName() == typeName) return t;
    return nullptr;
}
// Melhor entrada para um dado tipo + nome de saida (versao 2-args do bridge; logica de Main.cpp:
// casa por nome "limpo", senao usa a primeira entrada). So chamado para nao-ASIO.
static juce::String bestInputFor(juce::AudioIODeviceType* type, const juce::String& outName) {
    if (type == nullptr) return {};
    const auto ins = type->getDeviceNames(true);
    const juce::String key = bridgeCleanDeviceName(outName);
    for (const auto& n : ins)
        if (key.isNotEmpty() && bridgeCleanDeviceName(n).equalsIgnoreCase(key)) return n;
    return ins.size() > 0 ? ins[0] : juce::String();
}

// ------------------------- helpers locais -------------------------
static juce::var stateObj(const char* s) {
    auto* o = new juce::DynamicObject();
    o->setProperty("state", s);
    return juce::var(o);
}
static juce::var progressVar(const Progress& p) {
    auto* o = new juce::DynamicObject();
    o->setProperty("label",     juce::String(p.label.c_str()));
    o->setProperty("percent",   p.percent);
    o->setProperty("completed", p.completed);
    o->setProperty("total",     p.total);
    return juce::var(o);
}
// Serializa DeviceCapabilities (fonte unica de verdade, HardwareManager) -> var read-only p/ o painel
// (DOC-05 §3.2). Campos vazios sao omitidos pela UI (ex.: fabricante/bit depths quando o driver nao expoe).
static juce::var capsVar(const DeviceInfo& d) {
    auto* o = new juce::DynamicObject();
    o->setProperty("backend",        juce::String(d.backend.c_str()));
    o->setProperty("inputChannels",  d.caps.inputChannels);
    o->setProperty("outputChannels", d.caps.outputChannels);
    juce::Array<juce::var> sr; for (double v : d.caps.sampleRates) sr.add(v);
    juce::Array<juce::var> bs; for (int v : d.caps.bufferSizes)    bs.add(v);
    juce::Array<juce::var> bd; for (int v : d.caps.bitDepths)      bd.add(v);
    o->setProperty("sampleRates", sr);
    o->setProperty("bufferSizes", bs);
    o->setProperty("bitDepths",   bd);
    if (!d.caps.manufacturer.empty())
        o->setProperty("manufacturer", juce::String(d.caps.manufacturer.c_str()));
    return juce::var(o);
}

// ------------------------- ciclo de vida -------------------------
// BOOT-01 (TEMPORARIO): subclasse so para instrumentar o callback NATIVO de conclusao de navegacao
// do JUCE/WebView2. NAO sobrescreve pageAboutToLoad (default = permite tudo) -> navegacao inalterada.
struct Boot01WebView : public juce::WebBrowserComponent {
    explicit Boot01WebView(const Options& o) : juce::WebBrowserComponent(o) {}
    void pageFinishedLoading(const juce::String& url) override {
        bootMark("5N navegacao-concluida (pageFinishedLoading, NATIVO) url=" + url);
    }
};

AampWebBridge::AampWebBridge() {
    using WBC = juce::WebBrowserComponent;
    s_bootT0 = juce::Time::getMillisecondCounterHiRes();
    bootMark("1 ctor-inicio");                                        // ponto 1

    m_hw = std::make_unique<HardwareManager>(std::make_shared<JuceDeviceEnumerator>());
    m_dm.initialise(2, 2, nullptr, true);   // (custo aparece no delta 1->2; abertura ASIO e sincrona)

    bootMark("2 antes-de-criar-WebBrowserComponent");                 // ponto 2
    m_web.reset(new Boot01WebView(                                    // subclasse instrumentada (ponto 5N)
        WBC::Options{}
            .withBackend(WBC::Options::Backend::webview2)         // Edge/Chromium (nao IE); requer NEEDS_WEBVIEW2
            .withNativeIntegrationEnabled()                       // expoe window.__JUCE__.backend ao HTML
            .withEventListener("aampCommand",
                [this](const juce::var& payload) { onCommand(payload.toString()); })
            .withNativeFunction("aampCommand",
                [this](const juce::Array<juce::var>& args,
                       WBC::NativeFunctionCompletion complete) {
                    if (args.size() > 0) onCommand(args[0].toString());
                    complete(juce::var());                        // ack
                })
        // >>> DEV: SEM withResourceProvider / SEM BinaryData <<<
        ));
    bootMark("3 apos-criar-WebBrowserComponent");                     // ponto 3

    addAndMakeVisible(*m_web);

    // DEV: carrega o HTML de arquivo externo (edicao sem recompilar).
    const juce::File html = resolveHtmlFile();
    // BOOT-01: resultado COMPLETO de resolveHtmlFile() -> elimina duvida sobre QUAL HTML carrega.
    bridgeDiag("[BOOT-01] resolveHtmlFile"
        + juce::String(" | path=") + html.getFullPathName()
        + " | exists=" + juce::String(html.existsAsFile() ? "SIM" : "NAO")
        + " | mtime=" + (html.existsAsFile() ? html.getLastModificationTime().toString(true, true) : juce::String("-"))
        + " | size=" + (html.existsAsFile() ? juce::String(html.getSize()) + " bytes" : juce::String("-"))
        + " | wall=" + bootTs());
    ++s_htmlLoadCount;
    bootMark("4 antes-do-goToURL #" + juce::String(s_htmlLoadCount)   // ponto 4
        + (html.existsAsFile() ? " (arquivo-externo)" : " (FALLBACK-data-url)"));
    if (html.existsAsFile()) {
        m_web->goToURL(juce::URL(html).toString(false));              // file:///.../aamp_laboratory.html
    } else {
        m_web->goToURL("data:text/html,"
            "<body style='background:%230b0f13;color:%238a9ba6;font-family:sans-serif;"
            "display:flex;align-items:center;justify-content:center;height:100vh'>"
            "<div>aamp_laboratory.html nao encontrado.<br>Esperado em "
            "&lt;exe&gt;/resources/ ou C:\\aamp\\resources\\</div></body>");
    }
}

AampWebBridge::~AampWebBridge() {
    m_abort.store(true);                                 // pede aborto da medicao (JuceCaptureDevice)
    if (m_worker.joinable())       m_worker.join();      // aguarda o fim da medicao (sem detach)
    if (m_exportWorker.joinable()) m_exportWorker.join();// aguarda o fim de qualquer export em curso
    m_dm.closeAudioDevice();                             // libera o ASIO antes de destruir o manager
}

void AampWebBridge::resized() {
    if (m_web) m_web->setBounds(getLocalBounds());
}

// DEV: 1) <exe>/resources/aamp_laboratory.html  2) C:\aamp\resources\aamp_laboratory.html
juce::File AampWebBridge::resolveHtmlFile() {
    const auto exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                            .getParentDirectory();
    const juce::File rel = exeDir.getChildFile("resources")
                                 .getChildFile("aamp_laboratory.html");
    if (rel.existsAsFile()) return rel;
    return juce::File("C:\\aamp\\resources\\aamp_laboratory.html");
}

// ------------------------- emit ao JS -------------------------
void AampWebBridge::emitEvent(const juce::String& evt, const juce::var& data) {
    auto* o = new juce::DynamicObject();
    o->setProperty("evt", evt);
    o->setProperty("data", data);
    juce::var wrapped(o);
    // SafePointer: se a Bridge (juce::Component) for destruida enquanto um evento ainda esta
    // enfileirado na Message Thread, o lambda vira no-op (evita use-after-free em m_web).
    juce::Component::SafePointer<AampWebBridge> self(this);
    juce::MessageManager::callAsync([self, wrapped] {
        if (auto* p = self.getComponent())
            if (p->m_web) p->m_web->emitEventIfBrowserIsVisible("aampEvent", wrapped);
    });
}

// ------------------------- roteamento de comandos -------------------------
void AampWebBridge::onCommand(const juce::String& json) {
    if (!s_firstCbLogged) { s_firstCbLogged = true; bootMark("5 primeiro-comando-JS (onCommand -- NAO e fim de navegacao; ver 5N)"); } // ponto 5
    const auto v    = juce::JSON::parse(json);
    const auto cmd  = v.getProperty("cmd", juce::var()).toString();
    const auto args = v.getProperty("args", juce::var());
    if      (cmd == "enumerate") doEnumerate();
    else if (cmd == "prepare")   doPrepare(args);
    else if (cmd == "start")     doStart();
    else if (cmd == "cancel")    doCancel();
    else if (cmd == "export")    doExport(args);
    else if (cmd == "diag")      diagLog("[JS] " + args.getProperty("msg", juce::var()).toString()); // INSTRUMENTACAO TEMPORARIA
    else if (cmd == "fperf")     bridgeDiag("[FPERF] " + args.getProperty("msg", juce::var()).toString()); // INSTRUMENTACAO TEMPORARIA
    else if (cmd == "boot01")    bridgeDiag("[BOOT-01/JS] " + args.getProperty("msg", juce::var()).toString()); // BOOT-01 (marcas do HTML)
}

void AampWebBridge::doEnumerate() {
    m_devices = m_hw->enumerateDevices();                          // M-01: so dispositivos do OS
    juce::Array<juce::var> arr;
    for (const auto& d : m_devices) {
        auto* o = new juce::DynamicObject();
        o->setProperty("id",      juce::String(d.id.c_str()));
        o->setProperty("name",    juce::String(d.name.c_str()));
        o->setProperty("backend", juce::String(d.backend.c_str())); // DOC-05 §3.2
        o->setProperty("in",      d.caps.inputChannels);
        o->setProperty("out",     d.caps.outputChannels);
        arr.add(juce::var(o));
    }
    auto* o = new juce::DynamicObject();
    o->setProperty("devices", arr);
    emitEvent("devices", juce::var(o));
}

void AampWebBridge::doPrepare(juce::var args) {
    const std::string id = args.getProperty("deviceId", juce::var()).toString().toStdString();
    const std::string modeStr = args.getProperty("mode", juce::var("Mono")).toString().toStdString();
    const bool stereo  = (modeStr == "Stereo");
    const int  inBase  = (int) args.getProperty("inBase",  juce::var(0));
    const int  outBase = (int) args.getProperty("outBase", juce::var(0));

    // Localiza o device escolhido pelo MESMO id enviado em doEnumerate (DeviceInfo::id == nome real).
    if (m_devices.empty()) m_devices = m_hw->enumerateDevices();
    const DeviceInfo* chosen = nullptr;
    if (!id.empty()) {
        auto it = std::find_if(m_devices.begin(), m_devices.end(),
                               [&](const DeviceInfo& d) { return d.id == id; });
        if (it != m_devices.end()) chosen = &(*it);
    }
    if (chosen == nullptr && !m_devices.empty()) chosen = &m_devices.front();

    auto emitError = [&](const char* msg) {
        m_deviceReady = false; m_selectedDeviceName.clear();
        auto* val = new juce::DynamicObject(); val->setProperty("ok", false);
        val->setProperty("message", juce::String(msg));
        auto* o = new juce::DynamicObject();
        o->setProperty("state", "Error"); o->setProperty("validation", juce::var(val));
        emitEvent("prepared", juce::var(o));
    };
    if (chosen == nullptr) { emitError("Nenhum dispositivo de aquisicao conectado."); return; }

    // ===== Configura o AudioDeviceManager PERSISTENTE — MESMA logica do AudioConfig (Main.cpp) =====
    // (prepare() = preparacao do hardware: abre o device escolhido e aplica modo/canais; sem orchestrator.)
    const juce::String typeName(chosen->backend.c_str());
    const juce::String devName(chosen->id.c_str());
    const bool isAsio = typeName.containsIgnoreCase("asio");

    // Se o MESMO dispositivo/tipo ja esta aberto (alternancia Mono<->Estereo ou re-selecao do
    // device vigente), pula a 1a abertura — feita SO para ler nIn/nOut. O device ja aberto expoe
    // os nomes de canais (independente do bitmask ativo), entao a contagem sai dele diretamente.
    // Reduz de 2 aberturas para 1 no caso repetido (abrir ASIO e caro). Se algo divergir, o
    // fallback e a abertura completa: a otimizacao nunca quebra o resultado final.
    juce::AudioIODevice* dev = m_dm.getCurrentAudioDevice();
    const bool sameOpen = m_deviceReady && dev != nullptr
        && dev->getName() == devName
        && (typeName.isEmpty() || m_dm.getCurrentAudioDeviceType() == typeName);
    if (!sameOpen) {
        m_dm.setCurrentAudioDeviceType(typeName, true);
        juce::AudioIODeviceType* devType = findType(m_dm, typeName);
        juce::AudioDeviceManager::AudioDeviceSetup s;
        s.outputDeviceName = devName;
        s.inputDeviceName  = isAsio ? devName
                                    : (devType != nullptr ? bestInputFor(devType, devName) : devName);
        s.useDefaultInputChannels = true; s.useDefaultOutputChannels = true;
        m_dm.setAudioDeviceSetup(s, true);                 // 1a abertura: SO quando o device muda
        dev = m_dm.getCurrentAudioDevice();
    }
    if (dev == nullptr) { emitError("Nao foi possivel abrir o dispositivo selecionado."); return; }

    const int nIn  = dev->getInputChannelNames().size();
    const int nOut = dev->getOutputChannelNames().size();

    // Bitmask de canais (applyChannelBitmask): Entrada<-nOut/outBase, Saida<-nIn/inBase; pares em estereo.
    juce::AudioDeviceManager::AudioDeviceSetup cs = m_dm.getAudioDeviceSetup();
    cs.useDefaultInputChannels = false; cs.useDefaultOutputChannels = false;
    cs.inputChannels.clear(); cs.outputChannels.clear();
    const int ib = juce::jlimit(0, juce::jmax(0, nIn  - 1), inBase);
    const int ob = juce::jlimit(0, juce::jmax(0, nOut - 1), outBase);
    cs.inputChannels.setBit(ib);
    cs.outputChannels.setBit(ob);
    if (stereo && nIn >= 2 && nOut >= 2) { cs.inputChannels.setBit(ib + 1); cs.outputChannels.setBit(ob + 1); }
    cs.sampleRate = k::REF_SAMPLE_RATE;                  // DOC-03: fs fixo (48000)
    m_dm.setAudioDeviceSetup(cs, true);

    juce::AudioIODevice* dev2 = m_dm.getCurrentAudioDevice();
    m_deviceReady = (dev2 != nullptr);                  // BR-003: device aberto e validado (READY)
    m_prepFs = (dev2 && dev2->getCurrentSampleRate() > 0.0) ? dev2->getCurrentSampleRate() : k::REF_SAMPLE_RATE;
    m_selectedDeviceName = chosen->name;

    // Capacidades do device ABERTO -> painel (DOC-05 §3.2) e dropdowns Entrada/Saida (nIn/nOut).
    DeviceInfo full = *chosen;
    full.caps.inputChannels = nIn; full.caps.outputChannels = nOut;
    full.caps.sampleRates.clear(); full.caps.bufferSizes.clear();
    if (dev2) {
        for (double sr : dev2->getAvailableSampleRates()) full.caps.sampleRates.push_back(sr);
        for (int bs : dev2->getAvailableBufferSizes())    full.caps.bufferSizes.push_back(bs);
    }
    // Latencia REAL reportada pelo device aberto (round-trip); parcial se o driver nao reporta.
    double latencyMs = 0.0; bool latencyPartial = true;
    if (dev2) {
        const int rt = dev2->getOutputLatencyInSamples() + dev2->getInputLatencyInSamples();
        if (rt > 0 && m_prepFs > 0.0) { latencyMs = rt * 1000.0 / m_prepFs; latencyPartial = false; }
    }
    const auto& srs = full.caps.sampleRates;
    const bool rateSupported = srs.empty()
        || std::find(srs.begin(), srs.end(), k::REF_SAMPLE_RATE) != srs.end();

    auto* val = new juce::DynamicObject();
    val->setProperty("ok",             m_deviceReady);
    val->setProperty("usableChannels", (nIn > 0 && nOut > 0));   // ER-010
    val->setProperty("rateSupported",  rateSupported);           // BR-002 (operacional 48 kHz)
    val->setProperty("latencyMs",      latencyMs);               // FR-008 (real)
    val->setProperty("latencyPartial", latencyPartial);
    auto* op = new juce::DynamicObject();
    op->setProperty("sampleRate", k::REF_SAMPLE_RATE);           // DOC-03: fs fixo
    auto* o = new juce::DynamicObject();
    o->setProperty("state",      m_deviceReady ? "Ready" : "Error");
    o->setProperty("caps",       capsVar(full));
    o->setProperty("operating",  juce::var(op));
    o->setProperty("validation", juce::var(val));
    emitEvent("prepared", juce::var(o));
}

// Liga os callbacks do orchestrator (local da worker) aos eventos do frontend (via emitEvent).
void AampWebBridge::wireCallbacks(MeasurementOrchestrator& orch) {
    orch.setProgressCallback([this](const Progress& p) {
        emitEvent("progress", progressVar(p));
        if (p.label.rfind("Processando", 0) == 0) emitEvent("state", stateObj("Processing"));
    });
    orch.setResultCallback([this](const MetricResult& m, const MetricEmissionInfo&) {
        auto* o = new juce::DynamicObject();
        o->setProperty("id",   juce::String(m.id.c_str()));
        o->setProperty("name", juce::String(m.name.c_str()));
        emitEvent("metricReady", juce::var(o));
    });
    orch.setStimulusCallback([this](const StimulusReport& s) {
        auto* o = new juce::DynamicObject();
        o->setProperty("index", s.index - 1);                     // frontend usa 0-based
        o->setProperty("total", s.total);
        o->setProperty("ok",    !s.clipped && !s.nonDecayed);
        emitEvent("stimulusIntegrity", juce::var(o));
    });
}

// start() = medicao REAL. Worker fiel ao MainComponent::runMeasurement: JuceCaptureDevice(m_dm,fs,&abort)
// + MeasurementOrchestrator(fs, Mono) -> prepare -> runSuite. O StimulusGenerator (M-03) e reproduzido
// e capturado atraves do device configurado no prepare(). Orchestrator LOCAL (nao membro).
void AampWebBridge::doStart() {
    if (m_running.exchange(true)) return;
    if (!m_deviceReady || m_dm.getCurrentAudioDevice() == nullptr) {
        m_running = false;
        auto* o = new juce::DynamicObject();
        o->setProperty("outcome", "Aborted");
        o->setProperty("message", "Nenhum dispositivo aberto. Configure o audio.");
        emitEvent("runFinished", juce::var(o));
        return;
    }
    if (m_worker.joinable()) m_worker.join();   // encerra worker anterior antes de reusar (sem detach)
    m_haveBundle = false;
    m_abort.store(false);
    { auto su = m_dm.getAudioDeviceSetup(); su.sampleRate = k::REF_SAMPLE_RATE; m_dm.setAudioDeviceSetup(su, true); }
    emitEvent("state", stateObj("Measuring"));
    const double fs = m_prepFs;
    m_worker = std::thread([this, fs] {
        const double tW0 = juce::Time::getMillisecondCounterHiRes();          // INSTRUMENTACAO TEMPORARIA
        JuceCaptureDevice realDev(m_dm, fs, &m_abort);            // bloco REAL de play+capture
        MeasurementOrchestrator orch(fs, ChannelMode::Mono);     // FIEL ao MainComponent (Mono)
        wireCallbacks(orch);
        RunResult r;
        const bool prepOk = orch.prepare(realDev);
        const double tPrep = juce::Time::getMillisecondCounterHiRes();        // INSTRUMENTACAO TEMPORARIA
        if (prepOk) r = orch.runSuite(realDev);                  // M-08 conduz a suite (M-03 gera)
        else { r.outcome = RunOutcome::Aborted; r.userMessage = "Dispositivo nao esta pronto."; }
        const double tRun = juce::Time::getMillisecondCounterHiRes();         // INSTRUMENTACAO TEMPORARIA
        diagLog("[TIMER] prepare=" + juce::String(tPrep - tW0, 0)
                + "ms  runSuite(captura+analise)=" + juce::String(tRun - tPrep, 0) + "ms");

        if (r.outcome == RunOutcome::Completed) {
            if (!m_selectedDeviceName.empty()) r.bundle.meta.deviceName = m_selectedDeviceName;
            if (r.bundle.meta.timestamp.empty())
                r.bundle.meta.timestamp =
                    juce::Time::getCurrentTime().formatted("%Y-%m-%d %H:%M:%S").toStdString();
            m_lastBundle  = r.bundle;                              // retido p/ ExportEngine (M-07)
            m_haveBundle  = true;
            // ARCH-01: persiste a Session como .aamp CANONICO e a entrega ao frontend como bytes,
            // para reabertura pelo MESMO caminho de fromAamp() (sem payload/ fromEngineSession dedicados).
            const double tSer0 = juce::Time::getMillisecondCounterHiRes();    // INSTRUMENTACAO TEMPORARIA
            const juce::MemoryBlock aamp = serializeAampBytes(r.bundle);      // manifest.json + analysis.json (ZIP STORE)
            const juce::File tmp = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                       .getChildFile("aamp_characterization.aamp");
            tmp.replaceWithData(aamp.getData(), aamp.getSize());             // gravacao automatica do .aamp temporario
            const double tSer1 = juce::Time::getMillisecondCounterHiRes();    // INSTRUMENTACAO TEMPORARIA
            diagLog("[TIMER] serialize(.aamp)=" + juce::String(tSer1 - tSer0, 0) + "ms  bytes="
                    + juce::String((int) aamp.getSize()) + "  path=" + tmp.getFullPathName());
            auto* o = new juce::DynamicObject();
            o->setProperty("outcome",  "Completed");
            o->setProperty("aamp",     juce::Base64::toBase64(aamp.getData(), aamp.getSize())); // bytes do .aamp
            o->setProperty("aampPath", tmp.getFullPathName());
            emitEvent("runFinished", juce::var(o));
        } else {
            auto* o = new juce::DynamicObject();
            o->setProperty("outcome",
                r.outcome == RunOutcome::Cancelled ? "Cancelled" : "Aborted");
            o->setProperty("message", juce::String(r.userMessage.c_str()));
            emitEvent("runFinished", juce::var(o));
        }
        m_running = false;
    });
}

// cancel = aborto REAL: quebra o loop de playAndCapture no JuceCaptureDevice -> runSuite aborta.
void AampWebBridge::doCancel() {
    m_abort.store(true);
}

void AampWebBridge::doExport(juce::var args) {
    // Liga ExportEngine (M-07, DOC-10: CSV/WAV/PDF/PNG) sobre o ResultBundle ativo retido em doStart.
    // I/O em thread propria (nao bloqueia a Message Thread); emitEvent ja re-empacota o resultado.
    const juce::String fmt = args.getProperty("format", juce::var("pdf")).toString().toLowerCase();

    if (!m_haveBundle) {                                  // nada medido ainda: falha amigavel (DOC-07)
        auto* o = new juce::DynamicObject();
        o->setProperty("ok", false);
        o->setProperty("format", fmt);
        o->setProperty("message", "Nenhuma medicao disponivel para exportar.");
        emitEvent("exported", juce::var(o));
        return;
    }

    if (m_exportWorker.joinable()) m_exportWorker.join(); // encerra export anterior antes de reusar
    const ResultBundle bundle = m_lastBundle;             // copia estavel (ER-040: bundle nunca mutado)

    m_exportWorker = std::thread([this, fmt, bundle] {
        ExportEngine engine;
        // Destino: <Documentos>/AAMP/exports, criado se necessario. Nomenclatura DOC-10 §7.
        const juce::File dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                   .getChildFile("AAMP").getChildFile("exports");
        dir.createDirectory();
        const std::string ts = bundle.meta.timestamp;
        auto pathFor = [&](const std::string& tipo, const std::string& ext) {
            return dir.getChildFile(juce::String(ExportEngine::makeFilename(tipo, ext, ts).c_str()))
                       .getFullPathName().toStdString();
        };

        bool ok = false; std::string message, outPath;

        if (fmt == "csv") {
            const std::string p1 = pathFor("scalars", "csv");
            const std::string p2 = pathFor("curves",  "csv");
            const ExportStatus s1 = engine.exportScalarsCSV(bundle, p1);
            const ExportStatus s2 = engine.exportCurvesCSV(bundle, p2);
            ok = s1.ok && s2.ok; outPath = p1;
            if (!ok) message = !s1.ok ? s1.message : s2.message;
        } else if (fmt == "wav") {
            // IR (MS-04): exporta a curva de resposta ao impulso, se presente (DOC-10 §4).
            const MetricResult* ir = bundle.find("MS-04");
            if (ir != nullptr && !ir->curves.empty()) {
                const ExportStatus s = engine.exportWAV(ir->curves.front().y, bundle.meta.sampleRate,
                                                        1, pathFor("ir", "wav"));
                ok = s.ok; outPath = s.path; message = s.message;
            } else {
                message = "Resposta ao impulso indisponivel para exportacao WAV.";
            }
        } else if (fmt == "png") {
            // PNG (DOC-10 §6): renderiza a curva primaria (MS-01), se presente.
            const MetricResult* fr = bundle.find("MS-01");
            if (fr != nullptr && !fr->curves.empty()) {
                JucePngWriter png;
                const std::string p = pathFor("plot", "png");
                ok = png.write(fr->curves.front(), p); outPath = p;
                if (!ok) message = "Falha ao renderizar PNG.";
            } else {
                message = "Curva indisponivel para exportacao PNG.";
            }
        } else { // "pdf" (padrao): relatorio consolidado (6 secoes, DOC-10 §5).
            JucePdfWriter pdf;
            const std::string p = pathFor("report", "pdf");
            const ExportStatus s = engine.exportPDF(bundle, pdf, p);
            ok = s.ok; outPath = s.path.empty() ? p : s.path; message = s.message;
        }

        auto* o = new juce::DynamicObject();
        o->setProperty("ok", ok);
        o->setProperty("format", fmt);
        if (ok) o->setProperty("path", juce::String(outPath.c_str()));
        else    o->setProperty("message", juce::String(message.c_str()));
        emitEvent("exported", juce::var(o));
    });
}

} // namespace aamp
#endif // AAMP_WITH_JUCE
