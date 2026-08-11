// AAMP - src/organization/Project.h
// Project — primeira entidade da CAMADA DE ORGANIZAÇÃO (Etapa 1 da implementação).
//
// Materializa EXCLUSIVAMENTE as responsabilidades demonstradas pela auditoria conceitual do
// Projeto (marco [D-PROJETO-COMO-UNIDADE-MÁXIMA]). O Projeto é a unidade MÁXIMA de organização:
// ele confere coerência a um conjunto de Missões sob um propósito abrangente e é o lar do
// patrimônio que transcende qualquer Missão individual. Nada além disso.
//
// O Projeto NÃO EXECUTA. Por decisão arquitetural ([D-EXECUÇÃO-vs-GOVERNANÇA]), esta entidade
// não planeja, não percorre missões, não decide prioridade, não resolve conflitos e não invoca
// IA. Essas responsabilidades pertencem à futura Governança e às camadas de Estratégia/Execução.
// Por isso este cabeçalho define um AGREGADO DE DADOS PURO — sem métodos de comportamento — no
// mesmo padrão do modelo de domínio EvidencePackage.h (Princípio 4: dependência apenas da STL).
//
// Fronteira de posse (auditoria da identidade do Projeto):
//   - O Projeto tem IDENTIDADE PRÓPRIA e intrínseca (não deriva das Missões).
//   - O Projeto REFERENCIA suas Missões por identidade; NUNCA copia dados da Missão.
//   - Objetivo único, plano, execução, memória e artefatos INDIVIDUAIS pertencem à Missão.
//   - Propósito abrangente, patrimônio (conhecimento promovido + artefatos coletivos),
//     histórico inter-missão e contexto do Projeto pertencem AO PROJETO — os quatro elementos
//     "sem-lar" que forçaram a existência desta entidade.
//
// Escopo desta etapa: apenas as ESTRUTURAS. Não há promoção de conhecimento, persistência,
// deduplicação, indexação, herança/fusão de contexto, eventos, timeline nem lógica de transição
// de ciclo de vida. Todas essas decisões pertencem a etapas futuras e são deixadas explícitas
// como pendências deferidas nos comentários abaixo.
#pragma once
#include <string>
#include <vector>

namespace aamp {
namespace organization {

// -----------------------------------------------------------------------------------------------
// Ciclo de vida do Projeto (auditoria: estado próprio, distinto do estado de execução da Missão).
// Apenas a REPRESENTAÇÃO dos estados arquiteturais. Sem lógica de transição, sem máquina de
// estados, sem validação de mudança — essas decisões pertencem à futura Governança.
enum class LifecycleState {
    Draft,      // propósito declarado; o Projeto pode existir antes da primeira Missão
    Active,     // sob condução (uma ou mais Missões vivas)
    Paused,     // suspenso, estado preservado para retomada
    Archived,   // encerrado/inerte, preservado para referência
    Discarded   // abandonado, sem valor de retenção
};

// -----------------------------------------------------------------------------------------------
// Referência a uma Missão POR IDENTIDADE. O Projeto mantém apenas referências; a Missão detém
// sua própria identidade intrínseca, objetivo, plano, execução, memória e artefatos individuais.
// "Referência, não cópia": esta estrutura carrega somente o identificador, jamais dados da Missão.
// (A entidade Missão será implementada em etapa posterior; a referência é independente dela.)
struct MissionRef {
    std::string missionId;   // identidade intrínseca da Missão referenciada
};

// -----------------------------------------------------------------------------------------------
// PATRIMÔNIO — estruturas deferidas.
// A auditoria demonstrou a EXISTÊNCIA destas coleções (conhecimento promovido, artefatos
// coletivos, histórico inter-missão) como elementos "sem-lar" na Missão. A ESTRUTURA INTERNA de
// cada elemento NÃO foi demonstrada por nenhuma auditoria. Por [D-IMPLEMENTAÇÃO-POR-EVIDÊNCIA],
// nenhum atributo é inventado por conveniência: os tipos de elemento nascem como marcadores
// vazios e ganharão seus campos auditados quando a promoção/os artefatos/o histórico forem
// implementados. As coleções iniciam vazias (ver critério de aceitação), portanto o interior
// destes marcadores nunca é exercido nesta etapa.

// Elemento de conhecimento promovido ao patrimônio do Projeto (estrutura interna deferida).
struct PromotedKnowledge {};

// Artefato coletivo — saída que sintetiza mais de uma Missão (estrutura interna deferida).
struct SharedArtifact {};

// Entrada do histórico INTER-missão do Projeto (estrutura interna deferida).
struct HistoryEntry {};

// -----------------------------------------------------------------------------------------------
// Contexto do Projeto — entidade DISTINTA do Contexto da Missão (auditoria: contextos aninhados).
// Nasce vazio. Sem herança, sem fusão, sem resolução — apenas existe como entidade separada. Sua
// estrutura interna é deferida à etapa que definir o patrimônio compartilhado reutilizável.
struct ProjectContext {};

// -----------------------------------------------------------------------------------------------
// Project — o agregado. Pode existir com ZERO Missões, ZERO conhecimento, ZERO artefatos e ZERO
// histórico, e ainda assim possuir identidade, propósito e ciclo de vida próprios (critério de
// aceitação arquitetural desta etapa).
struct Project {
    // Identidade e propósito (o núcleo mínimo demonstrado: identidade + propósito abrangente).
    std::string    id;                                  // identidade própria e intrínseca
    std::string    title;                               // rótulo legível (metadado, não identidade)
    std::string    purpose;                             // propósito abrangente, NÃO-executável
    LifecycleState state = LifecycleState::Draft;       // ciclo de vida próprio; nasce em Draft

    // Coleção de Missões — apenas referências por identidade (nunca cópias de dados da Missão).
    std::vector<MissionRef> missions;                   // inicia vazia

    // Patrimônio coletivo — coleções que iniciam vazias; sem promoção automática nesta etapa.
    std::vector<PromotedKnowledge> sharedKnowledge;     // conhecimento promovido (vazio)
    std::vector<SharedArtifact>    sharedArtifacts;     // artefatos coletivos (vazio)

    // Histórico inter-missão — coleção que inicia vazia; sem eventos/timeline nesta etapa.
    std::vector<HistoryEntry> history;                  // inicia vazio

    // Contexto do Projeto — entidade distinta do Contexto da Missão; nasce vazia.
    ProjectContext context;
};

} // namespace organization
} // namespace aamp
