# Shadertoy WebGPU Playground

A high-performance C++ playground for Shadertoy-like procedural geometry and fragment shaders, built on top of **WebGPU** (using Google Dawn) and **GLFW**.

This project supports two compilation targets:
1. **Native Desktop Target** (Windows, macOS, Linux)
2. **WebAssembly / HTML5 Target** (Runs in any WebGPU-enabled web browser)


---

## Project Structure

Here is the directory layout of the project, including the core C++ rendering engine and the React + Vite frontend:

```text
├── src/                      # C++ Engine Source
│   ├── core/                 # Platform-agnostic core rendering engine
│   │   ├── config.h          # Global configuration constants
│   │   ├── graphics_context.h # Abstract GraphicsContext interface
│   │   ├── math_utils.h      # 3D math structures (Vec3, Mat4) and utilities
│   │   ├── mesh.h / .cpp     # Procedural mesh generation (Quad, Cube, Sphere) and buffers
│   │   ├── renderer.h / .cpp # Main WebGPU rendering pipeline & pass management
│   │   ├── shadertoy_compat.h # GLSL Shadertoy compatibility header & macros
│   │   └── texture_manager.h / .cpp # Texture allocation and ping-pong buffering
│   └── platform/             # Platform-specific integration layers
│       ├── native/           # Native Desktop integration (Dawn, GLFW, glslang + Tint)
│       │   ├── main.cpp      # Desktop entry point, render loop, and input handling
│       │   ├── native_context.h / .cpp # Desktop GLFW window and Dawn WebGPU context
│       │   └── shader_compiler.h / .cpp # Offline GLSL to WGSL shader translator
│       └── web/              # Web platform integration (Emscripten WebAssembly)
│           ├── bindings.cpp  # Emscripten bindings exporting EngineCore to JS
│           ├── index.html    # Simple standalone HTML mockup
│           └── web_context.h / .cpp # Browser WebGPU surface and context management
│
└── web/                      # React + Vite Frontend Dashboard
    ├── public/               # Static assets and compiled WASM binaries (engine, glslang, twgsl)
    ├── src/                  # React TypeScript source code
    │   ├── components/       # UI components (Viewport, Editor, Diagnostics console)
    │   │   ├── Viewport.tsx  # Canvas hosting the WebGPU canvas & render loop
    │   │   ├── Editor.tsx    # CodeMirror 6 editor supporting GLSL/WGSL and pass tabs
    │   │   └── Diagnostics.tsx # Console panel displaying compilation log messages
    │   ├── utils/            # Frontend utilities
    │   │   ├── compiler.ts   # Client-side GLSL-to-WGSL compiler toolchain
    │   │   └── consoleCapture.ts # Console log interceptor for glslang errors
    │   ├── App.tsx           # Central React component managing state & orchestration
    │   ├── index.css         # Styling tokens and global styling system
    │   └── main.tsx          # Application entry point (initializes consoleCapture)
    ├── copy-wasm.js          # Node script to copy WASM binaries from node_modules
    ├── vite.config.ts        # Vite configuration (sets COOP/COEP headers for WASM)
    └── package.json          # Frontend scripts and dependencies
```

