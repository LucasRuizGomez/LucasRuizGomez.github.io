# Copies what the web build needs into one folder that Emscripten packs into index.data:
# everything listed under "bundle" in config/assets.json, plus config/ itself.
# Music is copied next to index.html instead and fetched on demand, so the first load stays small.
function(hc_stage_assets target stage_dir)
  set(src "${CMAKE_CURRENT_SOURCE_DIR}")
  file(READ "${src}/config/assets.json" manifest)
  string(JSON count LENGTH "${manifest}" bundle)
  set(copy_cmds "")
  math(EXPR last "${count} - 1")
  foreach(i RANGE ${last})
    string(JSON entry GET "${manifest}" bundle ${i})
    if(IS_DIRECTORY "${src}/${entry}")
      list(APPEND copy_cmds COMMAND ${CMAKE_COMMAND} -E copy_directory "${src}/${entry}" "${stage_dir}/${entry}")
    else()
      get_filename_component(dir "${entry}" DIRECTORY)
      list(APPEND copy_cmds COMMAND ${CMAKE_COMMAND} -E make_directory "${stage_dir}/${dir}"
                            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${src}/${entry}" "${stage_dir}/${dir}/")
    endif()
  endforeach()
  file(GLOB config_files CONFIGURE_DEPENDS "${src}/config/*.json")
  add_custom_target(hc_stage
    COMMAND ${CMAKE_COMMAND} -E make_directory "${stage_dir}"
    ${copy_cmds}
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${src}/config" "${stage_dir}/config"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${src}/assets/audio/music" "${CMAKE_BINARY_DIR}/dist/music"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${src}/assets/CREDITS.md" "${CMAKE_BINARY_DIR}/dist/CREDITS.md"
    DEPENDS ${config_files}
    COMMENT "Staging assets for the web build")
  add_dependencies(${target} hc_stage)
  # Relink when a config file changes so index.data is repacked.
  set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS ${config_files})
endfunction()
