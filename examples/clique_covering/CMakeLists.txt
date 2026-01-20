set(program
clique_covering)

foreach(p ${program})
  add_executable(${p} ${p}.cc)
  target_link_libraries(${p} PRIVATE gmt_runtime ${SHAD_RUNTIME_LIB} runtime)
  if (SHAD_EXAMPLES_LIKWID_TARGET)
    target_compile_definitions(${p} PRIVATE ${SHAD_EXAMPLES_LIKWID_DEFS})
    target_link_libraries(${p} PRIVATE ${SHAD_EXAMPLES_LIKWID_TARGET})
  endif()
endforeach(p)
