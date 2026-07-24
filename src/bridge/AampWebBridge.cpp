// AAMP - src/bridge/AampWebBridge.cpp
// Host nativo único do frontend oficial (resources/aamp_laboratory.html). Ver contrato no .h.
#if defined(AAMP_WITH_JUCE)
#include "AampWebBridge.h"
#include "SessionSerializer.h"                 // ResultBundle -> .aamp canônico (ARCH-01)
#include "../hardware/JuceDeviceBackend.h"     // JuceDeviceEnumerator (M-01/L-04)
#include "../common/Constants.h"               // REF_SAMPLE_RATE
#include "../export/ExportEngine.h"            // M-07 (DOC-10): CSV/WAV/PDF/PNG
#include "../export/render/JuceRenderWriters.h"// JucePdfWriter/JucePngWriter (máquina-alvo)
#include "../evidence/EvidenceRecorder.h"      // Fase 1: observação da aquisição -> EvidencePackage
#include "../evidence/EvidenceSerializer.h"    // Fase 1: EvidencePackage -> .aamp de evidência
#include <algorithm>                           // std::find_if

namespace aamp {

// diagLog: canal único de diagnóstico (diag_log.txt), fornecido por JuceCaptureDevice.h
// (capture_detail::diagLog, importado no namespace aamp). Recebe 'diag' e 'fperf' do frontend.

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
// Serializa DeviceCapabilities (fonte única de verdade, HardwareManager) -> var read-only p/ o painel
// (DOC-05 §3.2). Campos vazios são omitidos pela UI (ex.: fabricante/bit depths quando o driver não expõe).
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
    m_dm.initialise(2, 2, nullptr, true);                 // prepara o manager/tipos

    // Persistência do produto: <userAppData>/AAMP/settings.xml (idioma + hardware).
    {
        juce::PropertiesFile::Options po;
        po.applicationName     = "AAMP";
        po.filenameSuffix      = "xml";
        po.folderName          = "AAMP";
        po.osxLibrarySubFolder = "Application Support";
        m_props = std::make_unique<juce::PropertiesFile>(po);
    }

    // ===== Seleção DETERMINÍSTICA da engine (sem fallback silencioso) =====
    // JUCE 8: Backend::webview2 é uma preferência; a queda para MSHTML/IE acontece
    // silenciosamente se (a) JUCE_USE_WIN_WEBVIEW2 não estava definido na compilação ou
    // (b) a criação do ambiente WebView2 falha em runtime (loader/COM/pasta de dados).
    // Fechamos os dois portões: pasta de dados explícita e gravável + verificação de
    // suporte com falha RUIDOSA (nunca IE em silêncio). Engine registrada no diag_log.
   #if defined(JUCE_USE_WIN_WEBVIEW2) && JUCE_USE_WIN_WEBVIEW2
    diagLog("[ENGINE] compilado com JUCE_USE_WIN_WEBVIEW2=1 (WebView2 disponivel no binario)");
   #else
    diagLog("[ENGINE] ERRO: binario compilado SEM JUCE_USE_WIN_WEBVIEW2 - so existe o backend IE");
    juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
        "AAMP - WebView2 ausente no binario",
        "Este executavel foi compilado sem suporte a WebView2 (JUCE_USE_WIN_WEBVIEW2).\n"
        "Recompile com o CMakeLists atual e o SDK em AAMP_WEBVIEW2_SDK.");
   #endif

    const juce::File dataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
                                   .getChildFile("AAMP").getChildFile("WebView2");
    dataDir.createDirectory();                                    // pasta de dados SEMPRE gravavel

    const auto options = WBC::Options{}
        .withBackend(WBC::Options::Backend::webview2)             // Edge/Chromium
        .withWinWebView2Options(WBC::Options::WinWebView2{}
            .withUserDataFolder(dataDir)                          // nunca depender de build\ gravavel
            .withStatusBarDisabled()
            .withBuiltInErrorPageDisabled())
        .withNativeIntegrationEnabled()                           // expõe window.__JUCE__.backend ao HTML
        // Canal de comandos JS->C++: window.__JUCE__.backend.emitEvent("aampCommand", "<json>")
        .withEventListener("aampCommand",
            [this](const juce::var& payload) { onCommand(payload.toString()); })
        .withNativeFunction("aampCommand",                        // válido se o módulo JS do JUCE for carregado
            [this](const juce::Array<juce::var>& args,
                   WBC::NativeFunctionCompletion complete) {
                if (args.size() > 0) onCommand(args[0].toString());
                complete(juce::var());                            // ack
            });

    const bool wv2Supported = WBC::areOptionsSupported(options);
    diagLog(juce::String("[ENGINE] WebView2 suportado em runtime: ") + (wv2Supported ? "SIM" : "NAO"));
    if (! wv2Supported) {
        // Falha RUIDOSA: jamais entregar o produto sobre MSHTML. O usuario ve a causa real.
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
            "AAMP - WebView2 indisponivel",
            "O ambiente WebView2 (Edge/Chromium) nao pode ser criado nesta maquina.\n"
            "Verifique o WebView2 Runtime e a pasta de dados:\n" + dataDir.getFullPathName());
    }

    m_web.reset(new WBC(options));
    addAndMakeVisible(*m_web);

    const juce::File html = resolveHtmlFile();
    if (html.existsAsFile()) {
        m_web->goToURL(juce::URL(html).toString(false));          // file:///.../aamp_laboratory.html
    } else {
        m_web->goToURL("data:text/html,"
            "<body style='background:%2306090C;color:%238a9ba6;font-family:sans-serif;"
            "display:flex;align-items:center;justify-content:center;height:100vh'>"
            "<div>aamp_laboratory.html nao encontrado.<br>Esperado em "
            "&lt;exe&gt;/resources/ ou C:\\aamp\\resources\\</div></body>");
    }
}

