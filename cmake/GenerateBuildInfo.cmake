# AAMP - cmake/GenerateBuildInfo.cmake
# Gera <build>/generated/AampBuildInfo.h com a proveniencia do binario.
#
# Executado em modo script (cmake -P) a CADA build, para que o commit registrado no
# diag_log.txt seja o do binario que esta rodando -- e nao o da ultima configuracao do
# CMake. O arquivo so e SUBSTITUIDO quando o conteudo muda (copy_if_different), portanto
# uma mudanca de HEAD recompila DiagLog.cpp e um build repetido no mesmo commit nao
# recompila nada.
#
# Entradas (via -D): AAMP_BUILD_INFO_OUT, AAMP_SRC, AAMP_VERSION, AAMP_CONFIG

if(NOT DEFINED AAMP_BUILD_INFO_OUT OR NOT DEFINED AAMP_SRC)
  message(FATAL_ERROR "GenerateBuildInfo.cmake: AAMP_BUILD_INFO_OUT e AAMP_SRC sao obrigatorios")
endif()

set(_commit_full  "unknown")
set(_commit_short "unknown")
set(_branch       "unknown")
set(_dirty        "unknown")

find_package(Git QUIET)
if(GIT_FOUND)
  execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
                  WORKING_DIRECTORY "${AAMP_SRC}"
                  OUTPUT_VARIABLE _o RESULT_VARIABLE _r
                  OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  if(_r EQUAL 0 AND NOT _o STREQUAL "")
    set(_commit_full "${_o}")
    string(SUBSTRING "${_o}" 0 7 _commit_short)
  endif()

  # --abbrev-ref devolve "HEAD" em detached HEAD; preservamos esse valor literal em vez de
  # inventar um nome de branch. O commit completo continua sendo a evidencia principal.
  execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --abbrev-ref HEAD
                  WORKING_DIRECTORY "${AAMP_SRC}"
                  OUTPUT_VARIABLE _o RESULT_VARIABLE _r
                  OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  if(_r EQUAL 0 AND NOT _o STREQUAL "")
    if(_o STREQUAL "HEAD")
      set(_branch "detached")
    else()
      set(_branch "${_o}")
    endif()
  endif()

  execute_process(COMMAND "${GIT_EXECUTABLE}" status --porcelain --untracked-files=no
                  WORKING_DIRECTORY "${AAMP_SRC}"
                  OUTPUT_VARIABLE _o RESULT_VARIABLE _r
                  OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
  if(_r EQUAL 0)
    if(_o STREQUAL "")
      set(_dirty "false")
    else()
      set(_dirty "true")
    endif()
  endif()
endif()

if(NOT DEFINED AAMP_CONFIG OR AAMP_CONFIG STREQUAL "")
  set(AAMP_CONFIG "unknown")
endif()
if(NOT DEFINED AAMP_VERSION OR AAMP_VERSION STREQUAL "")
  set(AAMP_VERSION "unknown")
endif()

string(TIMESTAMP _build_time "%Y-%m-%d %H:%M:%S" UTC)
set(_build_time "${_build_time} UTC")

set(_content
"// AAMP - AampBuildInfo.h  (GERADO AUTOMATICAMENTE -- NAO EDITAR, NAO VERSIONAR)
// Produzido por cmake/GenerateBuildInfo.cmake a cada build.
#pragma once
#define AAMP_BUILD_VERSION      \"${AAMP_VERSION}\"
#define AAMP_BUILD_CONFIG       \"${AAMP_CONFIG}\"
#define AAMP_BUILD_TIME         \"${_build_time}\"
#define AAMP_BUILD_COMMIT_FULL  \"${_commit_full}\"
#define AAMP_BUILD_COMMIT_SHORT \"${_commit_short}\"
#define AAMP_BUILD_BRANCH       \"${_branch}\"
#define AAMP_BUILD_GIT_DIRTY    \"${_dirty}\"
")

# BuildTime muda a cada execucao; se ele entrasse na comparacao, todo build recompilaria.
# Compara-se apenas a proveniencia (commit/branch/dirty/config/version): o arquivo so e
# reescrito quando algo relevante muda.
set(_probe "${AAMP_BUILD_INFO_OUT}.provenance")
set(_prov "${_commit_full}|${_commit_short}|${_branch}|${_dirty}|${AAMP_CONFIG}|${AAMP_VERSION}")
set(_old "")
if(EXISTS "${_probe}")
  file(READ "${_probe}" _old)
endif()
if(NOT EXISTS "${AAMP_BUILD_INFO_OUT}" OR NOT _old STREQUAL "${_prov}")
  get_filename_component(_dir "${AAMP_BUILD_INFO_OUT}" DIRECTORY)
  file(MAKE_DIRECTORY "${_dir}")
  file(WRITE "${AAMP_BUILD_INFO_OUT}" "${_content}")
  file(WRITE "${_probe}" "${_prov}")
  message(STATUS "AAMP build info: ${_commit_short} (${_branch}) dirty=${_dirty} config=${AAMP_CONFIG}")
endif()
