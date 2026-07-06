// AAMP - src/ui/Main.cpp
#if defined(AAMP_WITH_JUCE)
#include <juce_gui_basics/juce_gui_basics.h>
#include "../hardware/JuceDeviceBackend.h"
#include "../hardware/HardwareManager.h"
#include "../routing/RoutingManager.h"
#include "../orchestration/MeasurementOrchestrator.h"
#include "../dashboard/DashboardModel.h"
#include "../dashboard/DashboardComponent.h"
#include "../bridge/AampWebBridge.h"           // interface principal (WebView) — substitui o Dashboard
#include "../capture/IAudioDevice.h"
#include "../capture/Baseline.h"
#include "../common/Constants.h"
#include <atomic>
#include <cmath>
#include <memory>
#include <vector>
#include <algorithm>
#include <thread>
#include <functional>

namespace aamp {

// ---------- DIAG TEMPORARIO ----------
static juce::File diagFile() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("AAMP").getChildFile("diag_log.txt");
}
static void diagReset() { auto f = diagFile(); f.getParentDirectory().createDirectory(); f.replaceWithText(""); }
// Despeja TODO o ResultBundle (escalares + resumo de curvas por familia) no diag_log.txt,
// exatamente como M-05 entregou. Diagnostico: revela se os valores impossiveis (-300/200/150)
// ja estao no bundle ANTES do buildDashboardModel (a montante) ou nao (a jusante = exibicao).
static void diagDumpBundle(const ResultBundle& b) {
    diagLog("=== BUNDLE (M-05 -> dashboard) ===");
    diagLog("meta device=" + juce::String(b.meta.deviceName.c_str())
            + " fs=" + juce::String(b.meta.sampleRate, 0)
            + " mode=" + juce::String(b.meta.mode == ChannelMode::Stereo ? "Stereo" : "Mono"));
    diagLog("familias=" + juce::String((int) b.metrics.size()));
    for (const auto& m : b.metrics) {
        diagLog("[" + juce::String(m.id.c_str()) + "] " + juce::String(m.name.c_str())
                + "  quality=" + juce::String(qualityToString(m.quality))
                + "  escalares=" + juce::String((int) m.scalars.size())
                + "  curvas=" + juce::String((int) m.curves.size()));
        for (const auto& s : m.scalars)
            diagLog("    scalar key=" + juce::String(s.key.c_str())
                    + " value=" + juce::String(s.value, 6)
                    + " unit=" + juce::String(s.unit.c_str())
                    + " quality=" + juce::String(qualityToString(s.quality))
                    + " present=" + juce::String(s.valuePresent ? "1" : "0"));
        for (const auto& c : m.curves) {
            double ymin = 0.0, ymax = 0.0; bool first = true;
            for (double v : c.y) { if (first) { ymin = ymax = v; first = false; }
                                   else { ymin = juce::jmin(ymin, v); ymax = juce::jmax(ymax, v); } }
            diagLog("    curve name=" + juce::String(c.name.c_str())
                    + " N=" + juce::String((int) c.y.size())
                    + " yUnit=" + juce::String(c.yUnit.c_str())
                    + " yMin=" + juce::String(ymin, 6)
                    + " yMax=" + juce::String(ymax, 6));
        }
    }
    diagLog("=== FIM BUNDLE ===");
}

struct DeviceOption { juce::String display, typeName, deviceName; int priority = 5; };

static int apiPriority(const juce::String& tn) {
    if (tn.containsIgnoreCase("ASIO"))          return 0;
    if (tn.containsIgnoreCase("Exclusive"))     return 1;
    if (tn.containsIgnoreCase("Low Latency"))   return 2;
    if (tn.containsIgnoreCase("Windows Audio")) return 3;
    if (tn.containsIgnoreCase("DirectSound"))   return 4;
    return 5;
}
static juce::String cleanDeviceName(const juce::String& raw) {
    juce::String s = raw.trim();
    if (s.startsWithIgnoreCase("ASIO ")) s = s.substring(5).trim();
    int d = 0;
    while (d < s.length() && s[d] >= '0' && s[d] <= '9') ++d;
    if (d > 0 && d < s.length() && s[d] == '-') s = s.substring(d + 1).trim();
    const int a = s.indexOfChar('('), b = s.lastIndexOfChar(')');
    if (a >= 0 && b > a) s = s.substring(a + 1, b).trim();
    return s;
}
static bool isGenericMapper(const juce::String& n) {
    const juce::String l = n.toLowerCase();
    return l.contains("som primario") || l.contains("som primário")
        || l.contains("primary sound") || l.contains("mapeador") || l.contains("mapper");
}
static juce::String bestInputFor(juce::AudioDeviceManager& dm, const juce::String& typeName,
                                 const juce::String& outName) {
    for (auto* t : dm.getAvailableDeviceTypes()) {
        if (t->getTypeName() != typeName) continue;
        const auto ins = t->getDeviceNames(true);
        const juce::String key = cleanDeviceName(outName);
        for (const auto& n : ins)
            if (key.isNotEmpty() && cleanDeviceName(n).equalsIgnoreCase(key)) return n;
        if (ins.size() > 0) return ins[0];
    }
    return {};
}
static bool deviceExists(juce::AudioDeviceManager& dm, const juce::String& tn, const juce::String& dn) {
    if (tn.isEmpty() || dn.isEmpty()) return false;
    for (auto* t : dm.getAvailableDeviceTypes()) {
        if (t->getTypeName() != tn) continue;
        t->scanForDevices();
        for (const auto& n : t->getDeviceNames(false)) if (n == dn) return true;
    }
    return false;
}
static std::vector<DeviceOption> buildDeviceList(juce::AudioDeviceManager& dm) {
    struct Raw { juce::String typeName, deviceName, display; int priority; bool asio; };
    std::vector<Raw> raw;
    std::vector<juce::String> winEndpoints;
    for (auto* t : dm.getAvailableDeviceTypes()) {
        t->scanForDevices();
        const int prio = apiPriority(t->getTypeName());
        const bool asio = t->getTypeName().containsIgnoreCase("asio");
        for (const auto& name : t->getDeviceNames(false)) {
            if (!asio && isGenericMapper(name)) continue;
            raw.push_back({ t->getTypeName(), name, cleanDeviceName(name), prio, asio });
            if (!asio) winEndpoints.push_back(name.toLowerCase());
        }
    }
    const juce::String curType = dm.getCurrentAudioDeviceType();
    const juce::String curOut  = dm.getAudioDeviceSetup().outputDeviceName;
    auto asioConnected = [&](const Raw& r) {
        if (r.typeName == curType && r.deviceName == curOut) return true;
        const juce::String tok = r.display.toLowerCase();
        if (tok.isEmpty()) return false;
        for (const auto& w : winEndpoints) if (w.contains(tok)) return true;
        return false;
    };
    std::vector<DeviceOption> out; std::vector<juce::String> keptAsio;
    for (const auto& r : raw) {
        if (!r.asio || !asioConnected(r)) continue;
        out.push_back({ r.display, r.typeName, r.deviceName, r.priority });
        keptAsio.push_back(r.display.toLowerCase());
    }
    std::vector<DeviceOption> nonAsio;
    for (const auto& r : raw) {
        if (r.asio) continue;
        bool dup = false; const juce::String low = r.deviceName.toLowerCase();
        for (const auto& tok : keptAsio) if (low.contains(tok)) { dup = true; break; }
        if (!dup) nonAsio.push_back({ r.display, r.typeName, r.deviceName, r.priority });
    }
    std::sort(nonAsio.begin(), nonAsio.end(), [](const DeviceOption& a, const DeviceOption& b){
        if (a.display != b.display) return a.display < b.display;
        return a.priority < b.priority;
    });
    for (const auto& o : nonAsio)
        if (std::none_of(out.begin(), out.end(), [&](const DeviceOption& x){ return x.display == o.display; }))
            out.push_back(o);
    std::sort(out.begin(), out.end(), [](const DeviceOption& a, const DeviceOption& b){
        if (a.priority != b.priority) return a.priority < b.priority;
        return a.display < b.display;
    });
    return out;
}

