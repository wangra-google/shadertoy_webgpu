import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

// https://vitejs.dev/config/
export default defineConfig({
  plugins: [react()],
  optimizeDeps: {
    exclude: ['@webgpu/glslang']
  },
  server: {
    port: 3000,
    headers: {
      // These headers are required to enable SharedArrayBuffer in the browser,
      // which is used by multi-threaded WebAssembly modules.
      'Cross-Origin-Opener-Policy': 'same-origin',
      'Cross-Origin-Embedder-Policy': 'require-corp',
    },
  },
  build: {
    outDir: '../dist-web',
    emptyOutDir: true,
  },
});
