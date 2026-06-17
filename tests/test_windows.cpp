// AAMP - tests/test_windows.cpp
// Nucleo DSP - janelas (DOC-03 §4.1): Hann, Hamming, Blackman, Flat Top, Rectangular,
// Blackman-Harris 4-termos. Propriedades basicas e selecao por metrica.
#include "TestRunner.h"
#include "../src/analysis/Windows.h"
#include <cmath>

using namespace aamp;

TEST_CASE("DSP janelas: geracao e propriedades (DOC-03 §4.1)") {
    const int N = 1024;
    const Window all[6] = {Window::Rectangular, Window::Hann, Window::Hamming,
                           Window::Blackman, Window::FlatTop, Window::BlackmanHarris4};
    for (Window w : all) {
        auto win = makeWindow(w, N);
        CHECK(static_cast<int>(win.size()) == N);
        const double cg = windowCoherentGain(win);
        CHECK(cg > 0.0);                       // ganho coerente positivo
        // Rectangular = 1.0 em todas as amostras.
        if (w == Window::Rectangular) {
            bool allOne = true;
            for (double v : win) if (std::fabs(v - 1.0) > 1e-12) allOne = false;
            CHECK(allOne);
        }
        // Janelas com taper comecam perto de zero nas bordas.
        if (w == Window::Hann || w == Window::Blackman || w == Window::BlackmanHarris4) {
            CHECK(win.front() < 0.1);
            CHECK(win.back() < 0.1);
        }
    }
    // Caso degenerado N=1 nao quebra.
    auto w1 = makeWindow(Window::Hann, 1);
    CHECK(w1.size() == 1);
}
