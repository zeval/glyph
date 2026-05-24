cmake_minimum_required(VERSION 3.18)

project(glyph VERSION 0.1.0 LANGUAGES C CXX)

option(GLYPH_BUILD_PSP "Build for PSP using PSPDEV" OFF)
option(GLYPH_BUILD_TESTS "Build host tests" ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(glyph_document
  src/epub.cpp
  src/text_layout.cpp
  src/zip_archive.cpp
)

add_executable(glyph
  src/glyph_app.cpp
  src/glyph_app_books.cpp
  src/glyph_app_input.cpp
  src/glyph_app_render.cpp
  src/library.cpp
  src/main.cpp
  src/progress.cpp
  src/settings_store.cpp
)

target_include_directories(glyph_document PUBLIC src)
target_compile_features(glyph_document PUBLIC cxx_std_17)

target_include_directories(glyph PRIVATE src)
target_compile_features(glyph PRIVATE cxx_std_17)
target_link_libraries(glyph PRIVATE glyph_document)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(glyph_document PRIVATE
    -Wall
    -Wextra
    -Werror=return-type
    -fno-exceptions
    -fno-rtti
  )
  target_compile_options(glyph PRIVATE
    -Wall
    -Wextra
    -Werror=return-type
    -fno-exceptions
    -fno-rtti
  )
  if(NOT PSP AND NOT GLYPH_BUILD_PSP)
    target_compile_options(glyph_document PRIVATE -Wpedantic)
    target_compile_options(glyph PRIVATE -Wpedantic)
  endif()
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)
pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)
pkg_check_modules(ZLIB REQUIRED zlib)

function(glyph_normalize_pspdev_paths out_var)
  set(normalized)
  foreach(list_name IN LISTS ARGN)
    foreach(path IN LISTS ${list_name})
      if((PSP OR GLYPH_BUILD_PSP) AND path MATCHES "^/psp(/|$)" AND DEFINED ENV{PSPDEV})
        string(REGEX REPLACE "^/psp" "$ENV{PSPDEV}/psp" path "${path}")
      endif()
      list(APPEND normalized "${path}")
    endforeach()
  endforeach()
  if(normalized)
    list(REMOVE_DUPLICATES normalized)
  endif()
  set(${out_var} "${normalized}" PARENT_SCOPE)
endfunction()

glyph_normalize_pspdev_paths(GLYPH_SDL_INCLUDE_DIRS
  SDL2_INCLUDE_DIRS
  SDL2_IMAGE_INCLUDE_DIRS
  SDL2_TTF_INCLUDE_DIRS
)
glyph_normalize_pspdev_paths(GLYPH_SDL_LIBRARY_DIRS
  SDL2_LIBRARY_DIRS
  SDL2_IMAGE_LIBRARY_DIRS
  SDL2_TTF_LIBRARY_DIRS
)
glyph_normalize_pspdev_paths(GLYPH_ZLIB_INCLUDE_DIRS
  ZLIB_INCLUDE_DIRS
)
glyph_normalize_pspdev_paths(GLYPH_ZLIB_LIBRARY_DIRS
  ZLIB_LIBRARY_DIRS
)

target_include_directories(glyph PRIVATE ${GLYPH_SDL_INCLUDE_DIRS})
target_link_directories(glyph PRIVATE ${GLYPH_SDL_LIBRARY_DIRS})
target_include_directories(glyph_document PRIVATE ${GLYPH_ZLIB_INCLUDE_DIRS})
target_link_directories(glyph_document PRIVATE ${GLYPH_ZLIB_LIBRARY_DIRS})

set(GLYPH_SDL_COMPILE_OPTIONS
  ${SDL2_CFLAGS_OTHER}
  ${SDL2_IMAGE_CFLAGS_OTHER}
  ${SDL2_TTF_CFLAGS_OTHER}
)
set(GLYPH_SDL_LINK_OPTIONS
  ${SDL2_LDFLAGS_OTHER}
  ${SDL2_IMAGE_LDFLAGS_OTHER}
  ${SDL2_TTF_LDFLAGS_OTHER}
)

if(PSP OR GLYPH_BUILD_PSP)
  list(REMOVE_ITEM GLYPH_SDL_COMPILE_OPTIONS -pthread)
  list(REMOVE_ITEM GLYPH_SDL_LINK_OPTIONS -pthread)
endif()

target_compile_options(glyph PRIVATE ${GLYPH_SDL_COMPILE_OPTIONS})
target_link_options(glyph PRIVATE ${GLYPH_SDL_LINK_OPTIONS})
target_link_libraries(glyph PRIVATE
  ${SDL2_LIBRARIES}
  ${SDL2_IMAGE_LIBRARIES}
  ${SDL2_TTF_LIBRARIES}
)
target_link_libraries(glyph_document PUBLIC ${ZLIB_LIBRARIES})

target_compile_definitions(glyph PRIVATE
  GLYPH_VERSION="${PROJECT_VERSION}"
)

if(PSP OR GLYPH_BUILD_PSP)
  target_compile_definitions(glyph PRIVATE GLYPH_PLATFORM_PSP=1)

  create_pbp_file(
    TARGET glyph
    ICON_PATH ${CMAKE_CURRENT_SOURCE_DIR}/assets/psp/ICON0.PNG
    BACKGROUND_PATH NULL
    PREVIEW_PATH NULL
    TITLE glyph
    VERSION 00.01
  )
else()
  target_compile_definitions(glyph PRIVATE GLYPH_PLATFORM_HOST=1)
endif()

if(GLYPH_BUILD_TESTS AND NOT PSP AND NOT GLYPH_BUILD_PSP)
  include(CTest)
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_FOUND)
    add_test(
      NAME glyph.fixture_tiny_epub
      COMMAND
        ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/fixtures/make_tiny_epub.py
        --output ${CMAKE_CURRENT_BINARY_DIR}/fixtures/tiny.epub
        --force
        --check
    )
  else()
    message(STATUS "Python3 interpreter not found; EPUB fixture test is skipped.")
  endif()

  find_package(Catch2 3 QUIET)
  if(Catch2_FOUND)
    add_executable(glyph_tests
      src/library.cpp
      src/progress.cpp
      src/settings_store.cpp
      tests/epub_tests.cpp
      tests/progress_tests.cpp
      tests/settings_store_tests.cpp
      tests/smoke_tests.cpp
      tests/text_layout_tests.cpp
      tests/zip_tests.cpp
    )
    target_include_directories(glyph_tests PRIVATE src)
    target_compile_features(glyph_tests PRIVATE cxx_std_17)
    if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
      target_compile_options(glyph_tests PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Werror=return-type
      )
    endif()
    target_link_libraries(glyph_tests PRIVATE glyph_document Catch2::Catch2WithMain)
    add_test(NAME glyph.smoke COMMAND glyph_tests)
    set_tests_properties(glyph.smoke PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  else()
    message(STATUS "Catch2 v3 not found; host tests are skipped.")
  endif()
endif()
