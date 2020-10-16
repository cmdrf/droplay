cmake_minimum_required(VERSION 3.5)
project(droplay)

find_package(SDL2 REQUIRED)

add_executable(droplay
	droplay.c
	opl3.c
	opl3.h
)

target_link_libraries(droplay SDL2::SDL2)
