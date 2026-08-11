// AAMP - src/organization/Mission.h
// Mission — unidade FUNDAMENTAL de execução da CAMADA DE ORGANIZAÇÃO (Etapa 2 da implementação).
//
// Materializa EXCLUSIVAMENTE as responsabilidades demonstradas pela auditoria conceitual da
// Missão ([D-DEFINIÇÃO-DE-MISSÃO]) e pelo marco [D-IDENTIDADE-LOCAL]: a Missão tem identidade e
// ciclo de vida PRÓPRIOS, referencia exatamente um Projeto e detém apenas patrimônio LOCAL. O
// Projeto organiza Missões; a Missão NUNCA incorpora responsabilidades do Projeto, e NENHUMA
// informação coletiva pertence à Missão.
//
// A Missão NÃO EXECUTA e NÃO COORDENA. Por decisão arquitetural, esta entidade não planeja, não
// prioriza, não coordena outras missões e não invoca IA — essas responsabilidades pertencem à
// Estratégia/Execução e à Governança. Por isso este cabeçalho define um AGREGADO DE DADOS PURO —
// sem métodos de comportamento — no mesmo padrão de Project.h e EvidencePackage.h (dependência
// apenas da STL).
//
// Fronteira de posse (auditoria + [D-IDENTIDADE-LOCAL]):
//   - PERTENCE À MISSÃO: identidade própria, referência ao Projeto (por identidade), objetivo
//     ÚNICO, estado próprio, contexto próprio, e patrimônio LOCAL (conhecimento, artefatos e
//     histórico DAQUELA missão).
//   - NÃO PERTENCE À MISSÃO: propósito abrangente, conhecimento promovido, artefatos coletivos,
//     histórico inter-missão, contexto do Projeto — tudo isso é do Projeto (Project.h). A Missão
//     não os declara nem os referencia.
//
// Escopo desta etapa: apenas as ESTRUTURAS. Sem promoção, compartilhamento, deduplicação,
// indexação, herança/fusão/sincronização de contexto, nem lógica de transição de ciclo de vida.
#pragma once
#include <string>
#include <vector>

namespace aamp {
namespace organization {

// -----------------------------------------------------------------------------------------------
// Ciclo de vida da Missão (auditoria: estado próprio, distinto do estado de organização do
// Projeto). Apenas a REPRESENTAÇÃO dos estados arquiteturais — sem lógica de transição, sem
// máquina de estados, sem validação de mudança.
enum class MissionState {
    Draft,      // objetivo declarado; ainda não pronta para condução
    Ready,      // pronta para ser conduzida
    Running,    // sob condução
    Paused,     // suspensa, estado preservado para retomada
    Completed,  // objetivo satisfeito (encerramento por sucesso)
    Archived,   // encerrada/inerte, preservada para referência
    Cancelled   // abandonada (encerramento sem sucesso)
};

// -----------------------------------------------------------------------------------------------
// PATRIMÔNIO LOCAL — estruturas deferidas.
// A auditoria demonstrou a EXISTÊNCIA das coleções locais da Missão (conhecimento, artefatos e
// histórico DAQUELA missão), mas NÃO a estrutura interna de cada elemento. Por
// [D-IMPLEMENTAÇÃO-POR-EVIDÊNCIA], nenhum atributo é inventado por conveniência: os tipos de
// elemento nascem como marcadores vazios e ganharão campos auditados em etapa futura. Estes tipos
// são DISTINTOS dos tipos coletivos do Projeto (PromotedKnowledge/SharedArtifact/HistoryEntry):
// escopo LOCAL da Missão, jamais coletivo ([D-IDENTIDADE-LOCAL]).

// Conhecimento adquirido por ESTA missão (mission-local; estrutura interna deferida).
struct MissionKnowledge {};

// Artefato produzido por ESTA missão (individual, não coletivo; estrutura interna deferida).
struct MissionArtifact {};

// Entrada do histórico INTRA-missão (trilha desta missão; estrutura interna deferida).
struct MissionHistoryEntry {};

// -----------------------------------------------------------------------------------------------
// Contexto da Missão — entidade DISTINTA e completamente separada do Contexto do Projeto
// (auditoria: contextos aninhados, porém entidades separadas). Nasce vazio. Sem herança, sem
// fusão, sem sincronização. Estrutura interna deferida.
struct MissionContext {};

// -----------------------------------------------------------------------------------------------
// Mission — o agregado. Pode existir com ZERO conhecimento, ZERO artefatos e ZERO histórico,
// desde que possua identidade, projeto associado, objetivo e ciclo de vida (critério de aceitação
// arquitetural desta etapa).
struct Mission {
    // Identidade própria e intrínseca.
    std::string id;

    // Referência OBRIGATÓRIA ao Projeto — SOMENTE por identidade. Nunca um ponteiro/handle para o
    // objeto Project (sem referência reversa), nunca cópia de dados do Projeto.
    std::string projectId;

    // Rótulo legível (metadado, não identidade) e OBJETIVO ÚNICO da missão.
    std::string title;
    // objective = exclusivamente o resultado pretendido DESTA missão. Nunca propósito do Projeto,
    // nunca estratégia, backlog ou planejamento.
    std::string objective;

    // Ciclo de vida próprio; nasce em Draft.
    MissionState state = MissionState::Draft;

    // Contexto próprio da Missão — entidade distinta do Contexto do Projeto; nasce vazia.
    MissionContext context;

    // Patrimônio LOCAL — coleções que iniciam vazias. Sem promoção/compartilhamento nesta etapa.
    std::vector<MissionKnowledge>    knowledge;   // conhecimento local (vazio)
    std::vector<MissionArtifact>     artifacts;   // artefatos individuais (vazio)
    std::vector<MissionHistoryEntry> history;     // histórico intra-missão (vazio)
};

} // namespace organization
} // namespace aamp
