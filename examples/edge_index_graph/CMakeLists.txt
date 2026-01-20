set(program
ei-triangle-count
ei-page-rank
ei-sssp
ei-vertex_nomination
)


foreach(p ${program})
  add_executable(${p} ${p}.cc)
  target_link_libraries(${p} ${SHAD_RUNTIME_LIB} runtime)
  if (SHAD_EXAMPLES_LIKWID_TARGET)
    target_compile_definitions(${p} PRIVATE ${SHAD_EXAMPLES_LIKWID_DEFS})
    target_link_libraries(${p} PRIVATE ${SHAD_EXAMPLES_LIKWID_TARGET})
  endif()
endforeach(p)
