# Project Rules - Shadertoy WebGPU (`shadertoy_webgpu`)

## Documentation Rules
1. **Documentation Location**: All future project documentation, technical specs, research notes, and guides MUST be saved in the `docs/` subfolder of the project root directory (`C:/src/shadertoy_webgpu/docs/`).
2. **Documentation Integrity**: When updating planning documents, guides, or specifications, do NOT truncate or delete previously established sections, specs, or diagrams unless there is a real design change or deprecation. Always append or merge additions transparently to preserve project history.

## Coding Style Rules
1. **Google C++ Coding Style**: Strictly follow the [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html). Pay special attention to avoiding static/global variables with non-trivial constructors/destructors, as detailed in [Static and Global Variables](https://google.github.io/styleguide/cppguide.html#Static_and_Global_Variables).
2. **Google Dawn Coding Style**: Where the Google C++ Style Guide differs from Google Dawn's codebase style, prefer the Dawn style. Notably:
   - Private class member variables must use the `m` prefix followed by CamelCase (e.g., `mDevice`, `mWidth`), rather than a trailing underscore (`member_name_`).
   - Struct member variables must use `camelCase` (e.g., `iResolution`, `viewProj`), rather than `snake_case`.
   - Local variables must use `camelCase` (e.g., `initialWidth`, `startTime`), rather than `snake_case`.
   - Header guards must be of the form `PATH_FILE_H_` (with a trailing underscore).

