// AAMP - src/analysis/Windows.cpp
#include "Windows.h"
#include <cmath>

namespace aamp {

static const double PI = 3.14159265358979323846;

std::vector<double> makeWindow(Window w, int n) {
    std::vector<double> win(static_cast<std::size_t>(n), 1.0);
    if (n <= 1) return win;
    const double N1 = static_cast<double>(n - 1);
    for (int i = 0; i < n; ++i) {
        const double r = static_cast<double>(i) / N1; // 0..1
        double v = 1.0;
        switch (w) {
            case Window::Rectangular:
                v = 1.0; break;
            case Window::Hann:
                v = 0.5 - 0.5 * std::cos(2.0 * PI * r); break;
            case Window::Hamming:
                v = 0.54 - 0.46 * std::cos(2.0 * PI * r); break;
            case Window::Blackman:
                v = 0.42 - 0.5 * std::cos(2.0 * PI * r) + 0.08 * std::cos(4.0 * PI * r); break;
            case Window::FlatTop: {
                // Coeficientes SRS Flat Top (5 termos).
                const double a0 = 0.21557895, a1 = 0.41663158, a2 = 0.277263158,
                             a3 = 0.083578947, a4 = 0.006947368;
                v = a0 - a1 * std::cos(2.0 * PI * r) + a2 * std::cos(4.0 * PI * r)
                      - a3 * std::cos(6.0 * PI * r) + a4 * std::cos(8.0 * PI * r);
                break;
            }
            case Window::BlackmanHarris4: {
                // Blackman-Harris 4-termos (DOC-03 MS-05/06/07).
                const double a0 = 0.35875, a1 = 0.48829, a2 = 0.14128, a3 = 0.01168;
                v = a0 - a1 * std::cos(2.0 * PI * r) + a2 * std::cos(4.0 * PI * r)
                      - a3 * std::cos(6.0 * PI * r);
                break;
            }
        }
        win[static_cast<std::size_t>(i)] = v;
    }
    return win;
}

double windowCoherentGain(const std::vector<double>& win) {
    double s = 0.0;
    for (double v : win) s += v;
    return s;
}

double windowPowerGain(const std::vector<double>& win) {
    double s = 0.0;
    for (double v : win) s += v * v;
    return s;
}

double windowEnbwBins(const std::vector<double>& win) {
    if (win.empty()) return 1.0;
    const double s1 = windowCoherentGain(win);
    if (s1 == 0.0) return 1.0;
    const double s2 = windowPowerGain(win);
    return static_cast<double>(win.size()) * s2 / (s1 * s1);
}

} // namespace aamp
