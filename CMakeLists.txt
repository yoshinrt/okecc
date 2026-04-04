cmake_minimum_required(VERSION 3.16)
project(OkeccProject LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# コンパイルオプションの設定
if(MSVC)
    add_compile_options(
        /O2             # 実行速度の最適化
        /Ot             # 実行速度をコードサイズより優先
        /Oi             # 組み込み関数を使用
        /Oy             # フレームポインタの省略
        /GT             # ファイバーセーフなスレッドローカルストレージ
        /GL             # 全プログラムの最適化（リンク時最適化）
        /utf-8          # ソース・実行ファイル共にUTF-8として処理
    )
    # リンク時の最適化 (LTO/LTCG) も有効にする
    add_link_options(/LTCG)
else()
    # GCC/Clang用（一応残しておく場合）
    add_compile_options(-O3 -march=native -ffast-math -flto -finput-charset=UTF-8 -fexec-charset=UTF-8)
endif()

# インクルードパスの設定
set(SRCDIR "${CMAKE_CURRENT_SOURCE_DIR}/okecc")
include_directories(${SRCDIR})

# 共通オブジェクト
add_library(okecc_common OBJECT "${SRCDIR}/okecc_sa.cpp")

# 全ての .cpp を個別ターゲットにする
file(GLOB ALL_SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*.cpp")

foreach(source_file ${ALL_SOURCES})
    get_filename_component(exe_name ${source_file} NAME_WE)

    if(NOT "${exe_name}" STREQUAL "okecc_sa")
        add_executable(${exe_name} ${source_file} $<TARGET_OBJECTS:okecc_common>)
        
        # 出力先を build 直下に固定
        set_target_properties(${exe_name} PROPERTIES 
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_DEBUG "${CMAKE_CURRENT_BINARY_DIR}"
            RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_CURRENT_BINARY_DIR}"
        )
    endif()
endforeach()
