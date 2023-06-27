cmake_minimum_required(VERSION 3.25)
project(ppd_2023_par C)

set(CMAKE_C_STANDARD 23)

add_executable(ppd_2023_par main.c graphics/sdl_utils.c structures/grafo.c)
target_link_libraries(ppd_2023_par -lSDL2 -lSDL2_ttf m)