For detailed explanations of each module, see the respective README files:
- [src/README.md](file:///C:/src/shadertoy_webgpu/src/README.md)
  - [src/core/README.md](file:///C:/src/shadertoy_webgpu/src/core/README.md)
  - [src/platform/README.md](file:///C:/src/shadertoy_webgpu/src/platform/README.md)
    - [src/platform/native/README.md](file:///C:/src/shadertoy_webgpu/src/platform/native/README.md)
    - [src/platform/web/README.md](file:///C:/src/shadertoy_webgpu/src/platform/web/README.md)
- [web/README.md](file:///C:/src/shadertoy_webgpu/web/README.md)
  - [web/src/README.md](file:///C:/src/shadertoy_webgpu/web/src/README.md)
    - [web/src/components/README.md](file:///C:/src/shadertoy_webgpu/web/src/components/README.md)
    - [web/src/utils/README.md](file:///C:/src/shadertoy_webgpu/web/src/utils/README.md)
  - [web/public/README.md](file:///C:/src/shadertoy_webgpu/web/public/README.md)

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

### Running with Custom Shaders (Command Line)
You can load a custom shader file directly on startup by passing the file path as a command-line argument. The application will automatically detect the language based on the file extension and compile it:
*   **WGSL Shaders** (`.wgsl` extension): The shader is loaded directly. The entry point (e.g. `@fragment fn ...`) is parsed automatically.
*   **GLSL Shaders** (`.glsl`, `.frag`, `.vert`, etc.): The shader is treated as a Shadertoy-style GLSL shader. It is wrapped in the compatibility header and compiled to WGSL.

```bash
# Load a custom GLSL shader
build\Release\shadertoy_webgpu_native.exe samples\glsl\iq_primitives.glsl

# Load a custom WGSL shader
build\Release\shadertoy_webgpu_native.exe path\to\shader.wgsl
```

If the shader fails to compile, the detailed compilation errors are printed to `stderr` and the application exits immediately with code `-1`.

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
### 3. Setting Up & Running the React Frontend

The web target uses a modern **React + Vite** dashboard which wraps the WebGPU C++ engine, providing a live code editor, uniform controls, and real-time compilation logs.

#### Step A: Install Node.js Dependencies
Install the package dependencies from the project root (this will also copy `glslang.wasm` to the public folder automatically):
```bash
npm --prefix web install
```

*(Note: The compiled WebAssembly binaries `engine_wasm.js` and `engine_wasm.wasm` are now **automatically copied** to `web/public/` by CMake during the C++ build process in the previous section).*

#### Step B: Run the Development Server
Start the Vite local development server from the project root:
```bash
npm --prefix web run dev
```
Open your web browser and navigate to the URL printed in the terminal (typically `http://localhost:3000`).

---

### 4. Production Build (Web)
To compile and bundle the React web application for production deployment from the project root:
```bash
npm --prefix web run build
```
The optimized production assets will be outputted to the **`dist-web/`** folder in the project root.

To serve this folder, you can use any static web server. For example, using `http-server`:
```bash
npx http-server dist-web -p 8080
```

> [!IMPORTANT]
> **WebGPU Secure Context Requirement**
> WebGPU is only available in a **Secure Context**. When running the production build locally:
> * **Access it via:** **`http://localhost:8080`** or **`http://127.0.0.1:8080`** (the browser trusts local loopback addresses).
> * **Do NOT** access the app via your local network IP (e.g. `http://192.168.x.x:8080` or `http://100.x.x.x:8080`) over plain `http`. If you do, the browser will block WebGPU (`navigator.gpu` will be `undefined`).
> * If you must access the app from another device on your network, you must serve it over HTTPS. For example, you can generate local SSL certificates and run:
>   ```bash
>   npx http-server dist-web -p 8080 -S -C cert.pem -K key.pem
>   ```
>   And then navigate to `https://<your-lan-ip>:8080`.

---

## Interactive Controls (Testing Geometry Pipeline)
While running either the Native Desktop or Web application, you can interact with the rendering pipeline:
* **Geometry Selectors**: Switch the rendering target between a flat **2D Fullscreen Quad**, a **Procedural 3D Cube**, or a **Procedural 3D Sphere**.
  * **Native**: Use keys `[1]`, `[2]`, `[3]`, or `[ESC]` to exit.
  * **Web**: Click the buttons in the dashboard UI panel, or use the same keyboard shortcuts (`[1]`, `[2]`, `[3]`).
* **Mouse Interactions**: Click and drag your mouse inside the viewport to update the `iMouse` uniforms in real-time (allowing drag-based interactive shaders).
* **Live Shader Editing (Web)**: Edit the shader code directly in the code editor, select your language (WGSL), and press **Compile** (or `Alt + Enter`) to swap the pipeline on the fly.

---

## Interactive Controls (Testing Geometry Pipeline)
While running the application, you can use the following keyboard shortcuts to switch between different rendering pipelines:
*   `[1]` **Fullscreen Quad**: Renders the shader onto a flat 2D screen-space quad (standard Shadertoy mode).
*   `[2]` **Procedural Cube**: Renders the shader mapped onto a 3D rotating cube.
*   `[3]` **Procedural Sphere**: Renders the shader mapped onto a 3D rotating sphere.
*   `[ESC]` **Exit**: Closes the application.