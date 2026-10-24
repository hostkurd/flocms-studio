configure_file(src/version.h.in "${CMAKE_CURRENT_BINARY_DIR}/include/flostudio/core/version.h" @ONLY)

flostudio_add_library(FloStudioCore Core
    SOURCES
        src/buildinfo.cpp
        src/commands.cpp
        src/downloads.cpp
        src/env.cpp
        src/logs.cpp
        src/output.cpp
        src/process.cpp
        src/project.cpp
        src/server.cpp
        src/settings.cpp
        src/tools.cpp
        src/wizard.cpp
    HEADERS
        include/flostudio/core/buildinfo.h
        include/flostudio/core/commands.h
        include/flostudio/core/downloads.h
        include/flostudio/core/env.h
        include/flostudio/core/logs.h
        include/flostudio/core/output.h
        include/flostudio/core/process.h
        include/flostudio/core/project.h
        include/flostudio/core/result.h
        include/flostudio/core/server.h
        include/flostudio/core/settings.h
        include/flostudio/core/tools.h
        include/flostudio/core/wizard.h
        "${CMAKE_CURRENT_BINARY_DIR}/include/flostudio/core/version.h"
    # Network: portAnswers(), tool downloads; Concurrent: unpacking off the
    # interface thread
    QT Network Concurrent
)

# The tool list Studio may download from (D15), made by tools/tool_manifest.py
qt_add_resources(FloStudioCore "tools"
    PREFIX /flostudio/core
    BASE resources
    FILES resources/tools.json)

target_compile_definitions(FloStudioCore PRIVATE
    FLOSTUDIO_QT_STATIC_BUILD=$<BOOL:${FLOSTUDIO_QT_STATIC}>
    FLOSTUDIO_SHARED_LIBS_BUILD=$<BOOL:${FLOSTUDIO_SHARED_LIBS}>)
