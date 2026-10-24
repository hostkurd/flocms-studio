flostudio_add_library(FloStudioSecurity Security
    SOURCES
        src/library.cpp
    HEADERS
        include/flostudio/security/library.h
    DEPENDS
        FloStudio::Core
)
