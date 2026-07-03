# -------------------------------------------------------------
# Third-Party Dependencies Configuration
# -------------------------------------------------------------

# Turn off unnecessary components to optimize Dawn/Tint building time
set(DAWN_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(DAWN_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_OPENGLES OFF CACHE BOOL "" FORCE)
set(DAWN_ENABLE_NULL OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(TINT_BUILD_AS_CONFORMANCE OFF CACHE BOOL "" FORCE)

# Ensure Tint features required for GLSL->WGSL translation are enabled
set(TINT_BUILD_SPV_READER ON CACHE BOOL "" FORCE)
set(TINT_BUILD_WGSL_WRITER ON CACHE BOOL "" FORCE)
set(TINT_BUILD_GLSL_WRITER ON CACHE BOOL "" FORCE)
set(TINT_BUILD_CMD_TOOLS ON CACHE BOOL "" FORCE)

# Add Google Dawn
add_subdirectory(dawn)

# In the future, you can add other libraries here, for example:
# add_subdirectory(imgui)
