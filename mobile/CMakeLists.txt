cmake_minimum_required(VERSION 3.21)
project(UltimateTrackerMobile VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.4 REQUIRED COMPONENTS Core Gui Qml Quick Sql)

# El mismo motor que la app de escritorio (pomodoros, base, estadísticas, hitos).
set(CORE ${CMAKE_CURRENT_SOURCE_DIR}/../src/core)
add_library(ut_core STATIC
    ${CORE}/Types.h
    ${CORE}/Format.h          ${CORE}/Format.cpp
    ${CORE}/DayClock.h        ${CORE}/DayClock.cpp
    ${CORE}/Goals.h           ${CORE}/Goals.cpp
    ${CORE}/FolderTree.h      ${CORE}/FolderTree.cpp
    ${CORE}/Database.h        ${CORE}/Database.cpp
    ${CORE}/Stats.h           ${CORE}/Stats.cpp
    ${CORE}/Milestones.h      ${CORE}/Milestones.cpp
    ${CORE}/SessionController.h ${CORE}/SessionController.cpp
)
target_include_directories(ut_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../src)
target_link_libraries(ut_core PUBLIC Qt6::Core Qt6::Sql)

qt_add_executable(UltimateTrackerMobile
    src/main.cpp
    src/MobileApp.h src/MobileApp.cpp
    src/AndroidBridge.h src/AndroidBridge.cpp
)
qt_add_qml_module(UltimateTrackerMobile
    URI UltimateTracker
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/TimerRing.qml
        qml/Tiles.qml
        qml/ActivitySheet.qml
        qml/UtButton.qml
        qml/Stepper.qml
)
target_link_libraries(UltimateTrackerMobile PRIVATE ut_core Qt6::Gui Qt6::Qml Qt6::Quick)

if(ANDROID)
    set_target_properties(UltimateTrackerMobile PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android
        QT_ANDROID_TARGET_SDK_VERSION 34
        QT_ANDROID_MIN_SDK_VERSION 28
        QT_ANDROID_VERSION_NAME "1.0-beta.1"
        QT_ANDROID_VERSION_CODE 1)
endif()
