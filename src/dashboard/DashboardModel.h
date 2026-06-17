// AAMP - src/dashboard/DashboardModel.h
// M-06 view-model (camada de DADOS do dashboard, testavel headless).
// Mapeia ResultBundle -> 6 secoes (DOC-05 §3 / DOC-02 FR-028) com estados por item
// (FR-031): Normal, Baixa-confiabilidade (badge), N/A, Vazio. NAO recalcula (DOC-08 §3.1):
// apenas le os escalares ja produzidos por M-05. O JUCE Component renderiza este modelo.
#pragma once
#include <string>
#include <vector>
#include "../common/ResultBundle.h"

namespace aamp {

enum class ItemState { Normal, LowConfidence, NotApplicable, Empty };

const char* itemStateName(ItemState s);

struct DashboardItem {
    std::string label;
    std::string value;     // formatado (3 casas) ou "N/A" ou "" (vazio)
    std::string unit;
    ItemState state = ItemState::Empty;
};

struct DashboardSection {
    std::string title;
    std::vector<DashboardItem> items;
};

struct DashboardModel {
    std::vector<DashboardSection> sections;  // exatamente 6 (DOC-05 §3)
    bool isEmpty = true;                      // estado vazio (FR-031) quando sem metricas
};

// Constroi o modelo de 6 secoes a partir do bundle. Bundle vazio -> isEmpty=true (itens Empty).
DashboardModel buildDashboardModel(const ResultBundle& bundle);

} // namespace aamp
