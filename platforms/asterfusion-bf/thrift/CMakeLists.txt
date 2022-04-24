
include_directories("${CMAKE_CURRENT_BINARY_DIR}")
include_directories("${CMAKE_CURRENT_BINARY_DIR}/gen-cpp")
include_directories("${PROJECT_SOURCE_DIR}/src")
include_directories(include)
add_library(pltfm_mgr_thrift SHARED
src/bf_platform_rpc_server.cpp
${CMAKE_CURRENT_BINARY_DIR}/gen-cpp/pltfm_mgr_rpc.cpp
${CMAKE_CURRENT_BINARY_DIR}/gen-cpp/pltfm_mgr_rpc.h
${CMAKE_CURRENT_BINARY_DIR}/gen-cpp/pltfm_mgr_rpc_constants.cpp
${CMAKE_CURRENT_BINARY_DIR}/gen-cpp/pltfm_mgr_rpc_constants.h
${CMAKE_CURRENT_BINARY_DIR}/gen-cpp/pltfm_mgr_rpc_types.cpp
${CMAKE_CURRENT_BINARY_DIR}/gen-cpp/pltfm_mgr_rpc_types.h
${CMAKE_CURRENT_BINARY_DIR}/gen-cpp/pltfm_pm_rpc.cpp
${CMAKE_CURRENT_BINARY_DIR}/gen-cpp/pltfm_pm_rpc.h
${CMAKE_CURRENT_BINARY_DIR}/gen-cpp/pltfm_pm_rpc_constants.cpp
${CMAKE_CURRENT_BINARY_DIR}/gen-cpp/pltfm_pm_rpc_constants.h
${CMAKE_CURRENT_BINARY_DIR}/gen-cpp/pltfm_pm_rpc_types.cpp
${CMAKE_CURRENT_BINARY_DIR}/gen-cpp/pltfm_pm_rpc_types.h
)
target_link_libraries(pltfm_mgr_thrift PUBLIC ${THRIFT_LIBRARIES})
target_compile_options(pltfm_mgr_thrift PRIVATE -Wno-unused-parameter -Wno-unused-variable)
SET(THRIFT_FILES
pltfm_pm_rpc.thrift
pltfm_mgr_rpc.thrift)

add_custom_command(OUTPUT gen-cpp/pltfm_mgr_rpc.cpp gen-cpp/pltfm_mgr_rpc.h gen-cpp/pltfm_mgr_rpc_constants.cpp gen-cpp/pltfm_mgr_rpc_constants.h gen-cpp/pltfm_mgr_rpc_types.cpp gen-cpp/pltfm_mgr_rpc_types.h gen-cpp/pltfm_pm_rpc.cpp gen-cpp/pltfm_pm_rpc.h gen-cpp/pltfm_pm_rpc_constants.cpp gen-cpp/pltfm_pm_rpc_constants.h gen-cpp/pltfm_pm_rpc_types.cpp gen-cpp/pltfm_pm_rpc_types.h
    DEPENDS ${THRIFT_COMPILER} ${THRIFT_FILES}
    COMMAND ${THRIFT_COMPILER} --gen cpp:templates,cob_style -r ${CMAKE_CURRENT_SOURCE_DIR}/pltfm_mgr_rpc.thrift
    COMMAND ${THRIFT_COMPILER} --gen cpp:templates,cob_style -r ${CMAKE_CURRENT_SOURCE_DIR}/pltfm_pm_rpc.thrift
)