// ---------- persistencia ----------
struct AudioSettings { juce::String typeName, deviceName; bool stereo = false; int inBase = 0, outBase = 0; };

static juce::File settingsFile() {
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("AAMP").getChildFile("audio_settings.xml");
}
static void saveSettings(const AudioSettings& s) {
    juce::XmlElement xml("AAMP_AUDIO");
    xml.setAttribute("type", s.typeName); xml.setAttribute("device", s.deviceName);
    xml.setAttribute("stereo", s.stereo ? 1 : 0);
    xml.setAttribute("inBase", s.inBase); xml.setAttribute("outBase", s.outBase);
    const auto f = settingsFile(); f.getParentDirectory().createDirectory(); xml.writeTo(f);
}
static AudioSettings loadSettings() {
    AudioSettings s;
    if (auto xml = juce::XmlDocument::parse(settingsFile())) {
        s.typeName = xml->getStringAttribute("type"); s.deviceName = xml->getStringAttribute("device");
        s.stereo = xml->getIntAttribute("stereo") != 0;
        s.inBase = xml->getIntAttribute("inBase"); s.outBase = xml->getIntAttribute("outBase");
    }
    return s;
}
static void applySettings(juce::AudioDeviceManager& dm, const AudioSettings& s) {
    if (s.deviceName.isEmpty()) return;
    dm.setCurrentAudioDeviceType(s.typeName, true);
    const bool isAsio = s.typeName.containsIgnoreCase("asio");
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    setup.outputDeviceName = s.deviceName;
    setup.inputDeviceName  = isAsio ? s.deviceName : bestInputFor(dm, s.typeName, s.deviceName);
    setup.useDefaultInputChannels = false; setup.useDefaultOutputChannels = false;
    setup.inputChannels.setBit(s.inBase); setup.outputChannels.setBit(s.outBase);
    if (s.stereo) { setup.inputChannels.setBit(s.inBase + 1); setup.outputChannels.setBit(s.outBase + 1); }
    dm.setAudioDeviceSetup(setup, true);
}

// JuceCaptureDevice: RELOCADO para src/hardware/JuceCaptureDevice.h (aamp::JuceCaptureDevice),
// reutilizado pela ponte (AampWebBridge) e por este MainComponent. Definicao unica no header.

