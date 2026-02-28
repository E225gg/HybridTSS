# Helper to define test executables
function(add_hybridtss_test TEST_NAME TEST_SOURCE)
    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} PRIVATE hybridtss_lib GTest::gtest_main)
    # Set working directory to project root so tests can find Data/
    add_test(
        NAME ${TEST_NAME}
        COMMAND ${TEST_NAME}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endfunction()

add_hybridtss_test(elementary_classes_test elementary_classes_test.cpp)
add_hybridtss_test(pstss_test              pstss_test.cpp)
add_hybridtss_test(cut_tss_test            cut_tss_test.cpp)
add_hybridtss_test(tuplemerge_test         tuplemerge_test.cpp)
add_hybridtss_test(integration_test        integration_test.cpp)
add_hybridtss_test(hybrid_tss_test         hybrid_tss_test.cpp)
