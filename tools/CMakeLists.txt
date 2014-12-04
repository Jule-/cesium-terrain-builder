# The tools are not shared libraries
add_definitions(-DCPL_DISABLE_DLL)

include_directories(${Boost_INCLUDE_DIR})

set(TOOL_TARGETS commander ctb)

# Add the `ctb-tile` executable
add_executable(ctb-tile ctb-tile.cpp)
target_link_libraries(ctb-tile ${TOOL_TARGETS})

# Add the `ctb-export` executable
add_executable(ctb-export ctb-export.cpp)
target_link_libraries(ctb-export ${TOOL_TARGETS})

# Add the `ctb-patch` executable
add_executable(ctb-patch ctb-patch.cpp)
target_link_libraries(ctb-patch ${TOOL_TARGETS} ${Boost_LIBRARIES})

# Add the `ctb-info` executable
add_executable(ctb-info ctb-info.cpp)
target_link_libraries(ctb-info ${TOOL_TARGETS})

# Add the `ctb-extents` executable
add_executable(ctb-extents ctb-extents.cpp)
target_link_libraries(ctb-extents ${TOOL_TARGETS})

# Install the tools
set(TOOLS ctb-tile ctb-export ctb-patch ctb-info ctb-extents)
install(TARGETS ${TOOLS} DESTINATION bin)

# Copy dll dependencies for debug pupose (MSVC specific)
if (MSVC)
	foreach(loop_var ${TOOLS})
		add_custom_command(TARGET ${loop_var} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_BINARY_DIR}/src/\$\(Configuration\)" ${CMAKE_CURRENT_BINARY_DIR}/\$\(Configuration\))
		add_custom_command(TARGET ${loop_var} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${GDAL_INCLUDE_DIR}/../bin/gdal200.dll" ${CMAKE_CURRENT_BINARY_DIR}/\$\(Configuration\))
		add_custom_command(TARGET ${loop_var} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ZLIB_INCLUDE_DIR}/../lib/zlib.dll" ${CMAKE_CURRENT_BINARY_DIR}/\$\(Configuration\))
	endforeach(loop_var)
endif()
