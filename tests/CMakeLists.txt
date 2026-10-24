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
flostudio_add_test(tst_commands FloStudio::Core)
flostudio_add_test(tst_process FloStudio::Core)
flostudio_add_test(tst_realproject FloStudio::Core)
flostudio_add_test(tst_downloads FloStudio::Core Qt6::Network)
flostudio_add_test(tst_database FloStudio::Database)
flostudio_add_test(tst_security FloStudio::Security)
