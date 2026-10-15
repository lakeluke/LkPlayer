find_package(Qt6 REQUIRED COMPONENTS Test Concurrent)

function(lk_add_core_test target)
    add_executable(${target} ${ARGN})
    target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(${target} PRIVATE Qt6::Core Qt6::Test Qt6::Concurrent)
    add_test(NAME ${target} COMMAND ${target})
endfunction()

lk_add_core_test(tst_core
    tst_core.cpp
    ${CMAKE_SOURCE_DIR}/src/core/NfoParser.cpp
    ${CMAKE_SOURCE_DIR}/src/core/PathUtils.cpp
    ${CMAKE_SOURCE_DIR}/src/core/LibraryScanner.cpp
)
