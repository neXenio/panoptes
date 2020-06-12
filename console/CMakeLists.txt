set (UNIT_CONSOLE_NAME "PanoptesConsole")

set (PANOPTES_CONSOLE_SOURCES
  "main.cpp"
)

add_executable (${UNIT_CONSOLE_NAME} ${PANOPTES_CONSOLE_SOURCES})

target_include_directories(${UNIT_CONSOLE_NAME} PUBLIC ${PANOPTES_CONSOLE})

set_target_properties(${UNIT_CONSOLE_NAME} PROPERTIES CXX_STANDARD 17)

target_link_libraries(${UNIT_CONSOLE_NAME}
                         PUBLIC   ${CMAKE_THREAD_LIBS_INIT}
                                  ${PANOPTES_LIBRARY_NAME})
