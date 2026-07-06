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
AampWebBridge::AampWebBridge() {
    using WBC = juce::WebBrowserComponent;

    m_hw = std::make_unique<HardwareManager>(std::make_shared<JuceDeviceEnumerator>());
    m_dm.initialise(2, 2, nullptr, true);                 // prepara o manager/tipos (fiel ao MainComponent)

    m_web.reset(new WBC(
        WBC::Options{}
            .withBackend(WBC::Options::Backend::webview2)         // Edge/Chromium (nao IE); requer NEEDS_WEBVIEW2
            .withNativeIntegrationEnabled()                       // expoe window.__JUCE__.backend ao HTML
            // Canal de comandos JS->C++: o frontend chama window.__JUCE__.backend.emitEvent("aampCommand", "<json>")
            // (API documentada, mesmo objeto backend usado para receber eventos). getNativeFunction NAO
            // esta disponivel sem carregar o modulo JS do JUCE; por isso usamos o event listener.
            .withEventListener("aampCommand",
                [this](const juce::var& payload) { onCommand(payload.toString()); })
            .withNativeFunction("aampCommand",                    // mantido (funciona se o modulo JS do JUCE for carregado)
                [this](const juce::Array<juce::var>& args,
                       WBC::NativeFunctionCompletion complete) {
                    if (args.size() > 0) onCommand(args[0].toString());
                    complete(juce::var());                        // ack
                })
        // >>> DEV: SEM withResourceProvider / SEM BinaryData <<<
        ));

    addAndMakeVisible(*m_web);

    // DEV: carrega o HTML de arquivo externo (edicao sem recompilar).
    const juce::File html = resolveHtmlFile();
    if (html.existsAsFile()) {
        m_web->goToURL(juce::URL(html).toString(false));          // file:///.../aamp_laboratory.html
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
void AampWebBridge::emitEvent(const juce::String& evt, juce::var data) {
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
    const auto v    = juce::JSON::parse(json);
    const auto cmd  = v.getProperty("cmd", juce::var()).toString();
    const auto args = v.getProperty("args", juce::var());
    if      (cmd == "enumerate") doEnumerate();
    else if (cmd == "prepare")   doPrepare(args);
    else if (cmd == "start")     doStart();
    else if (cmd == "cancel")    doCancel();
    else if (cmd == "export")    doExport(args);
    else if (cmd == "diag")      diagLog("[JS] " + args.getProperty("msg", juce::var()).toString()); // INSTRUMENTACAO TEMPORARIA
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
    using namespace juce_hw_detail;
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
    m_dm.setCurrentAudioDeviceType(typeName, true);
    juce::AudioIODeviceType* devType = findType(m_dm, typeName);
    juce::AudioDeviceManager::AudioDeviceSetup s;
    s.outputDeviceName = devName;
    s.inputDeviceName  = isAsio ? devName
                                : (devType != nullptr ? bestInputFor(devType, devName) : devName);
    s.useDefaultInputChannels = true; s.useDefaultOutputChannels = true;
    m_dm.setAudioDeviceSetup(s, true);
    juce::AudioIODevice* dev = m_dm.getCurrentAudioDevice();
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
            const double tSer0 = juce::Time::getMillisecondCounterHiRes();    // INSTRUMENTACAO TEMPORARIA
            juce::var payload = serializeSessionVar(r.bundle);    // -> fromEngineSession()
            const double tSer1 = juce::Time::getMillisecondCounterHiRes();    // INSTRUMENTACAO TEMPORARIA
            diagLog("[TIMER] serialize(C++)=" + juce::String(tSer1 - tSer0, 0) + "ms  -> emitindo runFinished");
            auto* o = new juce::DynamicObject();
            o->setProperty("outcome", "Completed");
            o->setProperty("payload", payload);
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
