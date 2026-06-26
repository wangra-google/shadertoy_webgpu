# Shadertoy WebGPU Playground

A high-performance C++ playground for Shadertoy-like procedural geometry and fragment shaders, built on top of **WebGPU** (using Google Dawn) and **GLFW**.

This project supports two compilation targets:
1. **Native Desktop Target** (Windows, macOS, Linux)
2. **WebAssembly / HTML5 Target** (Runs in any WebGPU-enabled web browser)

---

## Prerequisites

Before building, ensure you have the following installed on your system:
*   **CMake** (version 3.19 or higher)
*   **Git**
*   **Python 3** (required by Dawn's build scripts)
*   **depot_tools** (Google's package manager, required to fetch Dawn and its dependencies)
    *   Make sure `depot_tools` is added to your system's `PATH`.

---

## 1. Setup & Synchronizing Dependencies

This project uses `gclient` (from `depot_tools`) to manage Google Dawn and its dependencies cleanly without bloating the Git repository.

To fetch or update the dependencies:
1. Open a terminal in the `third_party` directory:
   ```bash
   cd third_party
   ```
2. Run `gclient sync` to download Dawn and its dependencies:
   ```bash
   gclient sync
   ```
   *Note: On Windows, make sure you run this from a command prompt with developer tools or Git Bash.*

---

## 2. Building the Native Desktop Target

Once dependencies are synchronized, you can build the native desktop application using CMake.

### Windows (Visual Studio / MSBuild)
You can build the project via the command line, or generate a Visual Studio 2022 solution (`.sln`) for interactive development and debugging.

#### Option A: Build via Command Line
1. Configure the project:
   ```bash
   cmake -B build
   ```
2. Compile the Release binary:
   ```bash
   cmake --build build --config Release
   ```
3. Run the executable:
   ```bash
   build\Release\shadertoy_webgpu_native.exe
   ```

#### Option B: Generate Visual Studio 2022 Solution
1. Generate the VS2022 solution:
   ```bash
   cmake -B build -G "Visual Studio 17 2022" -A x64
   ```
2. Open the generated solution:
   * Navigate to the `build/` directory and open `shadertoy_webgpu.sln` in Visual Studio 2022.
3. In Visual Studio:
   * Right-click the `shadertoy_webgpu_native` project in the Solution Explorer and select **Set as StartUp Project**.
   * Choose your build configuration (e.g., `Debug` or `Release`) and press **F5** to build and run the application.

### macOS & Linux (Ninja)
1. Configure the project using Ninja:
   ```bash
   cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
   ```
2. Compile the binary:
   ```bash
   cmake --build build
   ```
3. Run the executable:
   ```bash
   ./build/shadertoy_webgpu_native
   ```

---

## 3. Building the WebAssembly (HTML5) Target

The WebAssembly target compiles the C++ codebase into WASM and HTML using the **Emscripten SDK**.

### Setup Emscripten
1. Clone and install the Emscripten SDK (emsdk) if you haven't already:
   ```bash
   git clone https://github.com/emscripten-core/emsdk.git
   cd emsdk
   ./emsdk install latest
   ./emsdk activate latest
   ```
2. Activate the Emscripten environment in your terminal:
   *   **Windows**: `emsdk_env.bat`
   *   **macOS / Linux**: `source ./emsdk_env.sh`

### Compile to WebAssembly
1. Configure the project using the Emscripten CMake wrapper (`emcmake`):

   **For Windows users (with `depot_tools` in PATH)**:
   To prevent CMake from picking up the broken `depot_tools` version of `ninja`, run this from the **Developer Command Prompt for VS 2022** and explicitly pass the path to Visual Studio's native `ninja.exe`:
   ```bash
   emcmake cmake -B build-web -DCMAKE_BUILD_TYPE=Release -DCMAKE_MAKE_PROGRAM="C:/Program Files/Microsoft Visual Studio/2022/Professional/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe"
   ```
   *(If you are using the Community edition, replace `Professional` with `Community` in the path).*

   **For macOS / Linux (or Windows without `depot_tools`)**:
   ```bash
   emcmake cmake -B build-web -DCMAKE_BUILD_TYPE=Release
   ```

2. Build the Web target:
   ```bash
   cmake --build build-web
   ```
### Running in the Web Browser

Due to browser security policies (CORS), WebAssembly files cannot be loaded directly from your local hard drive (`file://` paths). They must be served over a local HTTP server.

1. **Start a local web server** in the project root directory:
   * **Using Python** (pre-installed on most systems):
     ```bash
     python -m http.server 8000
     ```
   * **Using Node.js** (alternative):
     ```bash
     npx http-server -p 8000
     ```

2. **Open your web browser** and navigate to:
   [http://localhost:8000/build-web/](http://localhost:8000/build-web/)

3. **Interact with the Web Application**:
   * **Dashboard UI**: A glassmorphic control panel on the top-left displays live performance statistics (FPS and elapsed time).
   * **Switch Geometries**: Click the buttons in the UI panel to dynamically map the shader onto different shapes:
     * **2D Fullscreen Quad**: Classic Shadertoy mode.
     * **Procedural Cube**: Maps the shader to a rotating 3D cube.
     * **Procedural Sphere**: Maps the shader to a rotating 3D sphere.
   * **Keyboard Shortcuts**: You can also press keys `[1]`, `[2]`, or `[3]` on your keyboard to switch geometries instantly.

---

## Interactive Controls (Testing Geometry Pipeline)
While running the application, you can use the following keyboard shortcuts to switch between different rendering pipelines:
*   `[1]` **Fullscreen Quad**: Renders the shader onto a flat 2D screen-space quad (standard Shadertoy mode).
*   `[2]` **Procedural Cube**: Renders the shader mapped onto a 3D rotating cube.
*   `[3]` **Procedural Sphere**: Renders the shader mapped onto a 3D rotating sphere.
*   `[ESC]` **Exit**: Closes the application.