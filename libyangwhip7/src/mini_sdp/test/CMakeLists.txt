include_directories(${DMINISDP})

set(SDP_TEST_NAME "run_sdp_test")
add_executable(${SDP_TEST_NAME} test_sdp.cc)
target_link_libraries(${SDP_TEST_NAME} minisdp)

set(STOP_TEST_NAME "run_stop_test")
add_executable(${STOP_TEST_NAME} test_stop.cc)
target_link_libraries(${STOP_TEST_NAME} minisdp)

set(CLIENT_TEST_NAME "run_client_test")
add_executable(${CLIENT_TEST_NAME} test_client.cc)
target_link_libraries(${CLIENT_TEST_NAME} minisdp)

set(USIG_TEST_NAME "run_usig_test")
add_executable(${USIG_TEST_NAME} test_usig.cc)
target_link_libraries(${USIG_TEST_NAME} minisdp)
