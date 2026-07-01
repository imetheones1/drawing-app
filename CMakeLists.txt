cmake_minimum_required(VERSION 4.2.1)
project(drawingapp C)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(BUILD_SHARED_LIBS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

set(SDL3IMAGE_VENDORED ON)
set(SDLTTF_VENDORED ON)

add_subdirectory(vendored/SDL)
add_subdirectory(vendored/SDL_image)
add_subdirectory(vendored/SDL_ttf)


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
embed_asset("fonts/Roboto-Regular.ttf" "asset_font_roboto" EMBEDDED_ASSET_SOURCES)


add_executable(drawingapp "main.c")

target_sources(drawingapp PRIVATE ${EMBEDDED_ASSET_SOURCES})
target_sources(drawingapp PRIVATE "assets.c")
target_sources(drawingapp PRIVATE "canvas.c")
target_sources(drawingapp PRIVATE "Clay/clay_renderer_SDL3.c")

target_include_directories(drawingapp PRIVATE "include" "Clay")

target_link_libraries(drawingapp PRIVATE SDL3::SDL3 SDL3_image::SDL3_image SDL3_ttf::SDL3_ttf)

if(EMSCRIPTEN)
    target_compile_options(drawingapp PRIVATE -O3)

    target_link_options(drawingapp PRIVATE 
        -O3 
        "-sALLOW_MEMORY_GROWTH=1"
        "-sINITIAL_MEMORY=67108864"
        "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/shell.html" 
    )
    
    set_target_properties(drawingapp PROPERTIES SUFFIX ".html")
else()
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
    
    target_compile_options(drawingapp PRIVATE 
        -O3 
        -ffunction-sections 
        -fdata-sections
    )
    
    target_link_options(drawingapp PRIVATE 
        -flto 
        -Wl,--gc-sections
        $<$<C_COMPILER_ID:GNU>:-static>
        $<$<C_COMPILER_ID:Clang>:-static>
    )
endif()