qt_add_executable(Studio
    main.cpp
)

# Names follow each system's habit: Studio.exe (D1), "FloCMS Studio.app"
# (what Finder shows in Applications), flocms-studio on Linux.
if(WIN32)
    set_target_properties(Studio PROPERTIES OUTPUT_NAME "Studio" WIN32_EXECUTABLE ON)
    configure_file(studio.rc.in "${CMAKE_CURRENT_BINARY_DIR}/studio.rc" @ONLY)
    target_sources(Studio PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/studio.rc")
elseif(APPLE)
    # The same minimum as the program itself (CI passes Qt's minimum).
    if(CMAKE_OSX_DEPLOYMENT_TARGET)
        set(FLOSTUDIO_MIN_MACOS "${CMAKE_OSX_DEPLOYMENT_TARGET}")
    else()
        execute_process(COMMAND xcrun --show-sdk-version OUTPUT_VARIABLE FLOSTUDIO_MIN_MACOS OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()
    set_target_properties(Studio PROPERTIES
        OUTPUT_NAME "FloCMS Studio"
        MACOSX_BUNDLE ON
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
        MACOSX_BUNDLE_BUNDLE_NAME "FloCMS Studio"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.hostkurd.flocmsstudio"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}")
else()
    set_target_properties(Studio PROPERTIES OUTPUT_NAME "flocms-studio")
endif()

set_source_files_properties(qml/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)

qt_add_qml_module(Studio
    URI FloStudio.App
    VERSION 1.0
    # The same path on Qt 6.4 and 6.12: qrc:/qt/qml/FloStudio/App/Main.qml
    RESOURCE_PREFIX /qt/qml
    # Not the default folder next to the program: on macOS's case-insensitive
    # disk a module folder can clash with the program's name.
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qml/FloStudio/App"
    QML_FILES
        qml/Main.qml
    SOURCES
        appinfo.h
        appinfo.cpp
)

target_link_libraries(Studio PRIVATE
    Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
    FloStudio::Core FloStudio::Database FloStudio::Security)

flostudio_set_warnings(Studio)
flostudio_set_sanitizers(Studio)