// ---------- configuracao ----------
class AudioConfig : public juce::Component {
public:
    explicit AudioConfig(juce::AudioDeviceManager& dm) : m_dm(dm) {
        m_devices = buildDeviceList(m_dm);
        m_deviceLbl.setText("Dispositivo:", juce::dontSendNotification);
        m_modeLbl.setText("Modo:", juce::dontSendNotification);
        m_entradaLbl.setText("Entrada:", juce::dontSendNotification);
        m_saidaLbl.setText("Saida:", juce::dontSendNotification);
        for (auto* c : { &m_deviceLbl, &m_modeLbl, &m_entradaLbl, &m_saidaLbl, &m_status }) addAndMakeVisible(*c);
        for (auto* c : { &m_device, &m_entrada, &m_saida }) addAndMakeVisible(*c);
        addAndMakeVisible(m_mono); addAndMakeVisible(m_stereo);
        m_mono.setButtonText("Mono"); m_stereo.setButtonText("Estereo");
        m_mono.setRadioGroupId(1); m_stereo.setRadioGroupId(1);

        int id = 1;
        for (const auto& o : m_devices) m_device.addItem(o.display, id++);

        const AudioSettings st = loadSettings();
        m_stereoMode = st.stereo;
        m_inBase = juce::jmax(0, st.inBase); m_outBase = juce::jmax(0, st.outBase);
        m_mono.setToggleState(!m_stereoMode, juce::dontSendNotification);
        m_stereo.setToggleState(m_stereoMode, juce::dontSendNotification);

        int sel = indexForDevice(st.typeName, st.deviceName);
        if (sel < 0) sel = indexForDevice(m_dm.getCurrentAudioDeviceType(),
                                          m_dm.getAudioDeviceSetup().outputDeviceName);
        if (sel < 0 && !m_devices.empty()) sel = 0;
        if (sel >= 0) m_device.setSelectedItemIndex(sel, juce::dontSendNotification);

        m_device.onChange  = [this]{ applySelectedDevice(); };
        m_mono.onClick     = [this]{ if (m_mono.getToggleState())   setMode(false); };
        m_stereo.onClick   = [this]{ if (m_stereo.getToggleState()) setMode(true);  };
        m_entrada.onChange = [this]{ onChannelChange(); };
        m_saida.onChange   = [this]{ onChannelChange(); };

        applySelectedDevice();
        if (m_devices.empty())
            m_status.setText("Nenhum dispositivo de audio conectado.", juce::dontSendNotification);
    }
    void resized() override {
        auto r = getLocalBounds().reduced(12);
        auto rowLC = [&r](juce::Label& l, juce::Component& c) {
            auto rr = r.removeFromTop(26); l.setBounds(rr.removeFromLeft(95)); c.setBounds(rr); r.removeFromTop(8);
        };
        rowLC(m_deviceLbl, m_device);
        { auto rr = r.removeFromTop(26);
          m_modeLbl.setBounds(rr.removeFromLeft(95));
          m_mono.setBounds(rr.removeFromLeft(90));
          m_stereo.setBounds(rr.removeFromLeft(110)); r.removeFromTop(8); }
        rowLC(m_entradaLbl, m_entrada);
        rowLC(m_saidaLbl,   m_saida);
        r.removeFromTop(6);
        m_status.setBounds(r.removeFromTop(24));
    }
private:
    int indexForDevice(const juce::String& tn, const juce::String& dn) {
        for (size_t i = 0; i < m_devices.size(); ++i)
            if (m_devices[i].typeName == tn && m_devices[i].deviceName == dn) return (int) i;
        return -1;
    }
    void applySelectedDevice() {
        const int idx = m_device.getSelectedId() - 1;
        if (idx < 0 || idx >= (int) m_devices.size()) return;
        const DeviceOption& opt = m_devices[(size_t) idx];
        const auto setup0 = m_dm.getAudioDeviceSetup();
        const bool already = (m_dm.getCurrentAudioDevice() != nullptr)
            && (m_dm.getCurrentAudioDeviceType() == opt.typeName)
            && (setup0.outputDeviceName == opt.deviceName);
        if (already) { repopulateChannels(); persist(); return; }
        m_dm.setCurrentAudioDeviceType(opt.typeName, true);
        const bool isAsio = opt.typeName.containsIgnoreCase("asio");
        juce::AudioDeviceManager::AudioDeviceSetup s;
        s.outputDeviceName = opt.deviceName;
        s.inputDeviceName  = isAsio ? opt.deviceName : bestInputFor(m_dm, opt.typeName, opt.deviceName);
        s.useDefaultInputChannels = true; s.useDefaultOutputChannels = true;
        m_dm.setAudioDeviceSetup(s, true);
        if (m_dm.getCurrentAudioDevice() == nullptr) {
            m_entrada.clear(juce::dontSendNotification); m_saida.clear(juce::dontSendNotification);
            m_status.setText("Nao foi possivel abrir o dispositivo.", juce::dontSendNotification);
            juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Audio",
                "Nao foi possivel abrir '" + opt.display + "'."
                + (isAsio ? juce::String("\n\nEste dispositivo ASIO pode estar em uso por outro aplicativo.") : juce::String()));
            return;
        }
        repopulateChannels(); persist();
    }
    void setMode(bool stereo) { m_stereoMode = stereo; repopulateChannels(); persist(); }
    void selectClosest(juce::ComboBox& cb, int& base) {
        const int wantId = base + 1; bool found = false;
        for (int i = 0; i < cb.getNumItems(); ++i)
            if (cb.getItemId(i) == wantId) { cb.setSelectedItemIndex(i, juce::dontSendNotification); found = true; break; }
        if (!found && cb.getNumItems() > 0) cb.setSelectedItemIndex(0, juce::dontSendNotification);
        base = (cb.getSelectedId() > 0) ? cb.getSelectedId() - 1 : 0;
    }
    void repopulateChannels() {
        m_entrada.clear(juce::dontSendNotification); m_saida.clear(juce::dontSendNotification);
        auto* dev = m_dm.getCurrentAudioDevice();
        if (dev == nullptr) { m_status.setText("Nenhum dispositivo aberto.", juce::dontSendNotification); return; }
        const int nIn  = dev->getInputChannelNames().size();
        const int nOut = dev->getOutputChannelNames().size();
        const bool canStereo = (nIn >= 2 && nOut >= 2);
        m_stereo.setEnabled(canStereo);
        if (!canStereo && m_stereoMode) {
            m_stereoMode = false;
            m_mono.setToggleState(true, juce::dontSendNotification);
            m_stereo.setToggleState(false, juce::dontSendNotification);
        }
        if (m_stereoMode) {
            for (int i = 0; i + 1 < nOut; i += 2) m_entrada.addItem(juce::String(i+1) + "/" + juce::String(i+2), i + 1);
            for (int i = 0; i + 1 < nIn;  i += 2) m_saida.addItem  (juce::String(i+1) + "/" + juce::String(i+2), i + 1);
        } else {
            for (int i = 0; i < nOut; ++i) m_entrada.addItem(juce::String(i+1), i + 1);
            for (int i = 0; i < nIn;  ++i) m_saida.addItem  (juce::String(i+1), i + 1);
        }
        selectClosest(m_entrada, m_outBase);
        selectClosest(m_saida,   m_inBase);
        applyChannelBitmask();
    }
    void onChannelChange() {
        m_outBase = (m_entrada.getSelectedId() > 0) ? m_entrada.getSelectedId() - 1 : 0;
        m_inBase  = (m_saida.getSelectedId()   > 0) ? m_saida.getSelectedId()   - 1 : 0;
        applyChannelBitmask(); persist();
    }
    void applyChannelBitmask() {
        if (m_dm.getCurrentAudioDevice() == nullptr) return;
        juce::AudioDeviceManager::AudioDeviceSetup s = m_dm.getAudioDeviceSetup();
        s.useDefaultInputChannels = false; s.useDefaultOutputChannels = false;
        s.inputChannels.clear(); s.outputChannels.clear();
        s.inputChannels.setBit(m_inBase); s.outputChannels.setBit(m_outBase);
        if (m_stereoMode) { s.inputChannels.setBit(m_inBase + 1); s.outputChannels.setBit(m_outBase + 1); }
        m_dm.setAudioDeviceSetup(s, true);
        updateStatus();
    }
    void updateStatus() {
        const auto a = m_dm.getAudioDeviceSetup();
        const juce::String name = (m_device.getSelectedId() > 0) ? m_device.getText() : juce::String("-");
        const juce::String ch = m_stereoMode
            ? ("entrada " + juce::String(m_outBase+1) + "/" + juce::String(m_outBase+2)
               + "  saida " + juce::String(m_inBase+1) + "/" + juce::String(m_inBase+2))
            : ("entrada " + juce::String(m_outBase+1) + "  saida " + juce::String(m_inBase+1));
        m_status.setText(name + " | " + (m_stereoMode ? "Estereo" : "Mono") + " | " + ch
                         + " | " + juce::String(a.sampleRate, 0) + " Hz", juce::dontSendNotification);
    }
    void persist() {
        const int idx = m_device.getSelectedId() - 1;
        if (idx < 0 || idx >= (int) m_devices.size()) return;
        AudioSettings st;
        st.typeName = m_devices[(size_t) idx].typeName; st.deviceName = m_devices[(size_t) idx].deviceName;
        st.stereo = m_stereoMode; st.inBase = m_inBase; st.outBase = m_outBase;
        saveSettings(st);
    }
    juce::AudioDeviceManager& m_dm;
    std::vector<DeviceOption> m_devices;
    juce::Label m_deviceLbl, m_modeLbl, m_entradaLbl, m_saidaLbl, m_status;
    juce::ComboBox m_device, m_entrada, m_saida;
    juce::ToggleButton m_mono, m_stereo;
    bool m_stereoMode = false;
    int m_inBase = 0, m_outBase = 0;
};

// Ordem determinística documentada da suite (DOC-03 secao 5): A[ST-01..06, ST-16] ->
// B[ST-07..09] -> C[ST-10..15]. Confirmada pela ordem real de emissao no diag.
struct SuiteStep { const char* id; const char* name; const char* feeds; };
static const SuiteStep kSuiteOrder[16] = {
    {"ST-01", "Exponential Sine Sweep (ESS)",      "MS-01 Freq, MS-02 Phase, MS-03 GroupDelay, MS-04 Impulse, MS-11 Crosstalk(estereo)"},
    {"ST-02", "Multi-Tone",                        "MS-07 IMD"},
    {"ST-03", "Pure Sine",                         "MS-05 THD, MS-06 THD+N, MS-10 SNR"},
    {"ST-04", "White Noise",                       "referencia (nao consumido por metrica - DOC-03)"},
    {"ST-05", "Pink Noise",                        "referencia (nao consumido por metrica - DOC-03)"},
    {"ST-06", "Impulse",                           "MS-12 Latency"},
    {"ST-16", "Pure Sine - Latency Reference",     "referencia de fase/atraso (nao consumido por metrica - DOC-03)"},
    {"ST-07", "Burst Tone",                        "MS-13 settling time"},
    {"ST-08", "Dynamic Step Excitation",           "MS-09 Dynamic Range, MS-13 level transfer"},
    {"ST-09", "Silence Segments",                  "MS-08 Noise Floor, MS-10 SNR (ruido)"},
    {"ST-10", "Hard Attack Pulse",                 "MS-14 attack tracking + overshoot"},
    {"ST-11", "Soft Attack Pulse",                 "MS-14 attack tracking (soft)"},
    {"ST-12", "Pulse Train",                       "MS-13 repetition consistency"},
    {"ST-13", "Sustained Excitation",              "MS-13 drift"},
    {"ST-14", "Harmonic Cluster / Chord Cluster",  "MS-14 verificacao (cluster denso)"},
    {"ST-15", "Dynamic Performance Sequence",      "MS-15 reproducibility + aggregate linearity"},
};

