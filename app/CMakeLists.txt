qt_add_executable(Studio
    main.cpp
    language.h
    language.cpp
    logmodel.h
    logmodel.cpp
    mariadbserver.h
    mariadbserver.cpp
    newproject.h
    newproject.cpp
    singleinstance.h
    singleinstance.cpp
    projectchecks.h
    projectchecks.cpp
    projectgenerate.h
    projectgenerate.cpp
    projectpackages.h
    projectpackages.cpp
    projectserver.h
    projectserver.cpp
    projectsmodel.h
    projectsmodel.cpp
    studio.h
    studio.cpp
)

# Names follow each system's habit: Studio.exe (D1), "FloCMS Studio.app"
# (what Finder shows in Applications), flocms-studio on Linux.
if(WIN32)
    set_target_properties(Studio PROPERTIES OUTPUT_NAME "Studio" WIN32_EXECUTABLE ON)
    configure_file(studio.rc.in "${CMAKE_CURRENT_BINARY_DIR}/studio.rc" @ONLY)
    target_sources(Studio PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/studio.rc")
    # The segment heap: memory freed after a while goes back to the system
    # (MSVC merges this with the manifest it makes)
    target_sources(Studio PRIVATE windows/studio.manifest)
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
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_ICON_FILE "studio.icns"
        # The packaging step finds our libraries in Contents/Frameworks
        BUILD_RPATH "@executable_path/../Frameworks")
    # Room in the header for the packaging step's rpath and name changes
    target_link_options(Studio PRIVATE "LINKER:-headerpad_max_install_names")
    # The app's icon, in Contents/Resources
    set_source_files_properties(icons/studio.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    target_sources(Studio PRIVATE icons/studio.icns)
else()
    set_target_properties(Studio PROPERTIES OUTPUT_NAME "flocms-studio")
endif()

set(STUDIO_QML
    qml/Main.qml
    qml/Theme.qml
    qml/Icons.qml
    qml/Texts.qml
    qml/AppText.qml
    qml/RichText.qml
    qml/Mono.qml
    qml/Heading.qml
    qml/SectionLabel.qml
    qml/Icon.qml
    qml/DashedBox.qml
    qml/AppButton.qml
    qml/Card.qml
    qml/CommandTile.qml
    qml/DatabaseCard.qml
    qml/DoctorCard.qml
    qml/Pill.qml
    qml/KeyValue.qml
    qml/Field.qml
    qml/Segmented.qml
    qml/NavItem.qml
    qml/AppRadio.qml
    qml/AppCheck.qml
    qml/ChoiceRow.qml
    qml/OptionRow.qml
    qml/DownloadRow.qml
    qml/ProblemBar.qml
    qml/Sidebar.qml
    qml/WelcomePage.qml
    qml/ProjectPage.qml
    qml/CheckRow.qml
    qml/CommandOutput.qml
    qml/ConfirmDialog.qml
    qml/DeleteProjectDialog.qml
    qml/ProjectMenu.qml
    qml/DatabaseTab.qml
    qml/DoctorTab.qml
    qml/FormField.qml
    qml/GenerateTab.qml
    qml/PackagesTab.qml
    qml/Choice.qml
    qml/LanguageChip.qml
    qml/LogsTab.qml
    qml/NewProjectPage.qml
    qml/OverviewTab.qml
    qml/ProblemText.qml
    qml/ServerCard.qml
    qml/ShownOnce.qml
    qml/Splash.qml
    qml/StatusIcon.qml
    qml/SettingsPage.qml
    qml/DownloadsDialog.qml
    qml/DatabaseSettings.qml
    qml/MariadbProblem.qml
    qml/MariadbSetup.qml
)
foreach(file IN LISTS STUDIO_QML)
    cmake_path(GET file FILENAME name)
    set_source_files_properties(${file} PROPERTIES QT_RESOURCE_ALIAS ${name})
endforeach()
set_source_files_properties(qml/Theme.qml qml/Icons.qml qml/Texts.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(Studio
    URI FloStudio.App
    VERSION 1.0
    # The same path on Qt 6.4 and 6.12: qrc:/qt/qml/FloStudio/App/Main.qml
    RESOURCE_PREFIX /qt/qml
    # Not the default folder next to the program: on macOS's case-insensitive
    # disk a module folder can clash with the program's name.
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qml/FloStudio/App"
    QML_FILES ${STUDIO_QML}
    SOURCES
        appinfo.h
        appinfo.cpp
)

# The logo (tools/make_icons.py), at :/icons/: the window icon and the sidebar
qt_add_resources(Studio "icons"
    PREFIX /
    FILES icons/logo-64.png icons/logo-128.png icons/logo-256.png icons/logo-512.png
          icons/app-icon-64.png icons/app-icon-128.png icons/app-icon-256.png icons/app-icon-512.png)

# The bundled fonts (D8; tools/make_fonts.py), at :/fonts/
file(GLOB STUDIO_FONTS RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" CONFIGURE_DEPENDS fonts/*.ttf)
qt_add_resources(Studio "fonts"
    PREFIX /
    FILES ${STUDIO_FONTS})

# Translations (D9): i18n/studio_<language>.ts, compiled into :/i18n/
qt_add_translations(Studio
    TS_FILES
        i18n/studio_en.ts
        i18n/studio_ar.ts
        i18n/studio_ckb.ts
    RESOURCE_PREFIX /i18n
    LUPDATE_OPTIONS -no-obsolete)

target_link_libraries(Studio PRIVATE
    Qt6::Concurrent Qt6::Gui Qt6::Network Qt6::Qml Qt6::Quick Qt6::QuickControls2
    FloStudio::Core FloStudio::Database FloStudio::Security)

flostudio_set_warnings(Studio)
flostudio_set_sanitizers(Studio)
