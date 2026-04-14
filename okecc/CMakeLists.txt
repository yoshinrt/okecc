cmake_minimum_required(VERSION 3.16)
project(OkeccProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/O2 /Ot /Oi /Oy /GT /GL /utf-8)
    add_link_options(/LTCG)
else()
    add_compile_options(-O3 -march=native -ffast-math -flto -finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

set(SRCDIR "${CMAKE_CURRENT_SOURCE_DIR}")
include_directories(${SRCDIR})

add_library(okecc_common OBJECT "${SRCDIR}/okecc_sa.cpp")

file(GLOB ALL_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/../*.cpp")

foreach(source_file ${ALL_SOURCES})
    get_filename_component(exe_name ${source_file} NAME_WE)

    if(NOT "${exe_name}" STREQUAL "okecc_sa")
        add_executable(${exe_name} ${source_file} $<TARGET_OBJECTS:okecc_common>)
        
        # 出力先設定（バイナリディレクトリ直下に集める）
        set_target_properties(${exe_name} PROPERTIES 
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
        )
    endif()
endforeach()