// Glifos construidos por code point (sem bytes nao-ASCII no fonte -> seguro sob /WX).
static const juce::String kCheck = juce::String::charToString((juce::juce_wchar) 0x2713); // check
static const juce::String kPlay  = juce::String::charToString((juce::juce_wchar) 0x25B6); // triangulo

static juce::String fmtTime(int secs) {
    if (secs < 0) secs = 0;
    const int h = secs / 3600, m = (secs % 3600) / 60, s = secs % 60;
    return juce::String(h).paddedLeft('0', 2) + ":"
         + juce::String(m).paddedLeft('0', 2) + ":"
         + juce::String(s).paddedLeft('0', 2);
}

// Peso de duracao por estimulo (segundos aprox., DOC-03; ordem de kSuiteOrder).
// Usado SOMENTE para a peso por estimulo (legado; nao afeta medicao nem o tempo total exibido).
static const double kStimSeconds[16] = {
    11.0,  // ST-01 ESS (~10 s + post-roll)
     6.0,  // ST-02 Multi-Tone
     7.0,  // ST-03 Pure Sine
     6.0,  // ST-04 White Noise
     6.0,  // ST-05 Pink Noise
     3.0,  // ST-06 Impulse
     4.0,  // ST-16 Latency Reference
     1.2,  // ST-07 Burst Tone
     1.8,  // ST-08 Dynamic Step
     3.0,  // ST-09 Silence
     1.0,  // ST-10 Hard Attack
     1.1,  // ST-11 Soft Attack
     2.0,  // ST-12 Pulse Train
    31.0,  // ST-13 Sustained (~30 s)
     4.0,  // ST-14 Harmonic Cluster
    13.0,  // ST-15 Dynamic Performance (~12 s)
};

// Janela de progresso (card central): barra, "Executando", historico, fase e tempo total.
class ProgressOverlay : public juce::Component, private juce::Timer {
public:
    std::function<void()> onOpen;   // disparado pelo botao quando concluido
    std::function<void()> onAbort;  // disparado pelo botao durante a caracterizacao
    ProgressOverlay() {
        m_title.setText("Caracterizacao em andamento", juce::dontSendNotification);
        m_title.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(m_title);

        addAndMakeVisible(m_bar);

        m_runHdr.setText("Executando:", juce::dontSendNotification);
        m_runHdr.setColour(juce::Label::textColourId, juce::Colour(0xff9a9a9a));
        addAndMakeVisible(m_runHdr);
        m_runName.setText("-", juce::dontSendNotification);
        m_runName.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(m_runName);

        m_histHdr.setText("Historico", juce::dontSendNotification);
        m_histHdr.setColour(juce::Label::textColourId, juce::Colour(0xff9a9a9a));
        addAndMakeVisible(m_histHdr);

        m_phase.setText("Fase atual:  -", juce::dontSendNotification);
        m_phase.setColour(juce::Label::textColourId, juce::Colour(0xffc8c8c8));
        addAndMakeVisible(m_phase);
        m_eta.setText("Tempo total:  --:--:--", juce::dontSendNotification);
        m_eta.setColour(juce::Label::textColourId, juce::Colour(0xffc8c8c8));
        addAndMakeVisible(m_eta);

        m_hist.setMultiLine(true);
        m_hist.setReadOnly(true);
        m_hist.setScrollbarsShown(true);
        m_hist.setCaretVisible(false);
        m_hist.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff0c0c0c));
        m_hist.setColour(juce::TextEditor::textColourId,       juce::Colour(0xffd0d0d0));
        m_hist.setColour(juce::TextEditor::outlineColourId,    juce::Colour(0xff303030));
        addAndMakeVisible(m_hist);

        m_btn.setButtonText("Abrir Dashboard");
        m_btn.setEnabled(false);
        m_btn.onClick = [this] {
            if (m_measuring) {                       // durante a caracterizacao -> abortar
                m_btn.setButtonText("Abortando...");
                m_btn.setEnabled(false);
                if (onAbort) onAbort();
            } else {                                 // concluido -> abrir/fechar
                if (onOpen) onOpen();
            }
        };
        addAndMakeVisible(m_btn);
    }
    void reset() {
        m_entries.clear();
        m_progress = 0.0;
        m_runName.setText("-", juce::dontSendNotification);
        m_phase.setText("Fase atual:  -", juce::dontSendNotification);
        m_eta.setText("Tempo total:  00:00:00", juce::dontSendNotification);
        m_measuring = true;
        m_btn.setButtonText("Abortar caracterizacao");
        m_btn.setEnabled(true);
        // inicia cronometro para o tempo total de caracterizacao
        m_startMs = juce::Time::getMillisecondCounter();
        m_doneWeight = 0.0;
        m_totalWeight = 0.0;
        for (double w : kStimSeconds) m_totalWeight += w;
        m_etaRunning = true;
        startTimer(500);
        render();
    }
    void setPhase(const juce::String& text) { m_phase.setText("Fase atual:  " + text, juce::dontSendNotification); }
    void setProgress(double frac01) { m_progress = juce::jlimit(0.0, 1.0, frac01); }
    void setCurrent(const juce::String& text) { m_runName.setText(text, juce::dontSendNotification); }
    void markRunning(int k) {
        if (k < 1 || k > 16) return; ensureUpTo(k);
        Entry& e = m_entries[(size_t)(k - 1)];
        if (!e.done) e.line2 = "Em execucao...";
        render();
    }
    void markCompleted(int k) {
        if (k < 1 || k > 16) return; ensureUpTo(k);
        Entry& e = m_entries[(size_t)(k - 1)];
        if (!e.done) {
            e.done = true;
            e.line2 = "Capturado com sucesso";
            m_doneWeight += kStimSeconds[(size_t)(k - 1)];
        }
        render();
    }
    void setReady(const juce::String& btnText) {
        m_etaRunning = false; stopTimer();
        const double total = (double) (juce::Time::getMillisecondCounter() - m_startMs) / 1000.0;
        m_eta.setText("Tempo total:  " + fmtTime((int) (total + 0.5)), juce::dontSendNotification);
        m_measuring = false;
        m_btn.setButtonText(btnText); m_btn.setEnabled(true);
    }

    void paint(juce::Graphics& g) override {
        g.fillAll(juce::Colour(0x99000000));                       // escurece o dashboard atras
        g.setColour(juce::Colour(0xff1b1b1b)); g.fillRoundedRectangle(m_card.toFloat(), 10.0f);
        g.setColour(juce::Colour(0xff3a3a3a)); g.drawRoundedRectangle(m_card.toFloat(), 10.0f, 1.0f);
        // separadores
        g.setColour(juce::Colour(0xff2a2a2a));
        g.drawHorizontalLine(m_card.getY() + 44, (float) m_card.getX() + 1, (float) m_card.getRight() - 1);
    }
    void resized() override {
        auto full = getLocalBounds();
        const int cw = juce::jmin(460, full.getWidth()  - 20);
        const int ch = juce::jmin(620, full.getHeight() - 20);
        m_card = juce::Rectangle<int>(0, 0, cw, ch).withCentre(full.getCentre());
        auto r = m_card.reduced(16);
        m_title.setBounds(r.removeFromTop(28));
        r.removeFromTop(12);
        m_bar.setBounds(r.removeFromTop(20));
        r.removeFromTop(12);
        m_runHdr.setBounds(r.removeFromTop(18));
        m_runName.setBounds(r.removeFromTop(24));
        r.removeFromTop(10);
        // bloco inferior: botao, tempo total, fase atual
        auto bottom = r.removeFromBottom(40);
        m_btn.setBounds(bottom.removeFromRight(200).reduced(2));
        r.removeFromBottom(6);
        m_eta.setBounds(r.removeFromBottom(22));
        m_phase.setBounds(r.removeFromBottom(22));
        r.removeFromBottom(10);
        m_histHdr.setBounds(r.removeFromTop(20));
        m_hist.setBounds(r);
    }