AampWebBridge::~AampWebBridge() {
    m_abort.store(true);                                 // pede aborto da medição
    if (m_worker.joinable())       m_worker.join();      // aguarda o fim da medição (sem detach)
    if (m_exportWorker.joinable()) m_exportWorker.join();// aguarda o fim de qualquer export em curso
    if (m_props) m_props->saveIfNeeded();                // flush da persistência
    m_dm.closeAudioDevice();                             // libera o ASIO antes de destruir o manager
}

void AampWebBridge::resized() {
    if (m_web) m_web->setBounds(getLocalBounds());
}

// 1) <exe>/resources/aamp_laboratory.html (artefato do build)  2) C:\aamp\resources (fonte/dev).
juce::File AampWebBridge::resolveHtmlFile() {
    const auto exeDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                            .getParentDirectory();
    const juce::File rel = exeDir.getChildFile("resources")
                                 .getChildFile("aamp_laboratory.html");
    if (rel.existsAsFile()) return rel;
    return juce::File("C:\\aamp\\resources\\aamp_laboratory.html");
}

// ------------------------- persistência -------------------------
juce::var AampWebBridge::loadSettingsVar() const {
    auto* o = new juce::DynamicObject();
    const juce::String lang = m_props ? m_props->getValue("language", "") : juce::String();
    if (lang.isNotEmpty()) o->setProperty("language", lang);
    const juce::String hwJson = m_props ? m_props->getValue("hardware", "") : juce::String();
    if (hwJson.isNotEmpty()) {
        const juce::var hw = juce::JSON::parse(hwJson);
        if (hw.isObject()) o->setProperty("hardware", hw);       // corrompido -> omitido (padrão)
    }
    return juce::var(o);
}

void AampWebBridge::saveHardwareSettings(const juce::var& hw) {
    if (m_props == nullptr || !hw.isObject()) return;
    m_props->setValue("hardware", juce::JSON::toString(hw, true));
    m_props->saveIfNeeded();
}

void AampWebBridge::saveLanguage(const juce::String& lang) {
    if (m_props == nullptr || lang.isEmpty()) return;
    m_props->setValue("language", lang);
    m_props->saveIfNeeded();
}

// ------------------------- encerramento conduzido -------------------------
bool AampWebBridge::requestCloseFromHost() {
    if (!m_uiAlive.load()) return false;                 // página nunca falou -> host fecha direto
    emitEvent("closeRequested", juce::var());
    return true;                                         // o frontend decide e envia closeConfirm
}

