if(PYTHON_DEPENDENCIES_INCLUDED)
  return()
endif()
set(PYTHON_DEPENDENCIES_INCLUDED TRUE)

if (NOT DEFINED PYTHON_EXECUTABLE)
    execute_process(
        COMMAND bash -c "readlink --canonicalize $(which python3)"
        OUTPUT_VARIABLE PYTHON_EXECUTABLE
        RESULT_VARIABLE PYTHON_EXECUTABLE_RET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if (NOT PYTHON_EXECUTABLE_RET EQUAL 0)
        message(FATAL_ERROR "Python executable not found")
    endif ()
endif ()

if(NOT DEFINED SDE_GENERATED_PYTHON_DIR)
    execute_process(
        COMMAND ${PYTHON_EXECUTABLE} -c
        "from distutils import sysconfig as sc; print('${CMAKE_INSTALL_PREFIX}/' + sc.get_python_lib(prefix='', standard_lib=True, plat_specific=True) + '/site-packages')"
        OUTPUT_VARIABLE SDE_GENERATED_PYTHON_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()

if(NOT DEFINED SDE_PYTHON_THIRD_PARTY_DEPENDENCIES)
    execute_process(
        COMMAND ${PYTHON_EXECUTABLE} "${CMAKE_CURRENT_LIST_DIR}/sdepythonpath.py" --sde-install "${CMAKE_INSTALL_PREFIX}" --sde-dependencies "${SDE_DEPENDENCIES}"
        OUTPUT_VARIABLE SDE_PYTHON_THIRD_PARTY_DEPENDENCIES
        RESULT_VARIABLE SDE_PYTHON_THIRD_PARTY_DEPENDENCIES_RET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if (NOT SDE_PYTHON_THIRD_PARTY_DEPENDENCIES_RET EQUAL 0)
        message(FATAL_ERROR "Cannot determine path to SDE Python third-party dependencies")
    endif ()
endif()

set(PYTHON_COMMAND ${CMAKE_COMMAND} -E env PYTHONPATH="${SDE_PYTHON_THIRD_PARTY_DEPENDENCIES}:${SDE_GENERATED_PYTHON_DIR}" ${PYTHON_EXECUTABLE})
install(PROGRAMS "${CMAKE_CURRENT_LIST_DIR}/sdepythonpath.py" DESTINATION "${CMAKE_INSTALL_PREFIX}/bin")

set(SDE_PYTHON_DEPENDENCIES_DIR ${SDE_GENERATED_PYTHON_DIR})

set(CMAKE_PROGRAM_PATH ${SDE_PYTHON_THIRD_PARTY_DEPENDENCIES}/bin)