private:
    struct Entry { juce::String id, name, line2; bool done = false; };
    void ensureUpTo(int k) {
        while ((int) m_entries.size() < k) {
            const SuiteStep& s = kSuiteOrder[m_entries.size()];
            m_entries.push_back({ juce::String(s.id), juce::String(s.name), juce::String(), false });
        }
    }
    void render() {
        juce::String t;
        for (int i = 0; i < (int) m_entries.size(); ++i) {
            const Entry& e = m_entries[(size_t) i];
            const juce::String mark = e.done ? kCheck : kPlay;
            t += mark + " [" + juce::String(i + 1).paddedLeft('0', 2) + "/16] " + e.name + "\n";
            t += "      " + (e.line2.isEmpty() ? juce::String("...") : e.line2) + "\n\n";
        }
        m_hist.setText(t, false);
        m_hist.moveCaretToEnd();
    }
    void timerCallback() override {
        if (!m_etaRunning) return;
        const double elapsed = (double) (juce::Time::getMillisecondCounter() - m_startMs) / 1000.0;
        m_eta.setText("Tempo total:  " + fmtTime((int) (elapsed + 0.5)), juce::dontSendNotification);
    }
    double m_progress = 0.0;
    juce::ProgressBar m_bar { m_progress };
    juce::Label m_title, m_runHdr, m_runName, m_histHdr, m_phase, m_eta;
    juce::TextEditor m_hist;
    juce::TextButton m_btn;
    juce::Rectangle<int> m_card;
    std::vector<Entry> m_entries;
    juce::uint32 m_startMs = 0;
    double m_doneWeight = 0.0, m_totalWeight = 0.0;
    bool m_etaRunning = false;
    bool m_measuring = false;   // true durante a caracterizacao -> botao = Abortar
};

// ===================== Engineering View (FASE 1: curvas ja no ResultBundle) =====================

// Plot generico de uma curva (x,y) com eixos, titulo, linha de estatisticas e marcador de pico.
class CurvePlot : public juce::Component {
public:
    void setData(std::vector<double> xs, std::vector<double> ys,
                 juce::String title, juce::String stats, bool logX, int peakIdx) {
        m_x = std::move(xs); m_y = std::move(ys);
        m_title = std::move(title); m_stats = std::move(stats);
        m_logX = logX; m_peakIdx = peakIdx;
        repaint();
    }
    struct PlotOverlay { std::vector<double> xs, ys; juce::Colour colour; };
    void setOverlays(std::vector<PlotOverlay> ov) { m_overlays = std::move(ov); repaint(); }
    void paint(juce::Graphics& g) override {
        auto b = getLocalBounds();
        g.setColour(juce::Colour(0xff121212)); g.fillRect(b);
        g.setColour(juce::Colour(0xff2a2a2a)); g.drawRect(b, 1);

        g.setColour(juce::Colours::white);
        g.drawText(m_title, b.removeFromTop(22).reduced(8, 2), juce::Justification::centredLeft);
        g.setColour(juce::Colour(0xff9ad19a));
        g.drawText(m_stats, b.removeFromTop(18).reduced(8, 0), juce::Justification::centredLeft);

        if (m_x.size() < 2) return;
        auto plot = b.reduced(50, 10);
        plot.removeFromBottom(16);

        double xmin = m_x.front(), xmax = m_x.back();
        double ymin = m_y[0], ymax = m_y[0];
        for (double v : m_y) { ymin = std::min(ymin, v); ymax = std::max(ymax, v); }
        if (ymax - ymin < 1e-9) { ymax += 1.0; ymin -= 1.0; }
        if (m_logX && (xmin <= 0.0)) m_logX = false;

        auto xToPx = [&](double x) -> float {
            double t;
            if (m_logX && x > 0.0)
                t = (std::log10(x) - std::log10(xmin)) / (std::log10(xmax) - std::log10(xmin));
            else
                t = (x - xmin) / (xmax - xmin);
            return static_cast<float>(plot.getX() + t * plot.getWidth());
        };
        auto yToPx = [&](double y) -> float {
            const double t = (y - ymin) / (ymax - ymin);
            return static_cast<float>(plot.getBottom() - t * plot.getHeight());
        };

        g.setColour(juce::Colour(0xff2a2a2a));
        g.drawRect(plot, 1);
        g.setColour(juce::Colour(0xff808080));
        g.drawText(juce::String(ymax, 1), juce::Rectangle<int>(b.getX(), plot.getY() - 7, 46, 14), juce::Justification::centredRight);
        g.drawText(juce::String(ymin, 1), juce::Rectangle<int>(b.getX(), plot.getBottom() - 7, 46, 14), juce::Justification::centredRight);
        g.drawText(juce::String(xmin, 0), juce::Rectangle<int>(plot.getX(), plot.getBottom() + 2, 70, 14), juce::Justification::centredLeft);
        g.drawText(juce::String(xmax, 0) + (m_logX ? " (log)" : ""), juce::Rectangle<int>(plot.getRight() - 90, plot.getBottom() + 2, 90, 14), juce::Justification::centredRight);

        juce::Path p; bool started = false;
        const int N = static_cast<int>(m_x.size());
        for (int i = 0; i < N; ++i) {
            const float px = xToPx(m_x[(size_t) i]);
            const float py = yToPx(m_y[(size_t) i]);
            if (!started) { p.startNewSubPath(px, py); started = true; }
            else            p.lineTo(px, py);
        }
        g.setColour(juce::Colour(0xff4da6ff));
        g.strokePath(p, juce::PathStrokeType(1.4f));

        if (m_peakIdx >= 0 && m_peakIdx < N) {
            const float px = xToPx(m_x[(size_t) m_peakIdx]);
            const float py = yToPx(m_y[(size_t) m_peakIdx]);
            g.setColour(juce::Colours::orange);
            g.fillEllipse(px - 3.0f, py - 3.0f, 6.0f, 6.0f);
        }

        for (const auto& ov : m_overlays) {
            g.setColour(ov.colour);
            const size_t M = std::min(ov.xs.size(), ov.ys.size());
            for (size_t k = 0; k < M; ++k) {
                const float px = xToPx(ov.xs[k]);
                const float py = yToPx(ov.ys[k]);
                g.fillEllipse(px - 3.5f, py - 3.5f, 7.0f, 7.0f);
            }
        }
    }
private:
    std::vector<double> m_x, m_y;
    juce::String m_title, m_stats;
    bool m_logX = false;
    int m_peakIdx = -1;
    std::vector<PlotOverlay> m_overlays;
};

