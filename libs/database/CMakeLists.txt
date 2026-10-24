flostudio_add_library(FloStudioDatabase Database
    SOURCES
        src/actions.cpp
        src/library.cpp
    HEADERS
        include/flostudio/database/actions.h
        include/flostudio/database/library.h
    DEPENDS
        FloStudio::Core
)
