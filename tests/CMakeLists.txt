find_package(Qt6 REQUIRED COMPONENTS Test)

function(flostudio_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE Qt6::Test ${ARGN})
    flostudio_set_warnings(${name})
    flostudio_set_sanitizers(${name})
    add_test(NAME ${name} COMMAND ${name})
endfunction()

flostudio_add_test(tst_core FloStudio::Core)
flostudio_add_test(tst_database FloStudio::Database)
flostudio_add_test(tst_security FloStudio::Security)