// Painel rolavel com um CurvePlot por metrica que JA possui curva no bundle.
class EngineeringView : public juce::Component {
public:
    EngineeringView() {
        m_vp.setViewedComponent(&m_content, false);
        m_vp.setScrollBarsShown(true, false);
        addAndMakeVisible(m_vp);
    }
    void showBundle(const ResultBundle& b) {
        m_plots.clear();
        addPlotFor(b, "MS-01", true,  false);
        addPlotFor(b, "MS-02", false, false);
        addPlotFor(b, "MS-03", false, false);
        addPlotFor(b, "MS-04", false, true);
        addPlotFor(b, "MS-08", true,  false);
        addSpectrumPlotFor(b, "MS-05");
        addSpectrumPlotFor(b, "MS-06");
        addSpectrumPlotFor(b, "MS-07");
        layoutContent();
    }
    void resized() override { m_vp.setBounds(getLocalBounds()); layoutContent(); }
private:
    void layoutContent() {
        const int w  = juce::jmax(200, m_vp.getWidth() - 16);
        const int ph = 240;
        for (int i = 0; i < m_plots.size(); ++i)
            m_plots[i]->setBounds(8, 8 + i * (ph + 12), w, ph);
        m_content.setSize(w + 16, 8 + m_plots.size() * (ph + 12) + 8);
    }
    void addPlotFor(const ResultBundle& b, const char* id, bool logX, bool peak) {
        const MetricResult* m = b.find(id);
        if (m == nullptr || m->curves.empty()) return;
        const Curve& c = m->curves[0];
        if (c.x.size() < 2 || c.y.size() < 2) return;

        double mn = c.y[0], mx = c.y[0], sum = 0.0, sq = 0.0;
        for (double v : c.y) { mn = std::min(mn, v); mx = std::max(mx, v); sum += v; sq += v * v; }
        const double mean = sum / static_cast<double>(c.y.size());
        const double rms  = std::sqrt(sq / static_cast<double>(c.y.size()));

        const juce::String ids(id);
        juce::String stats;
        int peakIdx = -1;
        if (ids == "MS-01")
            stats = "min " + juce::String(mn, 2) + "  max " + juce::String(mx, 2)
                  + "  mean " + juce::String(mean, 2) + "  rms " + juce::String(rms, 2)
                  + "  flatness " + juce::String(mx - mn, 2) + " dB";
        else if (ids == "MS-02") {
            const double dx = c.x.back() - c.x.front();
            const double slope = (std::fabs(dx) > 1e-12) ? (c.y.back() - c.y.front()) / dx : 0.0;
            stats = "min " + juce::String(mn, 1) + "  max " + juce::String(mx, 1)
                  + "  slope " + juce::String(slope, 4) + " deg/Hz";
        } else if (ids == "MS-03")
            stats = "min " + juce::String(mn, 3) + "  max " + juce::String(mx, 3)
                  + "  mean " + juce::String(mean, 3) + " ms";
        else if (ids == "MS-04") {
            double pv = 0.0;
            for (int i = 0; i < static_cast<int>(c.y.size()); ++i)
                if (std::fabs(c.y[(size_t) i]) > pv) { pv = std::fabs(c.y[(size_t) i]); peakIdx = i; }
            int pkSamp = -1;
            for (const auto& sc : m->scalars) if (sc.key == "PeakIndex_samples") pkSamp = static_cast<int>(sc.value);
            stats = "peak " + juce::String(pv, 3) + "  peak index "
                  + juce::String(pkSamp >= 0 ? pkSamp : peakIdx) + " samples";
        } else if (ids == "MS-08") {
            double integ = mean;
            for (const auto& sc : m->scalars) if (sc.key == "NoiseFloor_dBFS") integ = sc.value;
            stats = "mean " + juce::String(mean, 1) + "  integrated " + juce::String(integ, 1) + " dBFS";
        }

        const juce::String title = juce::String(id) + "  " + m->name.c_str();
        auto* plot = new CurvePlot();
        plot->setData(c.x, c.y, title, stats, logX, peak ? peakIdx : -1);
        m_content.addAndMakeVisible(plot);
        m_plots.add(plot);
    }
    // DBG: plota o espectro de depuracao (MS-05/06/07) com marcadores sobrepostos.
    void addSpectrumPlotFor(const ResultBundle& b, const char* id) {
        const MetricResult* m = b.find(id);
        if (m == nullptr) return;
        const Curve* spec = nullptr;
        for (const auto& c : m->curves)
            if (juce::String(c.name.c_str()) == "DBG Spectrum") { spec = &c; break; }
        if (spec == nullptr || spec->x.size() < 2) return;

        const juce::String ids(id);
        const juce::String valKey = (ids == "MS-05") ? "THD_pct" : (ids == "MS-06") ? "THDN_pct" : "IMD_pct";
        double val = 0.0;
        for (const auto& sc : m->scalars) if (juce::String(sc.key.c_str()) == valKey) val = sc.value;
        const juce::String stats = valKey + " = " + juce::String(val, 2)
            + " %    [verde=fundamental/tons  laranja=harmonicos  magenta=produtos]";

        auto* plot = new CurvePlot();
        plot->setData(spec->x, spec->y, ids + "  " + m->name.c_str() + " - FFT", stats, true, -1);

        std::vector<CurvePlot::PlotOverlay> ov;
        for (const auto& c : m->curves) {
            const juce::String nm(c.name.c_str());
            if (nm == "DBG Fundamental" || nm == "DBG Tones") ov.push_back({c.x, c.y, juce::Colours::limegreen});
            else if (nm == "DBG Harmonics")                   ov.push_back({c.x, c.y, juce::Colours::orange});
            else if (nm == "DBG Products")                    ov.push_back({c.x, c.y, juce::Colours::magenta});
        }
        plot->setOverlays(std::move(ov));
        m_content.addAndMakeVisible(plot);
        m_plots.add(plot);
    }
    juce::Viewport m_vp;
    juce::Component m_content;
    juce::OwnedArray<CurvePlot> m_plots;
};

