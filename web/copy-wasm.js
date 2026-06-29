import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const publicDir = path.join(__dirname, 'public');
if (!fs.existsSync(publicDir)) {
  fs.mkdirSync(publicDir, { recursive: true });
}

// Helper to copy file with error handling
function copyFile(src, dest) {
  try {
    if (fs.existsSync(src)) {
      fs.copyFileSync(src, dest);
      console.log(`Successfully copied ${path.basename(src)} to ${dest}`);
    } else {
      throw new Error(`Source file not found: ${src}`);
    }
  } catch (err) {
    console.error(`Error copying ${path.basename(src)}:`, err.message || err);
    process.exit(1); // Exit with error to fail the postinstall step if files are missing
  }
}

// 1. Copy glslang.wasm from node_modules
const glslangSrc = path.join(__dirname, 'node_modules', '@webgpu', 'glslang', 'dist', 'web-devel', 'glslang.wasm');
const glslangDest = path.join(publicDir, 'glslang.wasm');
copyFile(glslangSrc, glslangDest);

// 2. Copy twgsl.js and twgsl.wasm from node_modules
const twgslJsSrc = path.join(__dirname, 'node_modules', 'twgsl', 'dist', 'twgsl.js');
const twgslJsDest = path.join(publicDir, 'twgsl.js');
copyFile(twgslJsSrc, twgslJsDest);

const twgslWasmSrc = path.join(__dirname, 'node_modules', 'twgsl', 'dist', 'twgsl.wasm');
const twgslWasmDest = path.join(publicDir, 'twgsl.wasm');
copyFile(twgslWasmSrc, twgslWasmDest);
