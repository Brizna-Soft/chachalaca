cmake_minimum_required(VERSION 3.25)

# --- vcpkg: triplet propio (todo estático, solo Release) ---
set(VCPKG_OVERLAY_TRIPLETS "${CMAKE_CURRENT_SOURCE_DIR}/triplets" CACHE STRING "")
set(VCPKG_TARGET_TRIPLET "x64-windows-static-rel" CACHE STRING "")
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    if(DEFINED ENV{VCPKG_ROOT})
        file(TO_CMAKE_PATH "$ENV{VCPKG_ROOT}" _vcpkg)
    else()
        set(_vcpkg "${CMAKE_CURRENT_SOURCE_DIR}/vendor/vcpkg")
    endif()
    set(CMAKE_TOOLCHAIN_FILE "${_vcpkg}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
endif()

# CRT estático, igual que las bibliotecas de vcpkg. Solo Release: el triplet no compila versiones Debug.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CONFIGURATION_TYPES "Release" CACHE STRING "" FORCE)

file(STRINGS version.txt APP_VERSION LIMIT_COUNT 1)
project(Chachalaca VERSION ${APP_VERSION} LANGUAGES C CXX RC)
if(NOT APP_VERSION_TEXTO)
    set(APP_VERSION_TEXTO "${APP_VERSION}")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(wxWidgets CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
# miniaudio y signalsmith-stretch son solo encabezados
find_path(MINIAUDIO_INCLUDE_DIRS "miniaudio.h" REQUIRED)
find_path(SIGNALSMITH_INCLUDE_DIRS "signalsmith-stretch/signalsmith-stretch.h" REQUIRED)
# SpeexDSP (quitar ruido): vcpkg instala la biblioteca sin archivo de CMake
find_path(SPEEXDSP_INCLUDE_DIRS "speex/speex_preprocess.h" REQUIRED)
find_library(SPEEXDSP_LIBRARY NAMES speexdsp libspeexdsp REQUIRED)

configure_file(src/version.h.in "${CMAKE_CURRENT_BINARY_DIR}/generado/version.h" @ONLY)

set(OPCIONES_CXX /utf-8 /W4 /permissive- /EHsc /Zc:__cplusplus /guard:cf)
# WIN32_LEAN_AND_MEAN: windows.h sin winsock.h (choca con el winsock2.h de wx)
set(DEFINICIONES UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _WIN32_WINNT=0x0A00)

file(GLOB FUENTES CONFIGURE_DEPENDS src/*.cpp src/*.hpp src/*.c src/*.h)
add_executable(Chachalaca WIN32 ${FUENTES} res/app.rc)
target_include_directories(Chachalaca PRIVATE src "${CMAKE_CURRENT_BINARY_DIR}/generado"
    ${MINIAUDIO_INCLUDE_DIRS} ${SIGNALSMITH_INCLUDE_DIRS} ${SPEEXDSP_INCLUDE_DIRS})
target_compile_definitions(Chachalaca PRIVATE ${DEFINICIONES})
target_compile_options(Chachalaca PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${OPCIONES_CXX}> $<$<COMPILE_LANGUAGE:C>:/utf-8 /guard:cf>)
target_link_options(Chachalaca PRIVATE /MANIFEST:NO /guard:cf)
target_link_libraries(Chachalaca PRIVATE
    wx::core wx::base
    nlohmann_json::nlohmann_json
    ${SPEEXDSP_LIBRARY}
    shlwapi winhttp wintrust crypt32 bcrypt)

# --- Hace los sonidos que trae Chachalaca (los inventa desde cero; no se publica el .exe, sí los sonidos) ---
add_executable(generar-sonidos herramientas/generar-sonidos.cpp)
target_compile_definitions(generar-sonidos PRIVATE ${DEFINICIONES})
target_compile_options(generar-sonidos PRIVATE ${OPCIONES_CXX})

# --- Prueba sin ventana: graba de una entrada para comprobar el micrófono virtual (no se publica) ---
add_executable(prueba-cable pruebas/prueba-cable.cpp src/miniaudio.c src/instalador.cpp src/util.cpp)
target_include_directories(prueba-cable PRIVATE src ${MINIAUDIO_INCLUDE_DIRS})
target_compile_definitions(prueba-cable PRIVATE ${DEFINICIONES})
target_compile_options(prueba-cable PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${OPCIONES_CXX}> $<$<COMPILE_LANGUAGE:C>:/utf-8 /guard:cf>)
target_link_options(prueba-cable PRIVATE /guard:cf)
target_link_libraries(prueba-cable PRIVATE wintrust crypt32 bcrypt)
