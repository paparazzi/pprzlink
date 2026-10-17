add_library(pprzlink_agent STATIC
  Options.cpp AircraftRegistry.cpp IvyBridge.cpp
  XbeeTransmitter.cpp LinkAgent.cpp SocatSession.cpp)
# Keep the copied agent independent of the library build directory. Component
# archives retain the normal dynamic dependencies on Ivy and system libraries.
target_link_libraries(pprzlink_agent PUBLIC pprzlink::io pprzlink::ivy)
target_include_directories(pprzlink_agent PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(pprzlink_agent PRIVATE -Wall -Wextra -Werror)
set_target_properties(pprzlink_agent PROPERTIES CXX_EXTENSIONS OFF)

add_executable(link++ main.cpp)
target_link_libraries(link++ PRIVATE pprzlink_agent)
target_compile_options(link++ PRIVATE -Wall -Wextra -Werror)
file(RELATIVE_PATH link_library_path "${CMAKE_INSTALL_FULL_BINDIR}" "${CMAKE_INSTALL_FULL_LIBDIR}")
if(APPLE)
    set(link_origin "@loader_path")
else()
    set(link_origin "$ORIGIN")
endif()
set_target_properties(link++ PROPERTIES INSTALL_RPATH "${link_origin}/${link_library_path}")
install(TARGETS link++ RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
