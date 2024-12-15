cmake_minimum_required(VERSION 3.25) 
project(DolmexicaInfinite CXX)               
set(CMAKE_CXX_STANDARD 17)            
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include("${CMAKE_SOURCE_DIR}/../../addons/prism/cmake/prism.cmake" REQUIRED)

# Add source/header files to project
file(GLOB_RECURSE SOURCES ../*.cpp SOURCES ../*.h SOURCES ../*.rc)
list(FILTER SOURCES EXCLUDE REGEX ".*web.*")
list(FILTER SOURCES EXCLUDE REGEX ".*/build/.*")
list(FILTER SOURCES EXCLUDE REGEX ".*/test/.*")
list(FILTER SOURCES EXCLUDE REGEX ".*/assets*")

add_link_options(/NODEFAULTLIB:libcmt.lib)
add_link_options(/IGNORE:4099,4286,4098)
add_link_options(/OPT:REF)
add_link_options(/OPT:ICF)

# Define exe
add_executable(DolmexicaInfinite ${SOURCES})

# Link with libraries
target_link_libraries(DolmexicaInfinite prism)
target_link_libraries(DolmexicaInfinite SDL2)
target_link_libraries(DolmexicaInfinite SDL2main)
target_link_libraries(DolmexicaInfinite SDL2_image)
target_link_libraries(DolmexicaInfinite SDL2_mixer)
target_link_libraries(DolmexicaInfinite SDL2_ttf)
target_link_libraries(DolmexicaInfinite enet)
target_link_libraries(DolmexicaInfinite freetype)
target_link_libraries(DolmexicaInfinite glew)
target_link_libraries(DolmexicaInfinite curl)
target_link_libraries(DolmexicaInfinite libjpeg)
target_link_libraries(DolmexicaInfinite libpng)
target_link_libraries(DolmexicaInfinite libwebp)
target_link_libraries(DolmexicaInfinite zstd)
target_link_libraries(DolmexicaInfinite zlib)
target_link_libraries(DolmexicaInfinite crypt32)
target_link_libraries(DolmexicaInfinite wldap32)
target_link_libraries(DolmexicaInfinite ws2_32)
target_link_libraries(DolmexicaInfinite winmm)
target_link_libraries(DolmexicaInfinite OpenGL32)
target_link_libraries(DolmexicaInfinite version)

# Define preprocessor definitions
target_compile_definitions(DolmexicaInfinite PUBLIC UNICODE)
target_compile_definitions(DolmexicaInfinite PUBLIC _UNICODE)

set_property(TARGET DolmexicaInfinite PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
target_compile_options(DolmexicaInfinite PRIVATE /Gy)

set_target_properties(DolmexicaInfinite PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/..)
set_target_properties(DolmexicaInfinite PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_SOURCE_DIR}/..)

# Copy over DLLs
file(GLOB DLLS "${CMAKE_SOURCE_DIR}/../../addons/prism/windows/vs17/DLL/*.dll")
foreach(DLL ${DLLS})
  add_custom_command(TARGET DolmexicaInfinite POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${DLL}
    $<TARGET_FILE_DIR:DolmexicaInfinite>)
endforeach()

# Optimize for release build
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  target_compile_options(DolmexicaInfinite PRIVATE /O2)
endif()