cmake_minimum_required(VERSION 3.17)
project(powermenu LANGUAGES CXX)

find_package(LawnchPluginApi REQUIRED)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(powermenu SHARED
	powermenu.cpp
)

target_link_libraries(powermenu PRIVATE
	Lawnch::LawnchPluginApi
)

set_target_properties(powermenu PROPERTIES
	PREFIX ""
	LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

install(TARGETS powermenu
	DESTINATION .
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/pinfo
	DESTINATION .
)
