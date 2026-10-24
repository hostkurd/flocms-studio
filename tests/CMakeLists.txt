find_package(Qt6 REQUIRED COMPONENTS Network Test)

# The program the process tests start (tests/helper.cpp).
add_executable(flostudio_test_helper helper.cpp)
target_link_libraries(flostudio_test_helper PRIVATE Qt6::Core)
flostudio_set_warnings(flostudio_test_helper)
flostudio_set_sanitizers(flostudio_test_helper)

function(flostudio_add_test name)
    add_executable(${name} ${name}.cpp testutil.h)
    target_link_libraries(${name} PRIVATE Qt6::Test ${ARGN})
    target_compile_definitions(${name} PRIVATE
        FLOSTUDIO_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
        FLOSTUDIO_TEST_HELPER="$<TARGET_FILE:flostudio_test_helper>")
    add_dependencies(${name} flostudio_test_helper)
    flostudio_set_warnings(${name})
    flostudio_set_sanitizers(${name})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

flostudio_add_test(tst_core FloStudio::Core)
flostudio_add_test(tst_settings FloStudio::Core)
flostudio_add_test(tst_env FloStudio::Core)
flostudio_add_test(tst_project FloStudio::Core)
flostudio_add_test(tst_output FloStudio::Core)
flostudio_add_test(tst_logs FloStudio::Core)
flostudio_add_test(tst_tools FloStudio::Core Qt6::Network)
flostudio_add_test(tst_server FloStudio::Core Qt6::Network)
flostudio_add_test(tst_commands FloStudio::Core)
flostudio_add_test(tst_process FloStudio::Core)
flostudio_add_test(tst_realproject FloStudio::Core)
flostudio_add_test(tst_downloads FloStudio::Core Qt6::Network)
flostudio_add_test(tst_database FloStudio::Database)
flostudio_add_test(tst_security FloStudio::Security)

# The app's view model, built from the app's own sources
find_package(Qt6 REQUIRED COMPONENTS Concurrent Gui Qml Network)
flostudio_add_test(tst_studio FloStudio::Core Qt6::Gui Qt6::Qml Qt6::Network Qt6::Concurrent)
target_sources(tst_studio PRIVATE
    ../app/language.cpp ../app/language.h
    ../app/logmodel.cpp ../app/logmodel.h
    ../app/projectserver.cpp ../app/projectserver.h
    ../app/projectsmodel.cpp ../app/projectsmodel.h
    ../app/studio.cpp ../app/studio.h)
target_include_directories(tst_studio PRIVATE ../app)
target_compile_definitions(tst_studio PRIVATE FLOSTUDIO_SOURCE="${PROJECT_SOURCE_DIR}")
flostudio_set_warnings(tst_studio)
set_tests_properties(tst_studio PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
