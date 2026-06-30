# Frontend Module (`web`)

This directory contains the frontend application for the **Shadertoy WebGPU Studio**. It is a React TypeScript project configured and bundled using Vite.

## Directory Structure

The frontend is divided into the following directories and files:

- **[src/](file:///C:/src/shadertoy_webgpu/web/src/README.md)**: The React TypeScript application source code.
  - **[components/](file:///C:/src/shadertoy_webgpu/web/src/components/README.md)**: React UI components (Viewport, Editor, Diagnostics).
  - **[utils/](file:///C:/src/shadertoy_webgpu/web/src/utils/README.md)**: Browser-side shader compilation and console logging utilities.
- **[public/](file:///C:/src/shadertoy_webgpu/web/public/README.md)**: Contains static assets and pre-compiled WebAssembly binaries (`engine_wasm`, `glslang`, `twgsl`) served directly to the browser.
- **[index.html](file:///C:/src/shadertoy_webgpu/web/index.html)**: The HTML entry point for the web application.
- **[vite.config.ts](file:///C:/src/shadertoy_webgpu/web/vite.config.ts)**: Configuration for the Vite bundler and development server.
- **[copy-wasm.js](file:///C:/src/shadertoy_webgpu/web/copy-wasm.js)**: A post-install script to copy WASM binaries from `node_modules` to the `public` folder.
- **[package.json](file:///C:/src/shadertoy_webgpu/web/package.json)**: Declares scripts, dependencies, and project metadata.
- **[tsconfig.json](file:///C:/src/shadertoy_webgpu/web/tsconfig.json)**: TypeScript compiler configuration.

---

## File Overview (Root Level)

### [index.html](file:///C:/src/shadertoy_webgpu/web/index.html)
The main HTML wrapper for the React application.
- Imports the Google Fonts ('Outfit' for UI, 'Fira Code' for the editor) and sets the favicon.
- Defines the `#root` element where React mounts.
- Loads the WebAssembly glue scripts (`/twgsl.js` and `/engine_wasm.js`) from the public directory.
- Loads the TypeScript entry point (`/src/main.tsx`).

### [vite.config.ts](file:///C:/src/shadertoy_webgpu/web/vite.config.ts)
Vite configuration file.
- Applies `@vitejs/plugin-react` for React compiling.
- Excludes `@webgpu/glslang` from dependency pre-bundling.
- Sets the development server port to `3000`.
- **Security Headers**: Configures `Cross-Origin-Opener-Policy: same-origin` and `Cross-Origin-Embedder-Policy: require-corp` headers. These are **strictly required** by modern web browsers to enable `SharedArrayBuffer` support, which is necessary for multi-threaded WebAssembly execution.
- Configures the build output directory to `../dist-web` (placing the compiled web app in the project's root `dist-web` directory).

### [copy-wasm.js](file:///C:/src/shadertoy_webgpu/web/copy-wasm.js)
A utility Node.js script run automatically after package installation.
- Copies the `glslang.wasm` and `twgsl` (`twgsl.js` and `twgsl.wasm`) compiler binaries out of the `node_modules` directory and places them in the `public` directory.
- This ensures that these large WebAssembly files can be served directly as static assets to the client browser.

### [package.json](file:///C:/src/shadertoy_webgpu/web/package.json)
Manages npm scripts and dependencies.
- **Scripts**:
  - `npm run dev`: Starts the Vite development server on port 3000.
  - `npm run build`: Compiles TypeScript and builds the production bundle into `../dist-web`.
  - `npm run preview`: Previews the production build locally.
  - `postinstall`: Runs `node copy-wasm.js` automatically after `npm install`.
- **Key Dependencies**:
  - `react`, `react-dom`: Core UI framework.
  - `@uiw/react-codemirror`: CodeMirror editor wrapper.
  - `@webgpu/glslang`, `twgsl`: Browser-side shader compilation toolchain.

### [tsconfig.json](file:///C:/src/shadertoy_webgpu/web/tsconfig.json)
Configures TypeScript compilation settings. It targets `ES2022`, uses modern `ESNext` module resolution, enables strict type checking, and configures React's JSX transform.

---

## Setup and Development

To run the web playground locally:

1. Navigate to the `web` directory:
   ```bash
   cd web
   ```
2. Install dependencies (this will automatically trigger `copy-wasm.js` to copy the WASM files):
   ```bash
   npm install
   ```
3. Start the development server:
   ```bash
   npm run dev
   ```
4. Open your browser and navigate to `http://localhost:3000`.
