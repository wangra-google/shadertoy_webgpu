cmake_minimum_required(VERSION 3.20)
project(shadertoy_webgpu C CXX)

# Enable folder support in IDEs (like Visual Studio)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(EMSCRIPTEN)
    # -------------------------------------------------------------
    # WEB / EMSCRIPTEN TARGET
    # -------------------------------------------------------------
    message(STATUS "Configuring for WebAssembly build using Emscripten")

    add_executable(engine_wasm
        src/core/config.h
        src/core/mesh.cpp
        src/core/mesh.h
        src/core/renderer.cpp
        src/core/renderer.h
        src/core/texture_manager.cpp
        src/core/texture_manager.h
        src/core/graphics_context.h
        src/core/math_utils.h
        src/platform/web/web_context.cpp
        src/platform/web/web_context.h
        src/platform/web/bindings.cpp
    )

    # Organize files into folders in Visual Studio Solution Explorer
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "Source Files" FILES
        src/core/config.h
        src/core/mesh.cpp
        src/core/mesh.h
        src/core/renderer.cpp
        src/core/renderer.h
        src/core/texture_manager.cpp
        src/core/texture_manager.h
        src/core/graphics_context.h
        src/core/math_utils.h
        src/platform/web/web_context.cpp
        src/platform/web/web_context.h
        src/platform/web/bindings.cpp
    )

    target_include_directories(engine_wasm PRIVATE 
        src
    )

    target_compile_options(engine_wasm PRIVATE
        "--use-port=emdawnwebgpu"
    )

    # Enable Emscripten WebGPU and Embind bindings flags
    target_link_options(engine_wasm PRIVATE
        "--use-port=emdawnwebgpu"
        "--bind"
        "-sWASM=1"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
        "-sASYNCIFY=1"
        "-sMODULARIZE=1"
        "-sEXPORT_NAME=createEngineModule"
    )

    # Copy index.html to the build directory
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform/web/index.html
        ${CMAKE_CURRENT_BINARY_DIR}/index.html
        COPYONLY
    )

    # Post-build step to copy WASM binaries directly to the React public folder
    add_custom_command(TARGET engine_wasm POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_SOURCE_DIR}/web/public
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE_DIR:engine_wasm>/engine_wasm.js
            ${CMAKE_CURRENT_SOURCE_DIR}/web/public/engine_wasm.js
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE_DIR:engine_wasm>/engine_wasm.wasm
            ${CMAKE_CURRENT_SOURCE_DIR}/web/public/engine_wasm.wasm
        COMMENT "Copying WebAssembly binaries to React public folder..."
    )

else()
    # -------------------------------------------------------------
    # NATIVE DESKTOP TARGET
    # -------------------------------------------------------------
    message(STATUS "Configuring for Native Desktop build")

    # 1. Local Google Dawn (paths needed for includes)
    set(dawn_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/dawn")
    set(dawn_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/third_party/dawn")

    # Recursively organize all third-party targets into a "ThirdParty" folder in IDEs
    function(set_directory_folder dir folder)
        get_property(targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)
        foreach(target ${targets})
            set_target_properties(${target} PROPERTIES FOLDER ${folder})
        endforeach()
        
        get_property(subdirs DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)
        foreach(subdir ${subdirs})
            set_directory_folder(${subdir} ${folder})
        endforeach()
    endfunction()

    # Include the third-party directory. This will automatically configure Dawn and any other dependencies.
    add_subdirectory(third_party)
    
    # Automatically group all targets defined in third_party (and its subdirectories) under "ThirdParty"
    set_directory_folder(third_party "ThirdParty")

    # Create the native desktop executable
    add_executable(shadertoy_webgpu_native
        src/core/config.h
        src/core/mesh.cpp
        src/core/mesh.h
        src/core/renderer.cpp
        src/core/renderer.h
        src/core/texture_manager.cpp
        src/core/texture_manager.h
        src/core/graphics_context.h
        src/core/math_utils.h
        src/platform/native/native_context.cpp
        src/platform/native/native_context.h
        src/platform/native/main.cpp
        src/platform/native/shader_compiler.cpp
        src/platform/native/shader_compiler.h
        
        # Web files (included for reference in VS, but not compiled)
        src/platform/web/web_context.cpp
        src/platform/web/web_context.h
        src/platform/web/bindings.cpp
    )

    # Organize files into folders in Visual Studio Solution Explorer
    source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "Source Files" FILES
        src/core/config.h
        src/core/mesh.cpp
        src/core/mesh.h
        src/core/renderer.cpp
        src/core/renderer.h
        src/core/texture_manager.cpp
        src/core/texture_manager.h
        src/core/graphics_context.h
        src/core/math_utils.h
        src/platform/native/native_context.cpp
        src/platform/native/native_context.h
        src/platform/native/main.cpp
        src/platform/native/shader_compiler.cpp
        src/platform/native/shader_compiler.h
        src/platform/web/web_context.cpp
        src/platform/web/web_context.h
        src/platform/web/bindings.cpp
    )

    # Exclude Web C++ source files from compiling in the native desktop target
    set_source_files_properties(
        src/platform/web/web_context.cpp
        src/platform/web/bindings.cpp
        PROPERTIES HEADER_FILE_ONLY TRUE
    )

    # Move any automatically-added third-party headers into a separate folder
    source_group("ThirdParty Headers" REGULAR_EXPRESSION "third_party/.*")

    # Include directories for Dawn headers
    target_include_directories(shadertoy_webgpu_native PRIVATE
        src
        ${dawn_SOURCE_DIR}/include
        ${dawn_SOURCE_DIR}/src
        ${dawn_SOURCE_DIR}/third_party/glslang/src
        ${dawn_BINARY_DIR}/gen/include
    )

    # Link libraries: dawn_native handles WebGPU logical layer,
    # dawn_proc maps entry points, and glfw operates window loops.
    target_link_libraries(shadertoy_webgpu_native PRIVATE
        dawn_native
        dawn_proc
        glfw
        glslang
        glslang-default-resource-limits
        SPIRV
        tint_api
    )

    # Link platform libraries for Windows Win32 surface support
    if(WIN32)
        target_link_libraries(shadertoy_webgpu_native PRIVATE
            gdi32
            user32
            shell32
        )
    endif()

endif()
