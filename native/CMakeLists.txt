# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.24)
project(LagosystemBridge VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. ELIMINA DEPENDÊNCIA EXTERNA DO JSON (Zero apt-get/brew/vcpkg)
# Baixa o nlohmann_json no momento da configuração. 
# Sendo header-only, ele será "embutido" direto no seu executável C++.
include(FetchContent)
FetchContent_Declare(json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
  URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
)
FetchContent_MakeAvailable(json)

# 2. ELIMINA VCREDIST NO WINDOWS
# Configura o MSVC para usar a C Runtime Library Estática (/MT ou /MTd).
# Isso embute a biblioteca do C++ no .exe, removendo a necessidade do usuário ter o VCRedist instalado.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# 3. FONTES COMUNS
set(SOURCES 
    src/main.cpp 
    src/serial_port.cpp 
    src/device_control.cpp 
    src/scale_protocol.cpp
    src/native_consent.cpp
)

add_executable(lagosystem-bridge ${SOURCES})

# Adiciona arquivos extras apenas para exibição no Solution Explorer das IDEs
target_sources(lagosystem-bridge PRIVATE
  README.md
  LICENSE
  install-unix.sh
  install-windows.ps1
  CMakeLists.txt # É uma boa prática ter o próprio CMakeLists facilmente acessível na IDE
)

# 4. TRATAMENTO ESPECÍFICO POR SISTEMA OPERACIONAL
if(APPLE)
  enable_language(OBJCXX)
  # Apenas o .mm no macOS
  target_sources(lagosystem-bridge PRIVATE src/native_consent.mm)
  find_library(APPKIT_FRAMEWORK AppKit REQUIRED)
  target_link_libraries(lagosystem-bridge PRIVATE ${APPKIT_FRAMEWORK})
elseif(UNIX)
  # Apenas o .cpp no Linux
  target_sources(lagosystem-bridge PRIVATE src/native_consent.cpp)
  
  # ELIMINA DEPENDÊNCIA DO GCC/CLANG NO LINUX
  # Força o link estático do libstdc++ e libgcc para rodar em qualquer distro sem dependências
  target_link_options(lagosystem-bridge PRIVATE -static-libgcc -static-libstdc++)

  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(GTK3 gtk+-3.0 IMPORTED_TARGET QUIET)
    if(GTK3_FOUND)
      target_link_libraries(lagosystem-bridge PRIVATE PkgConfig::GTK3)
      target_compile_definitions(lagosystem-bridge PRIVATE LAGOSYSTEM_HAS_GTK3)
    endif()
  endif()
elseif(WIN32)
  enable_language(RC)
  # Apenas o .cpp no Windows
  target_sources(lagosystem-bridge PRIVATE src/native_consent.cpp resources/windows.rc)
  target_include_directories(lagosystem-bridge PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/resources")
  target_compile_definitions(lagosystem-bridge PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_link_libraries(lagosystem-bridge PRIVATE Comctl32)
endif()

target_link_libraries(lagosystem-bridge PRIVATE nlohmann_json::nlohmann_json)

# 5. TESTES OPIONAIS
option(LAGOSYSTEM_BRIDGE_BUILD_TESTS "Build local protocol fixtures without peripherals" OFF)
if(LAGOSYSTEM_BRIDGE_BUILD_TESTS)
  enable_testing()
  add_executable(bridge-scale-protocol-fixtures tests/scale_protocol_fixtures.cpp src/scale_protocol.cpp)
  
  if(MSVC)
      set_property(TARGET bridge-scale-protocol-fixtures PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  elseif(UNIX AND NOT APPLE)
      target_link_options(bridge-scale-protocol-fixtures PRIVATE -static-libgcc -static-libstdc++)
  endif()

  target_include_directories(bridge-scale-protocol-fixtures PRIVATE src)
  target_link_libraries(bridge-scale-protocol-fixtures PRIVATE nlohmann_json::nlohmann_json)
  add_test(NAME bridge-scale-protocol-fixtures COMMAND bridge-scale-protocol-fixtures)
endif()