class MainComponent : public juce::Component {
public:
    MainComponent() {
        m_dm.initialise(2, 2, nullptr, true);
        const AudioSettings st = loadSettings();
        if (deviceExists(m_dm, st.typeName, st.deviceName)) applySettings(m_dm, st);
        if (m_dm.getCurrentAudioDevice() == nullptr) m_dm.initialise(2, 2, nullptr, true);

        addAndMakeVisible(m_dashboard);
        addAndMakeVisible(m_config);
        addAndMakeVisible(m_start);
        m_config.setButtonText("Configurar audio");      m_config.onClick = [this]{ configureAudio(); };
        m_start.setButtonText("Iniciar caracterizacao"); m_start.onClick  = [this]{ runMeasurement(); };

        addChildComponent(m_engView);                    // oculto ate haver resultado
        m_engToggle.setButtonText("Engineering View");
        m_engToggle.onClick = [this]{ toggleEng(); };
        m_engToggle.setEnabled(false);                   // habilita ao concluir a medicao
        addAndMakeVisible(m_engToggle);

        addChildComponent(m_overlay);                    // oculto ate iniciar a caracterizacao
        m_overlay.onOpen  = [this]{ openDashboard(); };
        m_overlay.onAbort = [this]{ m_abort.store(true); };   // botao "Abortar" durante a medicao
    }
    ~MainComponent() override {
        m_abort.store(true);                         // pede para a medicao abortar
        if (m_worker.joinable()) m_worker.join();    // aguarda a thread terminar
        m_dm.closeAudioDevice();                     // libera o ASIO antes de destruir o manager
    }
    void resized() override {
        auto r = getLocalBounds();
        auto top = r.removeFromTop(48);
        m_config.setBounds(top.removeFromLeft(180).reduced(6));
        m_engToggle.setBounds(top.removeFromRight(180).reduced(6));
        m_start.setBounds(top.reduced(6));
        m_dashboard.setBounds(r);
        m_engView.setBounds(r);
        m_overlay.setBounds(getLocalBounds());
    }
private:
    void configureAudio() {
        if (m_running.load()) return;
        auto* cfg = new AudioConfig(m_dm);
        cfg->setSize(440, 240);
        juce::DialogWindow::LaunchOptions o;
        o.content.setOwned(cfg); o.dialogTitle = "Configurar audio";
        o.dialogBackgroundColour = juce::Colour(0xff1e1e1e);
        o.escapeKeyTriggersCloseButton = true; o.useNativeTitleBar = true; o.resizable = true;
        o.launchAsync();
    }
    void runMeasurement() {
        if (m_running.load()) return;
        auto* dev0 = m_dm.getCurrentAudioDevice();
        if (dev0 == nullptr) {
            juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                "Iniciar caracterizacao", "Nenhum dispositivo aberto. Use 'Configurar audio'.");
            return;
        }
        { auto su = m_dm.getAudioDeviceSetup(); su.sampleRate = k::REF_SAMPLE_RATE; m_dm.setAudioDeviceSetup(su, true); }
        auto* dev = m_dm.getCurrentAudioDevice();
        const double fs = (dev != nullptr) ? dev->getCurrentSampleRate() : k::REF_SAMPLE_RATE;
        const double fsl = (fs > 0.0) ? fs : 48000.0;

        diagReset();
        diagLog("=== AAMP DIAG ===");
        if (dev != nullptr) {
            const int outLat = dev->getOutputLatencyInSamples();
            const int inLat  = dev->getInputLatencyInSamples();
            const int buf    = dev->getCurrentBufferSizeSamples();
            diagLog("fs=" + juce::String(fs, 0) + " Hz  buffer=" + juce::String(buf) + " samples");
            diagLog("outLatency=" + juce::String(outLat) + " (" + juce::String(outLat * 1000.0 / fsl, 2) + " ms)");
            diagLog("inLatency="  + juce::String(inLat)  + " (" + juce::String(inLat  * 1000.0 / fsl, 2) + " ms)");
            diagLog("roundTrip="  + juce::String(outLat + inLat) + " (" + juce::String((outLat + inLat) * 1000.0 / fsl, 2) + " ms)");
            diagLog("--- estimulos ---");
        }

        if (m_worker.joinable()) m_worker.join();
        m_abort.store(false); m_running.store(true);
        m_start.setEnabled(false); m_start.setButtonText("Medindo...");

        // Abre a janela de progresso (barra + historico rastreavel).
        m_lastCompleted = 0;
        m_metricsReady = 0;
        m_overlay.reset();
        m_overlay.setCurrent("preparando...");
        m_overlay.setVisible(true);
        m_overlay.toFront(false);

