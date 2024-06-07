
include_directories(SYSTEM ${THRIFT_INCLUDE_DIR})
include_directories("${CMAKE_CURRENT_BINARY_DIR}")
include_directories("${CMAKE_CURRENT_BINARY_DIR}/gen-cpp")
include_directories("${PROJECT_SOURCE_DIR}/src")
include_directories(include)

set(THRIFT_CPP_GEN_SRC_LIST
${CMAKE_CURRENT_BINARY_DIR}/gen-cpp/pltfm_mgr_rpc.cpp
${CMAKE_CURRENT_BINARY_DIR}/gen-cpp/pltfm_mgr_rpc.h
${CMAKE_CURRENT_BINARY_DIR}/gen-cpp/pltfm_mgr_rpc_types.cpp
${CMAKE_CURRENT_BINARY_DIR}/gen-cpp/pltfm_mgr_rpc_types.h
${CMAKE_CURRENT_BINARY_DIR}/gen-cpp/pltfm_pm_rpc.cpp
${CMAKE_CURRENT_BINARY_DIR}/gen-cpp/pltfm_pm_rpc.h
${CMAKE_CURRENT_BINARY_DIR}/gen-cpp/pltfm_pm_rpc_types.cpp
${CMAKE_CURRENT_BINARY_DIR}/gen-cpp/pltfm_pm_rpc_types.h
)
if ( THRIFT_VERSION_STRING VERSION_LESS 0.14.0 )
list(APPEND THRIFT_CPP_GEN_SRC_LIST
${CMAKE_CURRENT_BINARY_DIR}/gen-cpp/pltfm_mgr_rpc_constants.cpp
${CMAKE_CURRENT_BINARY_DIR}/gen-cpp/pltfm_mgr_rpc_constants.h
${CMAKE_CURRENT_BINARY_DIR}/gen-cpp/pltfm_pm_rpc_constants.cpp
${CMAKE_CURRENT_BINARY_DIR}/gen-cpp/pltfm_pm_rpc_constants.h
)
endif()

add_library(pltfm_mgr_thrift SHARED
src/bf_platform_rpc_server.cpp
${THRIFT_CPP_GEN_SRC_LIST}
)
set_target_properties(pltfm_mgr_thrift PROPERTIES VERSION 0.0.0 SOVERSION 0)
target_link_libraries(pltfm_mgr_thrift PUBLIC ${THRIFT_LIBRARIES})
target_compile_options(pltfm_mgr_thrift PRIVATE -Wno-unused-parameter -Wno-unused-variable)
SET(THRIFT_FILES
pltfm_pm_rpc.thrift
pltfm_mgr_rpc.thrift)

SET(PLTFM_MGR_RPC_THRIFT_PY_FILES
  ${CMAKE_CURRENT_BINARY_DIR}/gen-py/pltfm_mgr_rpc/constants.py
  ${CMAKE_CURRENT_BINARY_DIR}/gen-py/pltfm_mgr_rpc/__init__.py
  ${CMAKE_CURRENT_BINARY_DIR}/gen-py/pltfm_mgr_rpc/ttypes.py
  ${CMAKE_CURRENT_BINARY_DIR}/gen-py/pltfm_mgr_rpc/pltfm_mgr_rpc.py
  ${CMAKE_CURRENT_BINARY_DIR}/gen-py/pltfm_mgr_rpc/pltfm_mgr_rpc-remote
)

SET(PLTFM_PM_RPC_THRIFT_PY_FILES
  ${CMAKE_CURRENT_BINARY_DIR}/gen-py/pltfm_pm_rpc/constants.py
  ${CMAKE_CURRENT_BINARY_DIR}/gen-py/pltfm_pm_rpc/__init__.py
  ${CMAKE_CURRENT_BINARY_DIR}/gen-py/pltfm_pm_rpc/ttypes.py
  ${CMAKE_CURRENT_BINARY_DIR}/gen-py/pltfm_pm_rpc/pltfm_pm_rpc.py
  ${CMAKE_CURRENT_BINARY_DIR}/gen-py/pltfm_pm_rpc/pltfm_pm_rpc-remote
)

add_custom_command(OUTPUT ${THRIFT_CPP_GEN_SRC_LIST}
    DEPENDS ${THRIFT_COMPILER} ${THRIFT_FILES}
    COMMAND ${THRIFT_COMPILER} --gen cpp:templates,cob_style -r ${CMAKE_CURRENT_SOURCE_DIR}/pltfm_mgr_rpc.thrift
    COMMAND ${THRIFT_COMPILER} --gen cpp:templates,cob_style -r ${CMAKE_CURRENT_SOURCE_DIR}/pltfm_pm_rpc.thrift
)

add_custom_command(OUTPUT ${PLTFM_MGR_RPC_THRIFT_PY_FILES}
    DEPENDS ${THRIFT_COMPILER} ${THRIFT_FILES}
    COMMAND ${THRIFT_COMPILER} --gen py -r ${CMAKE_CURRENT_SOURCE_DIR}/pltfm_mgr_rpc.thrift
)

add_custom_command(OUTPUT ${PLTFM_PM_RPC_THRIFT_PY_FILES}
    DEPENDS ${THRIFT_COMPILER} ${THRIFT_FILES}
    COMMAND ${THRIFT_COMPILER} --gen py -r ${CMAKE_CURRENT_SOURCE_DIR}/pltfm_pm_rpc.thrift
)

add_custom_target(pltfm_mgr_rpc_python DEPENDS ${PLTFM_MGR_RPC_THRIFT_PY_FILES})
add_custom_target(pltfm_pm_rpc_python DEPENDS ${PLTFM_PM_RPC_THRIFT_PY_FILES})
add_dependencies(pltfm_mgr_thrift pltfm_mgr_rpc_python pltfm_pm_rpc_python)

