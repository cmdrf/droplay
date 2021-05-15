cmake_minimum_required(VERSION 3.5)
project(droplay)

find_package(SDL2 REQUIRED)

add_executable(droplay
	droplay.c
	Nuked-OPL3/opl3.c
	Nuked-OPL3/opl3.h
)

target_include_directories(droplay PRIVATE ${SDL2_INCLUDE_DIRS} Nuked-OPL3)
target_link_libraries(droplay PRIVATE ${SDL2_LIBRARIES})