// ------------------------- emit ao JS -------------------------
void AampWebBridge::emitEvent(const juce::String& evt, const juce::var& data) {
    auto* o = new juce::DynamicObject();
    o->setProperty("evt", evt);
    o->setProperty("data", data);
    juce::var wrapped(o);
    // SafePointer: se a Bridge for destruída com evento ainda enfileirado, o lambda vira no-op.
    juce::Component::SafePointer<AampWebBridge> self(this);
    juce::MessageManager::callAsync([self, wrapped] {
        if (auto* p = self.getComponent())
            if (p->m_web) p->m_web->emitEventIfBrowserIsVisible("aampEvent", wrapped);
    });
}

// ------------------------- roteamento de comandos -------------------------
void AampWebBridge::onCommand(const juce::String& json) {
    m_uiAlive.store(true);                               // frontend vivo (habilita closeRequested)
    const auto v = juce::JSON::parse(json);
    if (!v.isObject()) { diagLog("[BRIDGE] comando ilegível: " + json); return; }
    const auto cmd  = v.getProperty("cmd", juce::var()).toString();
    const auto args = v.getProperty("args", juce::var());
    if      (cmd == "enumerate")    doEnumerate();
    else if (cmd == "prepare")      doPrepare(args);
    else if (cmd == "start")        doStart();
    else if (cmd == "cancel")       doCancel();
    else if (cmd == "export")       doExport(args);
    else if (cmd == "getSettings")  emitEvent("settings", loadSettingsVar());
    else if (cmd == "saveSettings") saveHardwareSettings(args.getProperty("hardware", juce::var()));
    else if (cmd == "setLanguage")  saveLanguage(args.getProperty("lang", juce::var()).toString());
    else if (cmd == "closeConfirm") juce::JUCEApplication::getInstance()->systemRequestedQuit();
    else if (cmd == "diag" || cmd == "fperf")            // diagnóstico gated do frontend (FPERF/PDBG)
        diagLog("[" + cmd.toUpperCase() + "] " + args.getProperty("msg", juce::var()).toString());
    else diagLog("[BRIDGE] comando desconhecido: " + cmd);   // nunca falha em silêncio
}

