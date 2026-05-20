cmake_minimum_required(VERSION 3.10)
project(Cena3D)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Baixa stb_image.h automaticamente se ainda nao existir
if(NOT EXISTS "${CMAKE_SOURCE_DIR}/include/stb_image.h")
  message(STATUS "Baixando stb_image.h...")
  file(DOWNLOAD
    "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h"
    "${CMAKE_SOURCE_DIR}/include/stb_image.h"
    STATUS DOWNLOAD_STATUS
  )
  list(GET DOWNLOAD_STATUS 0 DOWNLOAD_STATUS_CODE)
  if(NOT DOWNLOAD_STATUS_CODE EQUAL 0)
    message(WARNING "Falha ao baixar stb_image.h. Coloque o arquivo manualmente em include/")
  endif()
endif()

include(FetchContent)

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG 3.4
)

FetchContent_Declare(
  glm
  GIT_REPOSITORY https://github.com/g-truc/glm.git
  GIT_TAG master
)

FetchContent_MakeAvailable(glfw glm)

include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/common)
include_directories(${CMAKE_SOURCE_DIR}/include/glad)
include_directories(${glm_SOURCE_DIR})

add_compile_options(-Wno-pragmas)

if(WIN32)
    set(OPENGL_LIBS opengl32)
elseif(APPLE)
    find_library(OpenGL_LIBRARY OpenGL)
    set(OPENGL_LIBS ${OpenGL_LIBRARY})
else()
    find_package(OpenGL REQUIRED)
    set(OPENGL_LIBS ${OPENGL_gl_LIBRARY})
endif()

set(GLAD_C_FILE "${CMAKE_SOURCE_DIR}/common/glad.c")

if (NOT EXISTS ${GLAD_C_FILE})
    message(FATAL_ERROR "glad.c não encontrado em common/")
endif()

add_executable(Objeto3D src/Objeto3D.cpp ${GLAD_C_FILE})
target_include_directories(Objeto3D PRIVATE ${CMAKE_SOURCE_DIR}/include/glad ${glm_SOURCE_DIR})
target_link_libraries(Objeto3D glfw ${OPENGL_LIBS})

add_custom_command(TARGET Objeto3D POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:Objeto3D>/assets")