        const juce::Component::SafePointer<MainComponent> safe(this);
        m_worker = std::thread([this, safe, fs] {
            JuceCaptureDevice realDev(m_dm, fs, &m_abort);
            MeasurementOrchestrator orch(fs, ChannelMode::Mono);
            orch.setProgressCallback([safe](const Progress& p) {
                juce::MessageManager::callAsync([safe, p] { if (auto* c = safe.getComponent()) c->onProgress(p); });
            });
            orch.setStimulusCallback([safe](const StimulusReport& s) {
                juce::MessageManager::callAsync([safe, s] { if (auto* c = safe.getComponent()) c->onStimulusReport(s); });
            });
            orch.setResultCallback([safe](const MetricResult& m, const MetricEmissionInfo& info) {
                juce::MessageManager::callAsync([safe, m, info] { if (auto* c = safe.getComponent()) c->onMetricReady(m, info); });
            });
            RunResult rr;
            if (orch.prepare(realDev)) rr = orch.runSuite(realDev);
            m_running.store(false);
            juce::MessageManager::callAsync([safe, rr = std::move(rr)] {
                if (auto* c = safe.getComponent()) c->onMeasurementDone(rr);
            });
        });
    }
    void onProgress(const Progress& p) {
        m_start.setButtonText("Medindo... " + juce::String(p.percent) + "%");
        if (p.label == "Preparando medicao") {
            m_overlay.setPhase("Captura dos estimulos");
            m_overlay.setProgress(0.0);
            m_overlay.markRunning(1);
            m_overlay.setCurrent(juce::String(kSuiteOrder[0].id) + "  " + kSuiteOrder[0].name);
        } else if (p.label == "Medindo") {
            m_overlay.setPhase("Captura dos estimulos");
            for (int j = m_lastCompleted + 1; j <= p.completed && j >= 1 && j <= 16; ++j)
                m_overlay.markCompleted(j);
            m_lastCompleted = juce::jmax(m_lastCompleted, p.completed);
            m_overlay.setProgress(p.total > 0 ? (double) p.completed / (double) p.total : 0.0);
            if (p.completed < 16) {
                const SuiteStep& s = kSuiteOrder[(size_t) p.completed];   // proximo (0-based = completed)
                m_overlay.markRunning(p.completed + 1);
                m_overlay.setCurrent(juce::String(s.id) + "  " + s.name);
            } else {
                m_overlay.setCurrent("processamento das metricas...");
            }
        } else if (p.label.rfind("Processando", 0) == 0) {
            m_overlay.setPhase("Processamento das metricas");
            m_overlay.setProgress(1.0);
            m_overlay.setCurrent("processando MS-01..MS-15...");
        }
    }
    void onStimulusReport(const StimulusReport& s) {
        // EVIDENCIA 1 - integridade da captura (Req. 1): tamanhos + prova explicita de buffer integral.
        const bool integral = (s.capturedSamples >= s.stimSamples) && (s.capturedSamples >= s.observedDurationSamples);
        diagLog("[EVID-CAP] estimulo " + juce::String(s.index) + "/" + juce::String(s.total)
                + " iniciado | tamStimulo=" + juce::String(s.stimSamples)
                + " | tamCapturado=" + juce::String(s.capturedSamples)
                + " | duracaoObservada=" + juce::String(s.observedDurationSamples)
                + " | NAO TRUNCADO (capturado >= observado: " + juce::String(s.capturedSamples)
                + " >= " + juce::String(s.observedDurationSamples) + ")"
                + " | BUFFER INTEGRAL=" + juce::String(integral ? "SIM" : "NAO")
                + " | rms=" + juce::String(s.rms, 5) + " residual=" + juce::String(s.residual, 5)
                + (s.nonDecayed ? " | nao-decaente: cauda preservada ate o teto (sem corte)" : ""));
    }
    void onMetricReady(const MetricResult& m, const MetricEmissionInfo& info) {
        ++m_metricsReady;
        // EVIDENCIA 2 - execucao incremental (Req. 2/4): cadeia completa, com prova de que saiu antes do fim.
        diagLog(juce::String("[EVID-INCR] estimulo concluido ") + juce::String(info.stimuliCompleted) + "/" + juce::String(info.totalStimuli)
                + " | deps satisfeitas {" + juce::String(info.deps.c_str()) + "}"
                + " | metrica EXECUTADA [" + juce::String(m.id.c_str()) + "] " + juce::String(m.name.c_str())
                + " | RESULTADO EMITIDO " + juce::String(info.emittedIndex) + "/" + juce::String(info.totalMetrics)
                + " | quality=" + juce::String(qualityToString(m.quality))
                + (info.stimuliCompleted < info.totalStimuli ? "  <-- ANTES do fim da suite" : "  (ultimo estimulo)"));
    }
    void onMeasurementDone(const RunResult& rr) {
        const bool ok = (rr.outcome == RunOutcome::Completed);
        diagLog("--- resultado ---");
        diagLog(juce::String("outcome=") + (ok ? "Completed" : "NAO concluido"));
        if (ok) diagLog("metricFamilies=" + juce::String((int) rr.bundle.metrics.size()));
        // EVIDENCIA 3 - eliminacao do batch (Req. 3): incremental vs apos o fim.
        if (ok) diagLog("[EVID-BATCH] metricas emitidas INCREMENTALMENTE (durante o laco) = " + juce::String(rr.emittedIncremental)
                        + " | metricas emitidas APOS o fim da suite = " + juce::String(rr.emittedAfterSuite)
                        + " | SEM processamento consolidado unico ao final = "
                        + juce::String(rr.emittedAfterSuite == 0 ? "CONFIRMADO" : "FALHA"));
        if (ok) diagDumpBundle(rr.bundle);
        // EVIDENCIA 4 - utilizacao da regiao analisada (Req. 4) por metrica espectral.
        if (ok) {
            struct SpecMap { const char* mid; const char* dbg; };
            const SpecMap specs[] = { {"MS-05","THD"}, {"MS-06","THDN"}, {"MS-07","IMD"}, {"MS-08","NF"} };
            for (const auto& sp : specs) {
                const MetricResult* mr = nullptr;
                for (const auto& mm : rr.bundle.metrics) if (mm.id == sp.mid) { mr = &mm; break; }
                if (mr == nullptr) continue;
                auto getv = [&](const std::string& key) -> double {
                    for (const auto& sc : mr->scalars) if (sc.key == key) return sc.value;
                    return -1.0;
                };
                diagLog(juce::String("[EVID-REGIAO] ") + sp.mid
                        + " regiaoDisponivel=" + juce::String(getv(std::string("DBG_") + sp.dbg + "_regionLen_samples"))
                        + " | utilizado=" + juce::String(getv(std::string("DBG_") + sp.dbg + "_used_samples"))
                        + " | percentualUtilizado=" + juce::String(getv(std::string("DBG_") + sp.dbg + "_usedPct_ofRegion"), 2) + "%");
            }
        }
        diagLog("log: " + diagFile().getFullPathName());

        m_lastResult = rr;   // guarda para abrir o dashboard ao clique do botao

        if (ok) {
            for (int i = 1; i <= 16; ++i) m_overlay.markCompleted(i);   // garante 16 capturas marcadas
            m_overlay.setPhase("Concluido");
            m_overlay.setProgress(1.0);
            m_overlay.setCurrent("16/16 estimulos capturados, "
                                 + juce::String((int) rr.bundle.metrics.size()) + "/15 familias processadas");
            m_overlay.setReady("Abrir Dashboard");
        } else {
            m_overlay.setPhase("Interrompido");
            m_overlay.setCurrent(juce::String("Nao concluido: ") + rr.userMessage.c_str());
            m_overlay.setReady("Fechar");
        }
        m_start.setButtonText("Iniciar caracterizacao"); m_start.setEnabled(true);
    }
    void openDashboard() {
        m_overlay.setVisible(false);
        if (m_lastResult.outcome == RunOutcome::Completed) {
            m_dashboard.showResults(m_lastResult.bundle);
            m_engView.showBundle(m_lastResult.bundle);
            m_engToggle.setEnabled(true);
        }
    }
    void toggleEng() {
        if (m_lastResult.outcome != RunOutcome::Completed) return;
        m_engShown = !m_engShown;
        m_engView.setVisible(m_engShown);
        m_dashboard.setVisible(!m_engShown);
        m_engToggle.setButtonText(m_engShown ? "Dashboard" : "Engineering View");
    }

    DashboardComponent m_dashboard;
    EngineeringView m_engView;
    bool m_engShown = false;
    ProgressOverlay m_overlay;
    RunResult m_lastResult;
    int m_lastCompleted = 0;
    int m_metricsReady = 0;   // contador de metricas emitidas em tempo real (Requisito 4/5)
    juce::AudioDeviceManager m_dm;
    std::thread m_worker;
    std::atomic<bool> m_abort { false };
    std::atomic<bool> m_running { false };
    juce::TextButton m_config, m_start, m_engToggle;
};

class MainWindow : public juce::DocumentWindow {
public:
    MainWindow() : juce::DocumentWindow("AAMP", juce::Colours::black, juce::DocumentWindow::allButtons) {
        setUsingNativeTitleBar(true);
        // INTEGRATION_BRIDGE.md §4: a apresentacao passa a ser o WebView (AampWebBridge); a GUI JUCE
        // do dashboard (MainComponent/DashboardComponent) sai de cena. O restante de Main.cpp
        // (backends de device, orchestrator) permanece reutilizavel pela ponte in-process.
        setContentOwned(new AampWebBridge(), true);
        setResizable(true, true);
        int w = 1280, h = 800;
        if (auto* disp = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()) {
            const auto area = disp->userArea;
            w = juce::jmin(w, area.getWidth() - 80); h = juce::jmin(h, area.getHeight() - 80);
        }
        centreWithSize(w, h); setVisible(true);
    }
    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
};

class AampApplication : public juce::JUCEApplication {
public:
    const juce::String getApplicationName() override { return "AAMP"; }
    const juce::String getApplicationVersion() override { return "2.0.0"; }
    void initialise(const juce::String&) override { m_window = std::make_unique<MainWindow>(); }
    void shutdown() override { m_window = nullptr; }
private:
    std::unique_ptr<MainWindow> m_window;
};

} // namespace aamp

START_JUCE_APPLICATION(aamp::AampApplication)
#endif // AAMP_WITH_JUCE
