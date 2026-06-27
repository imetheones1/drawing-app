cmake_minimum_required(VERSION 3.24)
project(drawingapp C)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(SDL_SHARED OFF CACHE BOOL "Disable shared library" FORCE)
set(SDL_STATIC ON CACHE BOOL "Enable static library" FORCE)
set(SDL_TESTS OFF CACHE BOOL "Disable SDL testing suites" FORCE)

add_subdirectory(vendored/SDL)

# e.g. add SDL_Image
# set(SDL_IMAGE_VENDORED ON CACHE BOOL "" FORCE)
# add_subdirectory(vendored/SDL_image)


set(SOURCES
    main.c
    # files go here
)


# embed images into code
function(embed_asset ASSET_PATH VARIABLE_NAME OUTPUT_LIST)
    get_filename_component(ASSET_ABS ${ASSET_PATH} ABSOLUTE)
    get_filename_component(ASSET_NAME ${ASSET_PATH} NAME_WLE)
    set(OUTPUT_C_FILE "${CMAKE_CURRENT_BINARY_DIR}/generated_${ASSET_NAME}.c")
    
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${ASSET_ABS})
    
    if(EXISTS ${ASSET_ABS})
        file(READ ${ASSET_ABS} HEX_CONTENT HEX)
        string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," C_ARRAY ${HEX_CONTENT})
        
        file(WRITE ${OUTPUT_C_FILE} 
            "const unsigned char ${VARIABLE_NAME}[] = {\n${C_ARRAY}\n};\n"
            "const unsigned int ${VARIABLE_NAME}_len = sizeof(${VARIABLE_NAME});\n"
        )
        set(${OUTPUT_LIST} ${${OUTPUT_LIST}} ${OUTPUT_C_FILE} PARENT_SCOPE)
    else()
        message(FATAL_ERROR "Asset target missing: ${ASSET_ABS}")
    endif()
endfunction()

set(EMBEDDED_ASSET_SOURCES "")
# e.g. embed_asset("assets/hello.png" "hello" EMBEDDED_ASSET_SOURCES)


add_executable(drawingapp ${SOURCES} ${EMBEDDED_ASSET_SOURCES})

target_include_directories(drawingapp PRIVATE include)

target_link_libraries(drawingapp PRIVATE SDL3::SDL3)

# e.g. add SDL_Image
# target_link_libraries(drawingapp PRIVATE SDL3_image::SDL3_image-static)


if(EMSCRIPTEN)
    target_compile_options(drawingapp PRIVATE -O3)
    target_link_options(drawingapp PRIVATE -O3)
    
    set_target_properties(drawingapp PROPERTIES SUFFIX ".html")
else()
    target_compile_options(drawingapp PRIVATE -O3 -march=native)
    target_link_options(drawingapp PRIVATE -O3)
endif()