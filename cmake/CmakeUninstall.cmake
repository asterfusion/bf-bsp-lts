if(NOT EXISTS "${BINARY_DIR}/install_manifest.txt")
  message(FATAL_ERROR "Install manifest does not exist: ${BINARY_DIR}/install_manifest.txt")
endif()

execute_process(
    COMMAND "${CMAKE_CURRENT_LIST_DIR}/get_files_to_be_uninstalled.py" "${BINARY_DIR}/install_manifest.txt"
    RESULT_VARIABLE return_code
    OUTPUT_VARIABLE files_to_remove
)

if(NOT "${return_code}" STREQUAL 0)
  message(FATAL_ERROR "cannot determine which files should be uninstalled")
endif()

string(REGEX REPLACE "\n" ";" files_to_remove "${files_to_remove}")
foreach(file ${files_to_remove})
  message(STATUS "Uninstalling ${file}")
  if(EXISTS "${file}" OR IS_SYMLINK "${file}")
    execute_process(
      COMMAND rm -r "${file}"
      RESULT_VARIABLE return_code
      )
    if(NOT "${return_code}" STREQUAL 0)
      message(FATAL_ERROR "Cannot remove ${file}")
    endif()
  else()
    message(STATUS "File ${file} does not exist.")
  endif()
endforeach()
