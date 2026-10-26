cmake_minimum_required(VERSION 3.21)
project(Constancia VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Sql Widgets Multimedia Network)
qt_standard_project_setup()

# ---------------------------------------------------------------------------
# Núcleo: datos, motor de sesión y estadísticas (sin interfaz, testeable)
# ---------------------------------------------------------------------------
add_library(constancia_core STATIC
    src/core/Types.h
    src/core/Format.h          src/core/Format.cpp
    src/core/FolderTree.h      src/core/FolderTree.cpp
    src/core/Database.h        src/core/Database.cpp
    src/core/Stats.h           src/core/Stats.cpp
    src/core/SessionController.h src/core/SessionController.cpp
)
target_include_directories(constancia_core PUBLIC src)
target_link_libraries(constancia_core PUBLIC Qt6::Core Qt6::Sql)

# ---------------------------------------------------------------------------
# Aplicación
# ---------------------------------------------------------------------------
qt_add_executable(Constancia WIN32
    src/main.cpp
    src/ui/Theme.h             src/ui/Theme.cpp
    src/ui/Icons.h             src/ui/Icons.cpp
    src/ui/Sounds.h            src/ui/Sounds.cpp
    src/ui/GridWidget.h        src/ui/GridWidget.cpp
    src/ui/EdgeTab.h           src/ui/EdgeTab.cpp
    src/ui/FolderMenu.h        src/ui/FolderMenu.cpp
    src/ui/FoldersDialog.h     src/ui/FoldersDialog.cpp
    src/ui/SettingsDialog.h    src/ui/SettingsDialog.cpp
    src/ui/StatsWidget.h       src/ui/StatsWidget.cpp
    src/ui/MainWindow.h        src/ui/MainWindow.cpp
)
qt_add_resources(Constancia "assets"
    PREFIX "/"
    BASE resources
    FILES
        resources/style.qss
        resources/sounds/chime.wav
        resources/sounds/alarm.wav
)
target_link_libraries(Constancia PRIVATE constancia_core Qt6::Widgets Qt6::Multimedia Qt6::Network)
if(WIN32)
    # Esquinas redondeadas, sombra y modo oscuro de Windows 11 (DWM).
    target_link_libraries(Constancia PRIVATE dwmapi)
endif()

# ---------------------------------------------------------------------------
# Tests (opcional): cmake -DCONSTANCIA_TESTS=ON
# ---------------------------------------------------------------------------
option(CONSTANCIA_TESTS "Compilar tests del núcleo" OFF)
if(CONSTANCIA_TESTS)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    enable_testing()
    qt_add_executable(core_tests tests/test_core.cpp)
    target_link_libraries(core_tests PRIVATE constancia_core Qt6::Test)
    add_test(NAME core_tests COMMAND core_tests)
endif()
