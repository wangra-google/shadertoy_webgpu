import fs from 'fs';
import path from 'path';
import https from 'https';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const publicDir = path.join(__dirname, 'public');
if (!fs.existsSync(publicDir)) {
  fs.mkdirSync(publicDir, { recursive: true });
}

// Helper to download a file using Node's native https module (no Node 18 fetch requirement)
function downloadFile(url, dest) {
  return new Promise((resolve, reject) => {
    console.log(`Downloading ${url} to ${dest}...`);
    const file = fs.createWriteStream(dest);
    https.get(url, (response) => {
      if (response.statusCode !== 200) {
        reject(new Error(`Failed to download ${url}: Status Code ${response.statusCode}`));
        return;
      }
      response.pipe(file);
      file.on('finish', () => {
        file.close();
        console.log(`Successfully downloaded ${path.basename(dest)}`);
        resolve();
      });
    }).on('error', (err) => {
      fs.unlink(dest, () => {}); // Delete the file on error
      reject(err);
    });
  });
}

// Helper to copy or download file
async function prepareFile(src, dest, downloadUrl) {
  try {
    if (fs.existsSync(src)) {
      fs.copyFileSync(src, dest);
      console.log(`Successfully copied ${path.basename(src)} to ${dest}`);
    } else if (fs.existsSync(dest)) {
      console.warn(`Source file not found: ${src}, but destination already exists at ${dest}. Skipping.`);
    } else if (downloadUrl) {
      await downloadFile(downloadUrl, dest);
    } else {
      throw new Error(`Source file not found: ${src} and destination does not exist.`);
    }
  } catch (err) {
    console.error(`Error preparing ${path.basename(dest)}:`, err.message || err);
    process.exit(1);
  }
}

async function main() {
  // 1. Copy glslang.wasm from node_modules
  const glslangSrc = path.join(__dirname, 'node_modules', '@webgpu', 'glslang', 'dist', 'web-devel', 'glslang.wasm');
  const glslangDest = path.join(publicDir, 'glslang.wasm');
  await prepareFile(glslangSrc, glslangDest);

  // Copy glslang.wasm to public/assets because in the production build,
  // @webgpu/glslang looks for the wasm file relative to the bundled JS file (which is in assets/).
  const assetsDir = path.join(publicDir, 'assets');
  if (!fs.existsSync(assetsDir)) {
    fs.mkdirSync(assetsDir, { recursive: true });
  }
  const glslangDestAssets = path.join(assetsDir, 'glslang.wasm');
  await prepareFile(glslangSrc, glslangDestAssets);

  // 2. Prepare twgsl.js and twgsl.wasm (using versioned CDN if missing from node_modules)
  const twgslJsSrc = path.join(__dirname, 'node_modules', 'twgsl', 'dist', 'twgsl.js');
  const twgslJsDest = path.join(publicDir, 'twgsl.js');
  await prepareFile(twgslJsSrc, twgslJsDest, 'https://cdn.babylonjs.com/v7.0.0/twgsl/twgsl.js');

  const twgslWasmSrc = path.join(__dirname, 'node_modules', 'twgsl', 'dist', 'twgsl.wasm');
  const twgslWasmDest = path.join(publicDir, 'twgsl.wasm');
  await prepareFile(twgslWasmSrc, twgslWasmDest, 'https://cdn.babylonjs.com/v7.0.0/twgsl/twgsl.wasm');
}

main();
