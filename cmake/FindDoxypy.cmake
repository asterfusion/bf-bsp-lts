# Check for the presence of doxypy

if (NOT DOXYPY_FOUND)

  find_program (DOXYPY_EXECUTABLE doxypy
    PATH_SUFFIXES /bin /usr/bin /usr/local/bin/)

  find_package_handle_standard_args (Doxypy DEFAULT_MSG DOXYPY_EXECUTABLE)

endif (NOT DOXYPY_FOUND)
