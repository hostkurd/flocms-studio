flostudio_add_library(FloStudioDatabase Database
    SOURCES
        src/library.cpp
    HEADERS
        include/flostudio/database/library.h
    DEPENDS
        FloStudio::Core
)
