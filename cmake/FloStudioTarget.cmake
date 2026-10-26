# Shared settings for every FloStudio target: warnings (D11), sanitizers,
# and the static/shared switch for the three libraries (D6).

include(GenerateExportHeader)

# Strict warnings apply to our own source files only, not to the files Qt
# generates into the build folder (moc, QML cache, resources): those trip
# -Wshadow and friends and are not ours to fix. Call this after all of the
# target's own sources are added.
function(flostudio_set_warnings target)
    if(MSVC)
        set(options /W4 /permissive- /external:W0)
        if(FLOSTUDIO_WARNINGS_AS_ERRORS)
            list(APPEND options /WX)
        endif()
        # UTF-8 source and execution character sets for every file.
        target_compile_options(${target} PRIVATE /utf-8)
    else()
        set(options -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor
            -Wold-style-cast -Wcast-align -Woverloaded-virtual -Wconversion
            -Wsign-conversion -Wformat=2)
        if(FLOSTUDIO_WARNINGS_AS_ERRORS)
            list(APPEND options -Werror)
        endif()
    endif()

    get_target_property(source_dir ${target} SOURCE_DIR)
    get_target_property(sources ${target} SOURCES)
    foreach(source IN LISTS sources)
        cmake_path(ABSOLUTE_PATH source BASE_DIRECTORY "${source_dir}" NORMALIZE OUTPUT_VARIABLE path)
        cmake_path(IS_PREFIX PROJECT_BINARY_DIR "${path}" NORMALIZE generated)
        if(generated OR NOT path MATCHES "\\.(cpp|cc|cxx)$")
            continue()
        endif()
        set_property(SOURCE "${path}" TARGET_DIRECTORY ${target} APPEND PROPERTY COMPILE_OPTIONS ${options})
    endforeach()
endfunction()

function(flostudio_set_sanitizers target)
    if(FLOSTUDIO_SANITIZE AND NOT MSVC)
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined)
        target_link_options(${target} PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()

# flostudio_add_library(<name> <short> SOURCES ... HEADERS ... [DEPENDS ...] [QT ...])
#   name   target name, also the file name: FloStudioCore
#   short  alias and export macro part: Core -> FloStudio::Core, FLOSTUDIOCORE_EXPORT
# Each library's public headers live in include/flostudio/<lower short>/.
# DEPENDS lists other FloStudio libraries; their include folders become
# visible only through that link, so a library can't include one it
# doesn't depend on (D5: dependencies point one way).
function(flostudio_add_library name short)
    cmake_parse_arguments(ARG "" "" "SOURCES;HEADERS;DEPENDS;QT" ${ARGN})

    if(FLOSTUDIO_SHARED_LIBS)
        add_library(${name} SHARED ${ARG_SOURCES} ${ARG_HEADERS})
    else()
        add_library(${name} STATIC ${ARG_SOURCES} ${ARG_HEADERS})
    endif()
    add_library(FloStudio::${short} ALIAS ${name})

    # Version information in each DLL, as in Studio.exe: code signing checks
    # the product name and version of every file it signs
    if(WIN32 AND FLOSTUDIO_SHARED_LIBS)
        set(RC_NAME "${name}")
        set(RC_DESCRIPTION "FloCMS Studio ${short} library")
        configure_file("${PROJECT_SOURCE_DIR}/cmake/library.rc.in" "${CMAKE_CURRENT_BINARY_DIR}/${name}.rc" @ONLY)
        target_sources(${name} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/${name}.rc")
    endif()

    string(TOLOWER "${short}" lower)
    string(TOUPPER "${name}" upper)
    generate_export_header(${name}
        BASE_NAME ${name}
        EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/include/flostudio/${lower}/export.h")
    if(NOT FLOSTUDIO_SHARED_LIBS)
        target_compile_definitions(${name} PUBLIC ${upper}_STATIC_DEFINE)
    endif()

    target_include_directories(${name}
        PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/include"
            "${CMAKE_CURRENT_BINARY_DIR}/include"
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src")

    set_target_properties(${name} PROPERTIES
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR})

    target_link_libraries(${name} PUBLIC Qt6::Core ${ARG_DEPENDS})
    foreach(module IN LISTS ARG_QT)
        target_link_libraries(${name} PRIVATE Qt6::${module})
    endforeach()

    flostudio_set_warnings(${name})
    flostudio_set_sanitizers(${name})
endfunction()
