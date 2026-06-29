import fs from 'fs';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const filesToCopy = [
  {
    src: path.join(__dirname, 'node_modules', '@webgpu', 'glslang', 'dist', 'web-devel', 'glslang.wasm'),
    dest: path.join(__dirname, 'public', 'glslang.wasm')
  },
  {
    src: path.join(__dirname, 'node_modules', 'twgsl', 'dist', 'twgsl.wasm'),
    dest: path.join(__dirname, 'public', 'twgsl.wasm')
  }
];

// Ensure public directory exists
const publicDir = path.join(__dirname, 'public');
if (!fs.existsSync(publicDir)) {
  fs.mkdirSync(publicDir, { recursive: true });
}

filesToCopy.forEach(({ src, dest }) => {
  try {
    if (fs.existsSync(src)) {
      fs.copyFileSync(src, dest);
      console.log(`Successfully copied ${path.basename(src)} to ${dest}`);
    } else {
      console.warn(`Source file not found: ${src}`);
    }
  } catch (err) {
    console.error(`Error copying ${path.basename(src)}:`, err);
  }
});
