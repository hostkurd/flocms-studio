cmake_minimum_required(VERSION 3.21)

project(FloStudioRtlCheck VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# 6.4 is the oldest we build with (Ubuntu 24.04's packages, used in the
# sandbox); CI uses Qt 6.12.
find_package(Qt6 6.4 REQUIRED COMPONENTS Gui Qml Quick QuickControls2 LinguistTools)

qt_add_executable(rtlcheck main.cpp language.cpp language.h selftest.cpp selftest.h)

if(MSVC)
    target_compile_options(rtlcheck PRIVATE /W4 /utf-8)
else()
    target_compile_options(rtlcheck PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_source_files_properties(Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(rtlcheck
    URI RtlCheck
    VERSION 1.0
    RESOURCE_PREFIX /
    # Not the default build/RtlCheck/: on macOS's case-insensitive disk that
    # folder clashes with the rtlcheck program and linking fails.
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/qml/RtlCheck
    QML_FILES
        Main.qml
        Theme.qml
        AppText.qml
        RichText.qml
        AppButton.qml
        Card.qml
        Pill.qml
        CheckRow.qml
        Sidebar.qml
        Overview.qml
        Doctor.qml
)

qt_add_resources(rtlcheck "fonts"
    PREFIX /
    FILES
        fonts/NotoSans-Regular.ttf
        fonts/NotoSans-Bold.ttf
        fonts/NotoSansArabic-Regular.ttf
        fonts/NotoSansArabic-Bold.ttf
        fonts/JetBrainsMono-Regular.ttf
)

qt_add_translations(rtlcheck
    TS_FILES
        i18n/rtlcheck_en.ts
        i18n/rtlcheck_ar.ts
        i18n/rtlcheck_ckb.ts
    RESOURCE_PREFIX /i18n
)

target_link_libraries(rtlcheck PRIVATE Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2)

set_target_properties(rtlcheck PROPERTIES WIN32_EXECUTABLE ON)
