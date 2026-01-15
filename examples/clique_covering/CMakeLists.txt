set(program
clique_covering)


foreach(p ${program})
  add_executable(${p} ${p}.cc)
  # Add debug symbols only for Debug and RelWithDebInfo configs:
  # target_compile_options(${p} PRIVATE
  #   $<$<CONFIG:Debug>:-g>
  #   $<$<CONFIG:RelWithDebInfo>:-g>
  # )
  target_link_libraries(${p} ${SHAD_RUNTIME_LIB} runtime)
endforeach(p)
