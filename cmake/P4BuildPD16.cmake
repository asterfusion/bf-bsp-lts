# p4-16 programs have bin and json files in the pipeline directory, so the conf # postprocess
# file is not a straight copy from the template file                            # postprocess
# search for pipeline_name from the p4 and append to program name               # postprocess
function(GEN_P4_16_PD_CONF_FILE t p4program)                                    # postprocess
  execute_process(COMMAND grep "Switch(.*).*main" ${t}                          # postprocess
                  COMMAND tr -d '[:space:]'                                     # postprocess
                  COMMAND cut -d\( -f2                                          # postprocess
                  COMMAND cut -d\) -f1                                          # postprocess
                  OUTPUT_VARIABLE pipeline_name                                 # postprocess
                  OUTPUT_STRIP_TRAILING_WHITESPACE)                             # postprocess
                                                                                # postprocess
  if (TARGET ${p4program}-tofino)                                               # postprocess
    gen_p4_16_pd_conf_file_trgt(${t} "tofino" ${pipeline_name} ${p4program})    # postprocess
  endif()                                                                       # postprocess
  if (TARGET ${p4program}-tofino2)                                              # postprocess
    gen_p4_16_pd_conf_file_trgt(${t} "tofino2" ${pipeline_name} ${p4program})   # postprocess
  endif()                                                                       # postprocess
  if (TARGET ${p4program}-tofino2m)                                             # postprocess
    gen_p4_16_pd_conf_file_trgt(${t} "tofino2m" ${pipeline_name} ${p4program})  # postprocess
  endif()                                                                       # postprocess
  if (TARGET ${p4program}-tofino3)                                              # postprocess
    gen_p4_16_pd_conf_file_trgt(${t} "tofino3" ${pipeline_name} ${p4program})   # postprocess
  endif()                                                                       # postprocess
endfunction()                                                                   # postprocess
                                                                                # postprocess
# t arch target p4program                                                       # postprocess
function(GEN_P4_16_PD_CONF_FILE_TRGT t target pipeline_name p4program)          # postprocess
  if (${target} STREQUAL "tofino2m")                                            # postprocess
    set(chiptype "tofino2")                                                     # postprocess
  else()                                                                        # postprocess
    set(chiptype ${target})                                                     # postprocess
  endif()                                                                       # postprocess
                                                                                # postprocess
  # Seek for .conf.in files to work both on p4factory and SDE                   # postprocess
  if (EXISTS "${CMAKE_MODULE_PATH}/../submodules/p4-tests/targets/")            # postprocess
    set(CONF_TARGET ${CMAKE_MODULE_PATH}/../submodules/p4-tests/targets/)       # postprocess
  elseif(EXISTS "${CMAKE_MODULE_PATH}/../pkgsrc/p4-examples/")                  # postprocess
    set(CONF_TARGET ${CMAKE_MODULE_PATH}/../pkgsrc/p4-examples/)                # postprocess
  endif()                                                                       # postprocess
                                                                                # postprocess
  string(TOUPPER ${chiptype} chiptype_uppercase)                                                       # postprocess
  set(DST_CONF_PATH ${CMAKE_CURRENT_BINARY_DIR}/pd_conf_gen/${p4program}/${target}/${pipeline_name})   # postprocess
  set(DST_CONF_FILE ${DST_CONF_PATH}/${p4program}.conf)                                                # postprocess
  add_custom_command(OUTPUT ${DST_CONF_FILE}                                                           # postprocess
    COMMAND mkdir -p ${DST_CONF_PATH}                                                                  # postprocess
    COMMAND cp ${CONF_TARGET}/${chiptype}/${chiptype}_single_device.conf.in ${DST_CONF_FILE}           # postprocess
    COMMAND sed -i 's/@${chiptype_uppercase}_SINGLE_DEVICE@/${p4program}/' ${DST_CONF_FILE}            # postprocess
    COMMAND sed -i 's/@${chiptype_uppercase}_VARIANT@/${target}/' ${DST_CONF_FILE}                     # postprocess
    COMMAND sed -i 's|context.json|${pipeline_name}/context.json|' ${DST_CONF_FILE}                    # postprocess
    COMMAND sed -i 's|${chiptype}\\.bin|${pipeline_name}/${chiptype}.bin|' ${DST_CONF_FILE}            # postprocess
  )                                                                                                    # postprocess
  add_custom_target(${p4program}-${target}-confgen DEPENDS ${DST_CONF_FILE})                           # postprocess
  add_dependencies(${p4program}-${target} ${p4program}-${target}-confgen)                              # postprocess
  add_dependencies(${p4program} ${p4program}-${target}-confgen)                                        # postprocess
  install(DIRECTORY ${DST_CONF_PATH}/ DESTINATION share/p4/targets/${target}                           # postprocess
    FILES_MATCHING                                                                                     # postprocess
    PATTERN "*.conf"                                                                                   # postprocess
  )                                                                                                    # postprocess
endfunction()                                                                                          # postprocess