void AampWebBridge::doEnumerate() {
    m_devices = m_hw->enumerateDevices();                          // M-01: só dispositivos do OS
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

// ------------------------- helpers de abertura de device -------------------------
static juce::AudioIODeviceType* bridgeFindType(juce::AudioDeviceManager& dm,
                                               const juce::String& typeName) {
    for (auto* t : dm.getAvailableDeviceTypes())
        if (t != nullptr && t->getTypeName() == typeName) return t;
    return nullptr;
}
// Nome de entrada correspondente a um endpoint de saída (não-ASIO): casa pelo nome limpo,
// senão usa a primeira entrada do tipo (mesma heurística da implementação de referência).
static juce::String bridgeBestInputFor(juce::AudioIODeviceType* type, const juce::String& outName) {
    if (type == nullptr) return outName;
    auto clean = [](const juce::String& raw) -> juce::String {
        juce::String s = raw.trim();
        if (s.startsWithIgnoreCase("ASIO ")) s = s.substring(5).trim();
        int d = 0;
        while (d < s.length() && s[d] >= '0' && s[d] <= '9') ++d;
        if (d > 0 && d < s.length() && s[d] == '-') s = s.substring(d + 1).trim();
        const int a = s.indexOfChar('('), b2 = s.lastIndexOfChar(')');
        if (a >= 0 && b2 > a) s = s.substring(a + 1, b2).trim();
        return s;
    };
    type->scanForDevices();
    const auto ins = type->getDeviceNames(true);
    const juce::String key = clean(outName);
    for (const auto& n : ins)
        if (key.isNotEmpty() && clean(n).equalsIgnoreCase(key)) return n;
    return ins.size() > 0 ? ins[0] : outName;
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
        o->setProperty("requestId", args.getProperty("requestId", juce::var()));  // Sprint 1: correlação
        emitEvent("prepared", juce::var(o));
    };
    if (chosen == nullptr) { emitError("Nenhum dispositivo de aquisicao conectado."); return; }

    // ===== Configura o AudioDeviceManager PERSISTENTE (abre o device escolhido; sem orchestrator) =====
    const juce::String typeName(chosen->backend.c_str());
    const juce::String devName(chosen->id.c_str());
    const bool isAsio = typeName.containsIgnoreCase("asio");
    m_dm.setCurrentAudioDeviceType(typeName, true);
    juce::AudioIODeviceType* devType = bridgeFindType(m_dm, typeName);
    juce::AudioDeviceManager::AudioDeviceSetup s;
    s.outputDeviceName = devName;
    s.inputDeviceName  = isAsio ? devName
                                : (devType != nullptr ? bridgeBestInputFor(devType, devName) : devName);
    s.useDefaultInputChannels = true; s.useDefaultOutputChannels = true;
    const juce::String hwErr1 = m_dm.setAudioDeviceSetup(s, true);   // [HWSWITCH] erro de abertura (antes descartado)
    juce::AudioIODevice* dev = m_dm.getCurrentAudioDevice();
    if (dev == nullptr) { emitError("Nao foi possivel abrir o dispositivo selecionado."); return; }

    const int nIn  = dev->getInputChannelNames().size();
    const int nOut = dev->getOutputChannelNames().size();

    // Bitmask de canais: Entrada<-nOut/outBase, Saída<-nIn/inBase; pares em estéreo.
    juce::AudioDeviceManager::AudioDeviceSetup cs = m_dm.getAudioDeviceSetup();
    cs.useDefaultInputChannels = false; cs.useDefaultOutputChannels = false;
    cs.inputChannels.clear(); cs.outputChannels.clear();
    const int ib = juce::jlimit(0, juce::jmax(0, nIn  - 1), inBase);
    const int ob = juce::jlimit(0, juce::jmax(0, nOut - 1), outBase);
    cs.inputChannels.setBit(ib);
    cs.outputChannels.setBit(ob);
    if (stereo && nIn >= 2 && nOut >= 2) { cs.inputChannels.setBit(ib + 1); cs.outputChannels.setBit(ob + 1); }
    cs.sampleRate = k::REF_SAMPLE_RATE;                  // DOC-03: fs fixo (48000)
    const juce::String hwErr2 = m_dm.setAudioDeviceSetup(cs, true); // [HWSWITCH] erro de reconfiguração (antes descartado)

    juce::AudioIODevice* dev2 = m_dm.getCurrentAudioDevice();
    m_deviceReady = (dev2 != nullptr);                  // BR-003: device aberto e validado (READY)
    m_prepFs = (dev2 && dev2->getCurrentSampleRate() > 0.0) ? dev2->getCurrentSampleRate() : k::REF_SAMPLE_RATE;
    m_selectedDeviceName = chosen->name;

    // [HWSWITCH] Verificação objetiva da reconfiguração (motivada pelo sintoma Helix-sem-áudio):
    // registra o dispositivo REALMENTE aberto em m_dm vs. o solicitado, os canais ativos e os erros
    // de setup antes descartados. Canal único diag_log.txt; nenhum efeito no fluxo de aquisição.
    diagLog(juce::String("[HWSWITCH] prepare req id='") + juce::String(chosen->id.c_str())
            + "' reqType='" + typeName + "' -> opened='" + (dev2 ? dev2->getName() : juce::String("<null>"))
            + "' curType='" + m_dm.getCurrentAudioDeviceType() + "'"
            + " nIn=" + juce::String(nIn) + " nOut=" + juce::String(nOut)
            + " outMask=" + (dev2 ? dev2->getActiveOutputChannels().toString(2) : juce::String("-"))
            + " inMask="  + (dev2 ? dev2->getActiveInputChannels().toString(2)  : juce::String("-"))
            + " fs=" + juce::String(m_prepFs, 1) + " ready=" + (m_deviceReady ? "1" : "0")
            + (hwErr1.isNotEmpty() ? (" err1='" + hwErr1 + "'") : juce::String())
            + (hwErr2.isNotEmpty() ? (" err2='" + hwErr2 + "'") : juce::String()));

    // Capacidades do device ABERTO -> painel (DOC-05 §3.2) e dropdowns Entrada/Saída (nIn/nOut).
    DeviceInfo full = *chosen;
    full.caps.inputChannels = nIn; full.caps.outputChannels = nOut;
    full.caps.sampleRates.clear(); full.caps.bufferSizes.clear();
    if (dev2) {
        for (double sr : dev2->getAvailableSampleRates()) full.caps.sampleRates.push_back(sr);
        for (int bs : dev2->getAvailableBufferSizes())    full.caps.bufferSizes.push_back(bs);
    }
    // Latência REAL reportada pelo device aberto (round-trip); parcial se o driver não reporta.
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
    // Sprint 1: PreparedConfiguration — descreve exclusivamente a config aplicada ao m_dm (aditivo ao
    // protocolo). Sem requestId/callbacks/flags. Emitida só quando a configuração foi de fato aplicada.
    if (m_deviceReady && dev2 != nullptr) {
        auto* applied = new juce::DynamicObject();
        applied->setProperty("deviceId",   juce::String(chosen->id.c_str()));
        applied->setProperty("mode",       stereo ? "Stereo" : "Mono");
        applied->setProperty("outBase",    outBase);
        applied->setProperty("inBase",     inBase);
        applied->setProperty("sampleRate", m_prepFs);
        applied->setProperty("outMask",    dev2->getActiveOutputChannels().toInteger());
        applied->setProperty("inMask",     dev2->getActiveInputChannels().toInteger());
        applied->setProperty("bufferSize", dev2->getCurrentBufferSizeSamples());
        o->setProperty("applied", juce::var(applied));
    }
    o->setProperty("requestId", args.getProperty("requestId", juce::var()));   // Sprint 1: correlação (HardwareSession na Sprint 5)
    emitEvent("prepared", juce::var(o));

    // Persistência dos parâmetros EFETIVOS do device aberto (fs/buffer atuais) junto à última
    // configuração — restauração completa na próxima abertura (mesmo hardware presente).
    if (m_deviceReady && m_props != nullptr) {
        const juce::String saved = m_props->getValue("hardware", "");
        juce::var hw = saved.isNotEmpty() ? juce::JSON::parse(saved) : juce::var();
        if (hw.isObject() && hw.getProperty("id", juce::var()).toString().toStdString() == chosen->id) {
            if (auto* dobj = hw.getDynamicObject()) {
                dobj->setProperty("sampleRate", m_prepFs);
                if (dev2) dobj->setProperty("bufferSize", dev2->getCurrentBufferSizeSamples());
                saveHardwareSettings(hw);
            }
        }
    }
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

// start() = medição REAL: JuceCaptureDevice(m_dm,fs,&abort) + MeasurementOrchestrator(fs, Mono)
// -> prepare -> runSuite. Orchestrator LOCAL (não membro).
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
        // [HWSWITCH] Dispositivo ATIVO no instante do start — responde objetivamente: a Helix é mesmo
        // o device ativo antes da nova caracterização? Reusa diag_log.txt; não altera o fluxo.
        if (auto* cur = m_dm.getCurrentAudioDevice())
            diagLog(juce::String("[HWSWITCH] start active='") + cur->getName()
                    + "' curType='" + m_dm.getCurrentAudioDeviceType() + "'"
                    + " selName='" + juce::String(m_selectedDeviceName.c_str()) + "'"
                    + " fs=" + juce::String(fs, 1)
                    + " outMask=" + cur->getActiveOutputChannels().toString(2)
                    + " inMask="  + cur->getActiveInputChannels().toString(2));
        else
            diagLog("[HWSWITCH] start active=<null>");
        JuceCaptureDevice realDev(m_dm, fs, &m_abort);            // bloco REAL de play+capture
        MeasurementOrchestrator orch(fs, ChannelMode::Mono);
        wireCallbacks(orch);
        // Fase 1: observa os fatos brutos (estímulo emitido + captura) e acumula um EvidencePackage.
        // O observador é isolado (o orchestrator envolve a chamada em try/catch) — nunca afeta a aquisição.
        aamp::evidence::EvidenceRecorder rec;
        orch.setObservationCallback([&rec](const Stimulus& s, const CaptureResult& c) { rec.observe(s, c); });
        RunResult r;
        const bool prepOk = orch.prepare(realDev);
        if (prepOk) r = orch.runSuite(realDev);                   // M-08 conduz a suite (M-03 gera)
        else { r.outcome = RunOutcome::Aborted; r.userMessage = "Dispositivo nao esta pronto."; }

        if (r.outcome == RunOutcome::Completed) {
            if (!m_selectedDeviceName.empty()) r.bundle.meta.deviceName = m_selectedDeviceName;
            if (r.bundle.meta.timestamp.empty())
                r.bundle.meta.timestamp =
                    juce::Time::getCurrentTime().formatted("%Y-%m-%d %H:%M:%S").toStdString();
            m_lastBundle  = r.bundle;                              // retido p/ ExportEngine (M-07)
            m_haveBundle  = true;
            // A caracterização entrega um .aamp; o Laboratório reabre pelo MESMO caminho (ingest →
            // fromAamp → LabEngine). Fluxo principal (Fase 1): entrega o Evidence Package (evidência
            // bruta), e a ciência é derivada pelo consumidor oficial (ADR-0003). Reversível: com
            // kDeliverEvidence=false volta-se ao .aamp legado (ResultBundle/análise), sem remover nada.
            constexpr bool kDeliverEvidence = true;
            juce::MemoryBlock aampBytes;
            if (kDeliverEvidence) {
                aamp::evidence::SessionEvidence ses;
                ses.sessionId = juce::Uuid().toDashedString().toStdString();   // ADS-0004: id opaco 128-bit local
                ses.timestamp = r.bundle.meta.timestamp;
                rec.setSession(ses);
                aamp::evidence::HardwareEvidence hw;
                hw.deviceName = r.bundle.meta.deviceName;
                hw.sampleRate = fs;
                hw.channels   = aamp::evidence::Channels::Mono;
                rec.setHardware(hw);
                const std::string ev = aamp::evidence::serialize(rec.package());
                aampBytes.append(ev.data(), ev.size());
            } else {
                serializeAampBytes(r.bundle, aampBytes);           // legado (ResultBundle → análise)
            }
            const juce::File dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                       .getChildFile("AAMP").getChildFile("sessions");
            dir.createDirectory();
            const juce::String stamp = juce::String(r.bundle.meta.timestamp.c_str())
                                           .replaceCharacters(": ", "--");
            const juce::File aampFile = dir.getChildFile("characterization_" + stamp + ".aamp");
            const bool wrote = aampFile.replaceWithData(aampBytes.getData(), aampBytes.getSize());
            auto* o = new juce::DynamicObject();
            o->setProperty("outcome", "Completed");
            o->setProperty("aamp",    juce::Base64::toBase64(aampBytes.getData(), aampBytes.getSize()));
            if (wrote) o->setProperty("aampPath", aampFile.getFullPathName());
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
    // ExportEngine (M-07, DOC-10: CSV/WAV/PDF/PNG) sobre o ResultBundle ativo retido em doStart.
    // I/O em thread própria (não bloqueia a Message Thread); emitEvent já re-empacota o resultado.
    const juce::String fmt = args.getProperty("format", juce::var("pdf")).toString().toLowerCase();

    if (!m_haveBundle) {                                  // nada medido ainda: falha amigável (DOC-07)
        auto* o = new juce::DynamicObject();
        o->setProperty("ok", false);
        o->setProperty("format", fmt);
        o->setProperty("message", "Nenhuma medicao disponivel para exportar.");
        emitEvent("exported", juce::var(o));
        return;
    }

    if (m_exportWorker.joinable()) m_exportWorker.join(); // encerra export anterior antes de reusar
    const ResultBundle bundle = m_lastBundle;             // cópia estável (ER-040: bundle nunca mutado)

    m_exportWorker = std::thread([this, fmt, bundle] {
        ExportEngine engine;
        // Destino: <Documentos>/AAMP/exports, criado se necessário. Nomenclatura DOC-10 §7.
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
            // PNG (DOC-10 §6): renderiza a curva primária (MS-01), se presente.
            const MetricResult* fr = bundle.find("MS-01");
            if (fr != nullptr && !fr->curves.empty()) {
                JucePngWriter png;
                const std::string p = pathFor("plot", "png");
                ok = png.write(fr->curves.front(), p); outPath = p;
                if (!ok) message = "Falha ao renderizar PNG.";
            } else {
                message = "Curva indisponivel para exportacao PNG.";
            }
        } else { // "pdf" (padrão): relatório consolidado (6 seções, DOC-10 §5).
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
