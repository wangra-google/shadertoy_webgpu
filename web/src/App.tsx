import { useState, useEffect, useRef } from 'react';
import { Viewport } from './components/Viewport';
import { Editor } from './components/Editor';
import { Diagnostics, LogMessage } from './components/Diagnostics';
import { initCompiler, compileGLSLToWGSL } from './utils/compiler';

const DEFAULT_GLSL = `void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    // Normalized pixel coordinates (from 0 to 1)
    vec2 uv = fragCoord / iResolution.xy;

    // Time varying pixel color
    vec3 col = 0.5 + 0.5 * cos(iTime + uv.xyx + vec3(0, 2, 4));

    // Output to screen
    fragColor = vec4(col, 1.0);
}`;

const DEFAULT_WGSL = `struct ShadertoyUniforms {
    iResolution : vec3<f32>,
    iTime       : f32,
    iTimeDelta  : f32,
    iFrameRate  : f32,
    iFrame      : i32,
    pad0        : u32,
    iMouse      : vec4<f32>,
    iDate       : vec4<f32>,
};

struct CameraUniforms {
    uViewProj       : mat4x4<f32>,
    uModel          : mat4x4<f32>,
    uNormalMatrix   : mat4x4<f32>,
    uCameraPosition : vec3<f32>,
    padding         : f32,
};

@group(0) @binding(0) var<uniform> u_shadertoy : ShadertoyUniforms;
@group(0) @binding(1) var<uniform> u_camera    : CameraUniforms;

struct VertexOutput {
    @builtin(position) position : vec4<f32>,
    @location(0) normal   : vec3<f32>,
    @location(1) uv       : vec2<f32>,
    @location(2) worldPos : vec3<f32>,
};

@fragment
fn main(input : VertexOutput) -> @location(0) vec4<f32> {
    let uv = input.uv;
    let time = u_shadertoy.iTime;
    
    // Create a beautiful plasma wave
    let col = 0.5 + 0.5 * cos(vec3<f32>(time) + uv.xyx + vec3<f32>(0.0, 2.0, 4.0));
    
    return vec4<f32>(col, 1.0);
}`;

const BUFFER_WGSL_TEMPLATE = (passName: string) => `struct ShadertoyUniforms {
    iResolution : vec3<f32>,
    iTime       : f32,
    iTimeDelta  : f32,
    iFrameRate  : f32,
    iFrame      : i32,
    pad0        : u32,
    iMouse      : vec4<f32>,
    iDate       : vec4<f32>,
};

struct CameraUniforms {
    uViewProj       : mat4x4<f32>,
    uModel          : mat4x4<f32>,
    uNormalMatrix   : mat4x4<f32>,
    uCameraPosition : vec3<f32>,
    padding         : f32,
};

@group(0) @binding(0) var<uniform> u_shadertoy : ShadertoyUniforms;
@group(0) @binding(1) var<uniform> u_camera    : CameraUniforms;

@group(1) @binding(0) var s_sampler  : sampler;
@group(1) @binding(1) var iChannel0 : texture_2d<f32>;
@group(1) @binding(2) var iChannel1 : texture_2d<f32>;
@group(1) @binding(3) var iChannel2 : texture_2d<f32>;
@group(1) @binding(4) var iChannel3 : texture_2d<f32>;

struct VertexOutput {
    @builtin(position) position : vec4<f32>,
    @location(0) normal   : vec3<f32>,
    @location(1) uv       : vec2<f32>,
    @location(2) worldPos : vec3<f32>,
};

@fragment
fn main(input : VertexOutput) -> @location(0) vec4<f32> {
    let uv = input.uv;
    
    // Simple feedback or procedural generator for ${passName}
    let col = vec3<f32>(uv.x, uv.y, 0.5 + 0.5 * sin(u_shadertoy.iTime));
    
    return vec4<f32>(col, 1.0);
}`;

const BUFFER_GLSL_TEMPLATE = (passName: string) => `void mainImage(out vec4 fragColor, in vec2 fragCoord) {
    vec2 uv = fragCoord / iResolution.xy;
    
    // Simple feedback or procedural generator for ${passName}
    vec3 col = vec3(uv.x, uv.y, 0.5 + 0.5 * sin(iTime));
    
    fragColor = vec4(col, 1.0);
}`;

const PASS_NAMES = ['BufferA', 'BufferB', 'BufferC', 'BufferD', 'Image'];

const detectLanguage = (code: string): 'glsl' | 'wgsl' => {
  const cleanCode = code
    .replace(/\/\*[\s\S]*?\*\//g, '')
    .replace(/\/\/.*/g, '');

  const wgslPatterns = [
    /@fragment|@vertex|@compute/i,
    /fn\s+\w+\s*\(/,
    /vec\d<[fui]32>/,
    /\bvar\b|\blet\b/
  ];

  const glslPatterns = [
    /#version/i,
    /void\s+main/i,
    /\bvec2\b|\bvec3\b|\bvec4\b/,
    /\buniform\b|\bvarying\b|\battribute\b/
  ];

  let wgslScore = 0;
  let glslScore = 0;

  for (const pattern of wgslPatterns) {
    if (pattern.test(cleanCode)) wgslScore++;
  }

  for (const pattern of glslPatterns) {
    if (pattern.test(cleanCode)) glslScore++;
  }

  return wgslScore >= glslScore ? 'wgsl' : 'glsl';
};

const generateNoiseTexture = () => {
  const size = 256 * 256 * 4;
  const data = new Uint8Array(size);
  for (let i = 0; i < size; i += 4) {
    const val = Math.floor(Math.random() * 256);
    data[i] = val;
    data[i + 1] = val;
    data[i + 2] = val;
    data[i + 3] = 255;
  }
  return data;
};

const generateCheckerboardTexture = () => {
  const size = 256 * 256 * 4;
  const data = new Uint8Array(size);
  for (let y = 0; y < 256; ++y) {
    for (let x = 0; x < 256; ++x) {
      const i = (y * 256 + x) * 4;
      const checkX = Math.floor(x / 16) % 2;
      const checkY = Math.floor(y / 16) % 2;
      const val = (checkX ^ checkY) ? 255 : 0;
      data[i] = val;
      data[i + 1] = val;
      data[i + 2] = val;
      data[i + 3] = 255;
    }
  }
  return data;
};

export default function App() {
  const [activePass, setActivePass] = useState<string>('Image');
  
  const [passCode, setPassCode] = useState<Record<string, string>>({
    BufferA: BUFFER_WGSL_TEMPLATE('BufferA'),
    BufferB: BUFFER_WGSL_TEMPLATE('BufferB'),
    BufferC: BUFFER_WGSL_TEMPLATE('BufferC'),
    BufferD: BUFFER_WGSL_TEMPLATE('BufferD'),
    Image: DEFAULT_WGSL
  });

  const [passLanguage, setPassLanguage] = useState<Record<string, 'glsl' | 'wgsl'>>({
    BufferA: 'wgsl',
    BufferB: 'wgsl',
    BufferC: 'wgsl',
    BufferD: 'wgsl',
    Image: 'wgsl'
  });

  const [passEnabled, setPassEnabled] = useState<Record<string, boolean>>({
    BufferA: false,
    BufferB: false,
    BufferC: false,
    BufferD: false,
    Image: true
  });

  const [passInputs, setPassInputs] = useState<Record<string, string[]>>({
    BufferA: ['', '', '', ''],
    BufferB: ['', '', '', ''],
    BufferC: ['', '', '', ''],
    BufferD: ['', '', '', ''],
    Image: ['', '', '', '']
  });

  const [geometryType, setGeometryType] = useState<number>(0); // 0: Quad, 1: Cube, 2: Sphere
  const [logs, setLogs] = useState<LogMessage[]>([]);
  const [isCompiling, setIsCompiling] = useState(false);
  const [compilerReady, setCompilerReady] = useState(false);
  const [resetTrigger, setResetTrigger] = useState(0);
  const [showWgsl, setShowWgsl] = useState(false);
  const showWgslRef = useRef(false);

  useEffect(() => {
    showWgslRef.current = showWgsl;
  }, [showWgsl]);
  
  const engineRef = useRef<any>(null);
  const fileInputRef = useRef<HTMLInputElement>(null);
  const [isDragging, setIsDragging] = useState(false);
  const dragCounter = useRef(0);

  const importShaderFile = (file: File) => {
    const fileName = file.name;
    const extension = fileName.split('.').pop()?.toLowerCase();

    if (extension === 'glsl' || extension === 'txt' || extension === 'wgsl') {
      const reader = new FileReader();
      reader.onload = (event) => {
        const content = event.target?.result as string;
        if (content) {
          // Update active pass code
          setPassCode(prev => ({
            ...prev,
            [activePass]: content
          }));
          
          let targetLang: 'glsl' | 'wgsl' = passLanguage[activePass];
          if (extension === 'wgsl') {
            targetLang = 'wgsl';
            setPassLanguage(prev => ({ ...prev, [activePass]: 'wgsl' }));
            addLog(`Imported WGSL shader to ${activePass} from ${fileName}`, "info");
          } else if (extension === 'glsl') {
            targetLang = 'glsl';
            setPassLanguage(prev => ({ ...prev, [activePass]: 'glsl' }));
            addLog(`Imported GLSL shader to ${activePass} from ${fileName}`, "info");
          } else if (extension === 'txt') {
            targetLang = detectLanguage(content);
            setPassLanguage(prev => ({ ...prev, [activePass]: targetLang }));
            addLog(`Imported .txt file. Auto-detected language as ${targetLang.toUpperCase()}`, "info");
          }

          // Compile immediately
          handleCompile(content, targetLang, activePass);
        }
      };
      reader.readAsText(file);
    } else {
      addLog(`Unsupported file type: .${extension}. Please select a .glsl, .wgsl, or .txt file.`, "error");
    }
  };

  const handleLoadClick = () => {
    fileInputRef.current?.click();
  };

  const handleFileChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    const files = e.target.files;
    if (files && files.length > 0) {
      importShaderFile(files[0]);
      e.target.value = '';
    }
  };

  const handleSaveClick = () => {
    try {
      const code = passCode[activePass];
      const language = passLanguage[activePass];
      const blob = new Blob([code], { type: 'text/plain;charset=utf-8' });
      const url = URL.createObjectURL(blob);
      
      const a = document.createElement('a');
      a.href = url;
      a.download = `${activePass}.${language}`;
      
      document.body.appendChild(a);
      a.click();
      
      document.body.removeChild(a);
      URL.revokeObjectURL(url);
      
      addLog(`Saved ${activePass} shader as ${activePass}.${language}`, "info");
    } catch (err: any) {
      console.error("Failed to save file:", err);
      addLog(`Failed to save shader file: ${err.message || err}`, "error");
    }
  };

  const handleDragEnter = (e: React.DragEvent) => {
    e.preventDefault();
    e.stopPropagation();
    dragCounter.current++;
    if (e.dataTransfer.items && e.dataTransfer.items.length > 0) {
      setIsDragging(true);
    }
  };

  const handleDragLeave = (e: React.DragEvent) => {
    e.preventDefault();
    e.stopPropagation();
    dragCounter.current--;
    if (dragCounter.current === 0) {
      setIsDragging(false);
    }
  };

  const handleDragOver = (e: React.DragEvent) => {
    e.preventDefault();
    e.stopPropagation();
  };

  const handleDrop = (e: React.DragEvent) => {
    e.preventDefault();
    e.stopPropagation();
    setIsDragging(false);
    dragCounter.current = 0;

    const files = e.dataTransfer.files;
    if (files && files.length > 0) {
      importShaderFile(files[0]);
    }
  };

  const addLog = (text: string, type: 'info' | 'error' | 'warning') => {
    if (!text.trim()) return;
    setLogs(prev => [
      ...prev,
      {
        id: Math.random().toString(36).substring(2, 9),
        text,
        type,
        timestamp: new Date()
      }
    ]);
  };

  useEffect(() => {
    initCompiler('/glslang.wasm', '/twgsl.wasm')
      .then(() => {
        setCompilerReady(true);
        addLog("GLSL-to-WGSL Compiler initialized.", "info");
      })
      .catch(err => {
        console.error("Compiler initialization failed:", err);
        addLog(`Compiler initialization failed: ${err.message || err}`, "error");
      });

    const isDiscardingWgslRef = { current: false };

    (window as any).onWasmLog = (text: string, type: 'info' | 'warning' | 'error') => {
      if (text.startsWith("=== WGSL SOURCE FOR")) {
        isDiscardingWgslRef.current = true;
        if (showWgslRef.current) {
          addLog(text, type);
        }
        return;
      }
      
      if (text.startsWith("=====================")) {
        isDiscardingWgslRef.current = false;
        if (showWgslRef.current) {
          addLog(text, type);
        }
        return;
      }
      
      if (isDiscardingWgslRef.current && !showWgslRef.current) {
        return;
      }

      addLog(text, type);
    };

    return () => {
      (window as any).onWasmLog = undefined;
    };
  }, []);

  const handleCodeChange = (newCode: string) => {
    setPassCode(prev => ({
      ...prev,
      [activePass]: newCode
    }));
  };

  const handleLanguageChange = (lang: 'glsl' | 'wgsl') => {
    setPassLanguage(prev => ({
      ...prev,
      [activePass]: lang
    }));
    
    setPassCode(prev => {
      const currentCode = prev[activePass];
      const isDefaultWgsl = currentCode === DEFAULT_WGSL || currentCode.includes('BUFFER_WGSL_TEMPLATE') || currentCode.includes('feedback or procedural generator');
      const isDefaultGlsl = currentCode === DEFAULT_GLSL || currentCode.includes('void mainImage');
      
      if (isDefaultWgsl || isDefaultGlsl) {
        if (activePass === 'Image') {
          return {
            ...prev,
            Image: lang === 'glsl' ? DEFAULT_GLSL : DEFAULT_WGSL
          };
        } else {
          return {
            ...prev,
            [activePass]: lang === 'glsl' ? BUFFER_GLSL_TEMPLATE(activePass) : BUFFER_WGSL_TEMPLATE(activePass)
          };
        }
      }
      return prev;
    });
  };

  const handlePassToggle = (passName: string) => {
    setPassEnabled(prev => {
      const nextEnabled = !prev[passName];
      
      if (engineRef.current) {
        engineRef.current.set_pass_enabled(passName, nextEnabled);
      }

      setResetTrigger(prev => prev + 1);

      if (nextEnabled) {
        // Compile immediately on enable
        setTimeout(() => {
          handleCompile(passCode[passName], passLanguage[passName], passName);
        }, 50);
      }
      
      return {
        ...prev,
        [passName]: nextEnabled
      };
    });
  };

  const handleInputChange = (passName: string, channel: number, value: string) => {
    setPassInputs(prev => {
      const nextInputs = [...(prev[passName] || ['', '', '', ''])];
      nextInputs[channel] = value;
      
      if (engineRef.current) {
        engineRef.current.set_pass_input(passName, channel, value);
      }
      
      return {
        ...prev,
        [passName]: nextInputs
      };
    });
  };

  const findEntryPoint = (wgslCode: string, stage: 'fragment' | 'vertex'): string => {
    const cleanCode = wgslCode
      .replace(/\/\*[\s\S]*?\*\//g, '')
      .replace(/\/\/.*/g, '');

    const regex = new RegExp(`@${stage}\\s+fn\\s+(\\w+)`, 'i');
    const match = cleanCode.match(regex);
    if (match && match[1]) {
      return match[1];
    }
    return "main";
  };

  const sanitizeGlsl = (source: string): string => {
    return source
      .replace(/\bref\b/g, 'ref_')
      .replace(/\bptr\b/g, 'ptr_')
      .replace(/\blet\b/g, 'let_')
      .replace(/\bvar\b/g, 'var_')
      .replace(/\bfn\b/g, 'fn_');
  };

  const handleCompile = async (overrideCode?: string, overrideLanguage?: 'glsl' | 'wgsl', targetPassName?: string) => {
    if (!engineRef.current) {
      addLog("Cannot compile: WebGPU rendering engine is not initialized yet.", "error");
      return;
    }

    const targetPass = (typeof targetPassName === 'string') ? targetPassName : activePass;
    const activeLanguage = (typeof overrideLanguage === 'string') ? overrideLanguage : passLanguage[targetPass];
    const activeCode = (typeof overrideCode === 'string') ? overrideCode : passCode[targetPass];

    setIsCompiling(true);
    addLog(`Compiling ${activeLanguage.toUpperCase()} shader for ${targetPass}...`, "info");

    try {
      if (typeof targetPass !== 'string') {
        throw new Error(`targetPass is not a string! Got: ${typeof targetPass}`);
      }
      if (typeof activeCode !== 'string') {
        throw new Error(`activeCode is not a string! Got: ${typeof activeCode}`);
      }
      if (typeof activeLanguage !== 'string') {
        throw new Error(`activeLanguage is not a string! Got: ${typeof activeLanguage}`);
      }

      let wgslCode = activeCode;

      if (activeLanguage === 'glsl') {
        if (!compilerReady) {
          throw new Error("GLSL compiler is still loading. Please wait.");
        }
        const sanitizedCode = sanitizeGlsl(activeCode);
        const wrappedGlsl = engineRef.current.get_wrapped_glsl(sanitizedCode);
        wgslCode = compileGLSLToWGSL(wrappedGlsl, 'fragment');
      }

      const entryPoint = findEntryPoint(wgslCode, 'fragment');

      engineRef.current.update_shader(
        targetPass,
        wgslCode,
        "wgsl",
        "fragment",
        entryPoint,
        true, // isShadertoy
        (success: boolean, errorMsg: string) => {
          setIsCompiling(false);
          if (success) {
            addLog(`Shader for ${targetPass} compiled and updated successfully.`, "info");
            setResetTrigger(prev => prev + 1);
          } else {
            addLog(`Shader compilation for ${targetPass} failed: ${errorMsg}`, "error");
          }
        }
      );
    } catch (err: any) {
      console.error("Compilation error:", err);
      addLog(err.message || String(err), "error");
      setIsCompiling(false);
    }
  };

  const handleEngineReady = (engine: any) => {
    engineRef.current = engine;
    addLog("WebGPU rendering engine ready.", "info");

    // 1. Generate and load static textures
    const noiseData = generateNoiseTexture();
    engine.load_texture_data("Noise", noiseData, 256, 256, "linear");
    
    const checkerboardData = generateCheckerboardTexture();
    engine.load_texture_data("Checkerboard", checkerboardData, 256, 256, "linear");
    
    addLog("Static textures (Noise, Checkerboard) uploaded to GPU.", "info");

    // 2. Compile enabled passes (initially only Image is enabled)
    setTimeout(() => {
      handleCompile();
    }, 100);
  };

  return (
    <div 
      onDragEnter={handleDragEnter}
      onDragLeave={handleDragLeave}
      onDragOver={handleDragOver}
      onDrop={handleDrop}
      style={{
        display: 'flex',
        flexDirection: 'column',
        height: '100vh',
        background: 'var(--bg-main)',
        color: 'var(--text-primary)',
        position: 'relative'
      }}
    >
      {/* Premium Header */}
      <header style={{
        height: 'var(--header-height)',
        padding: '0 24px',
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        borderBottom: '1px solid var(--border)',
        background: 'rgba(10, 10, 18, 0.8)',
        backdropFilter: 'blur(12px)',
        zIndex: 10
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          <div style={{
            width: '32px',
            height: '32px',
            borderRadius: '8px',
            background: 'var(--accent-gradient)',
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            boxShadow: '0 0 15px var(--accent-glow)'
          }}>
            <svg style={{ width: '18px', height: '18px', color: '#fff' }} fill="none" stroke="currentColor" viewBox="0 0 24 24">
              <path strokeLinecap="round" strokeLinejoin="round" strokeWidth="2.5" d="M13 10V3L4 14h7v7l9-11h-7z" />
            </svg>
          </div>
          <div>
            <h1 style={{ fontSize: '1.2rem', fontWeight: 800, letterSpacing: '-0.5px' }}>
              Shadertoy <span style={{ background: 'var(--accent-gradient)', WebkitBackgroundClip: 'text', WebkitTextFillColor: 'transparent' }}>WebGPU</span> Studio
            </h1>
            <p style={{ margin: 0, fontSize: '0.7rem', color: 'var(--text-muted)' }}>Cross-Platform Shader & Geometry Playground</p>
          </div>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '20px' }}>
          <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
            <button
              onClick={() => setResetTrigger(prev => prev + 1)}
              title="Reset Time and Frame Count (Rewind)"
              style={{
                background: 'rgba(255,255,255,0.03)',
                border: '1px solid var(--border)',
                color: 'var(--text-secondary)',
                padding: '8px 12px',
                borderRadius: '8px',
                cursor: 'pointer',
                fontSize: '0.8rem',
                fontWeight: 600,
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
                transition: 'all 0.2s',
              }}
              onMouseEnter={(e) => {
                e.currentTarget.style.background = 'rgba(255,255,255,0.08)';
                e.currentTarget.style.color = '#fff';
                e.currentTarget.style.borderColor = 'rgba(255,255,255,0.2)';
              }}
              onMouseLeave={(e) => {
                e.currentTarget.style.background = 'rgba(255,255,255,0.03)';
                e.currentTarget.style.color = 'var(--text-secondary)';
                e.currentTarget.style.borderColor = 'var(--border)';
              }}
            >
              <svg style={{ width: '14px', height: '14px' }} fill="none" stroke="currentColor" viewBox="0 0 24 24">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth="2.5" d="M4 4v5h.582m15.356 2A8.001 8.001 0 1121.21 8H16.5" />
              </svg>
              Reset Time
            </button>

            <button
              onClick={handleLoadClick}
              title="Load Shader from File (.glsl, .wgsl, .txt)"
              style={{
                background: 'rgba(255,255,255,0.03)',
                border: '1px solid var(--border)',
                color: 'var(--text-secondary)',
                padding: '8px 12px',
                borderRadius: '8px',
                cursor: 'pointer',
                fontSize: '0.8rem',
                fontWeight: 600,
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
                transition: 'all 0.2s',
              }}
              onMouseEnter={(e) => {
                e.currentTarget.style.background = 'rgba(255,255,255,0.08)';
                e.currentTarget.style.color = '#fff';
                e.currentTarget.style.borderColor = 'rgba(255,255,255,0.2)';
              }}
              onMouseLeave={(e) => {
                e.currentTarget.style.background = 'rgba(255,255,255,0.03)';
                e.currentTarget.style.color = 'var(--text-secondary)';
                e.currentTarget.style.borderColor = 'var(--border)';
              }}
            >
              <svg style={{ width: '14px', height: '14px' }} fill="none" stroke="currentColor" viewBox="0 0 24 24">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth="2" d="M4 16v1a3 3 0 003 3h10a3 3 0 003-3v-1m-4-8l-4-4m0 0L8 8m4-4v12" />
              </svg>
              Load File
            </button>

            <input
              type="file"
              ref={fileInputRef}
              onChange={handleFileChange}
              accept=".glsl,.wgsl,.txt"
              style={{ display: 'none' }}
            />

            <button
              onClick={handleSaveClick}
              title="Save Current Shader to File"
              style={{
                background: 'rgba(255,255,255,0.03)',
                border: '1px solid var(--border)',
                color: 'var(--text-secondary)',
                padding: '8px 12px',
                borderRadius: '8px',
                cursor: 'pointer',
                fontSize: '0.8rem',
                fontWeight: 600,
                display: 'flex',
                alignItems: 'center',
                gap: '6px',
                transition: 'all 0.2s',
              }}
              onMouseEnter={(e) => {
                e.currentTarget.style.background = 'rgba(255,255,255,0.08)';
                e.currentTarget.style.color = '#fff';
                e.currentTarget.style.borderColor = 'rgba(255,255,255,0.2)';
              }}
              onMouseLeave={(e) => {
                e.currentTarget.style.background = 'rgba(255,255,255,0.03)';
                e.currentTarget.style.color = 'var(--text-secondary)';
                e.currentTarget.style.borderColor = 'var(--border)';
              }}
            >
              <svg style={{ width: '14px', height: '14px' }} fill="none" stroke="currentColor" viewBox="0 0 24 24">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth="2" d="M4 16v1a3 3 0 003 3h10a3 3 0 003-3v-1m-4-4l-4 4m0 0l-4-4m4 4V4" />
              </svg>
              Save File
            </button>
          </div>

          <div style={{ height: '20px', width: '1px', background: 'var(--border)' }} />

          <div style={{ display: 'flex', alignItems: 'center', gap: '8px', fontSize: '0.8rem' }}>
            <span style={{
              width: '8px',
              height: '8px',
              borderRadius: '50%',
              background: engineRef.current ? 'var(--color-success)' : 'var(--color-warning)',
              display: 'inline-block',
              boxShadow: engineRef.current ? '0 0 8px var(--color-success)' : '0 0 8px var(--color-warning)'
            }} />
            <span style={{ color: 'var(--text-secondary)' }}>
              {engineRef.current ? 'Engine Active' : 'Loading Engine...'}
            </span>
          </div>
        </div>
      </header>

      {/* Main Workspace Layout */}
      <main style={{
        flex: 1,
        display: 'grid',
        gridTemplateColumns: '55% 45%',
        gap: '20px',
        padding: '20px',
        overflow: 'hidden',
        height: 'calc(100vh - var(--header-height))'
      }}>
        {/* Left Column: Editor & Diagnostics */}
        <div style={{
          display: 'flex',
          flexDirection: 'column',
          gap: '20px',
          height: '100%',
          overflow: 'hidden'
        }}>
          <div style={{ flex: 1.8, overflow: 'hidden' }}>
            <Editor
              value={passCode[activePass] || ''}
              onChange={handleCodeChange}
              onCompile={handleCompile}
              language={passLanguage[activePass] || 'wgsl'}
              onLanguageChange={handleLanguageChange}
              isCompiling={isCompiling}
              passes={PASS_NAMES}
              activePass={activePass}
              onActivePassChange={setActivePass}
              passEnabled={passEnabled}
              onPassToggle={handlePassToggle}
            />
          </div>
          <div style={{ flex: 0.9, overflow: 'hidden', minHeight: '180px' }}>
            <Diagnostics
              logs={logs}
              onClear={() => setLogs([])}
              showWgsl={showWgsl}
              onToggleShowWgsl={setShowWgsl}
            />
          </div>
        </div>

        {/* Right Column: Viewport & Settings */}
        <div style={{
          display: 'flex',
          flexDirection: 'column',
          gap: '20px',
          height: '100%',
          overflow: 'hidden'
        }}>
          <div className="glass-panel" style={{
            flex: 1.2,
            borderRadius: '16px',
            overflow: 'hidden',
            boxShadow: '0 8px 32px 0 rgba(0, 0, 0, 0.2)',
            display: 'flex',
            flexDirection: 'column'
          }}>
            <div style={{ flex: 1, position: 'relative' }}>
              <Viewport
                onEngineReady={handleEngineReady}
                geometryType={geometryType}
                resetTrigger={resetTrigger}
              />
            </div>
          </div>

          {/* Controls Panel */}
          <div style={{
            flex: 0.8,
            display: 'flex',
            flexDirection: 'column',
            gap: '20px',
            overflowY: 'auto'
          }}>
            {/* Geometry Selector Card */}
            <div className="glass-panel animate-slide-in" style={{
              padding: '20px',
              borderRadius: '16px',
              display: 'flex',
              flexDirection: 'column',
              gap: '12px',
              boxShadow: '0 8px 32px 0 rgba(0, 0, 0, 0.1)'
            }}>
              <h3 style={{ fontSize: '0.85rem', fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase', color: 'var(--text-secondary)' }}>
                Geometry Controls
              </h3>
              
              <div style={{
                display: 'grid',
                gridTemplateColumns: '1fr 1fr 1fr',
                gap: '10px'
              }}>
                <button
                  onClick={() => setGeometryType(0)}
                  className={`glow-on-hover`}
                  style={{
                    background: geometryType === 0 ? 'var(--accent-gradient)' : 'rgba(255,255,255,0.03)',
                    border: '1px solid ' + (geometryType === 0 ? 'var(--accent-color)' : 'var(--border)'),
                    color: '#fff',
                    padding: '12px',
                    borderRadius: '10px',
                    cursor: 'pointer',
                    fontWeight: 600,
                    fontSize: '0.85rem',
                    transition: 'all 0.2s',
                    boxShadow: geometryType === 0 ? '0 0 15px var(--accent-glow)' : 'none'
                  }}
                >
                  2D Quad
                </button>
                <button
                  onClick={() => setGeometryType(1)}
                  className={`glow-on-hover`}
                  style={{
                    background: geometryType === 1 ? 'var(--accent-gradient)' : 'rgba(255,255,255,0.03)',
                    border: '1px solid ' + (geometryType === 1 ? 'var(--accent-color)' : 'var(--border)'),
                    color: '#fff',
                    padding: '12px',
                    borderRadius: '10px',
                    cursor: 'pointer',
                    fontWeight: 600,
                    fontSize: '0.85rem',
                    transition: 'all 0.2s',
                    boxShadow: geometryType === 1 ? '0 0 15px var(--accent-glow)' : 'none'
                  }}
                >
                  3D Cube
                </button>
                <button
                  onClick={() => setGeometryType(2)}
                  className={`glow-on-hover`}
                  style={{
                    background: geometryType === 2 ? 'var(--accent-gradient)' : 'rgba(255,255,255,0.03)',
                    border: '1px solid ' + (geometryType === 2 ? 'var(--accent-color)' : 'var(--border)'),
                    color: '#fff',
                    padding: '12px',
                    borderRadius: '10px',
                    cursor: 'pointer',
                    fontWeight: 600,
                    fontSize: '0.85rem',
                    transition: 'all 0.2s',
                    boxShadow: geometryType === 2 ? '0 0 15px var(--accent-glow)' : 'none'
                  }}
                >
                  3D Sphere
                </button>
              </div>
            </div>

            {/* Pass Inputs Card */}
            <div className="glass-panel animate-slide-in" style={{
              padding: '20px',
              borderRadius: '16px',
              display: 'flex',
              flexDirection: 'column',
              gap: '12px',
              boxShadow: '0 8px 32px 0 rgba(0, 0, 0, 0.1)'
            }}>
              <h3 style={{ fontSize: '0.85rem', fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase', color: 'var(--text-secondary)' }}>
                Pass Inputs ({activePass})
              </h3>
              
              <div style={{
                display: 'grid',
                gridTemplateColumns: '1fr 1fr',
                gap: '12px'
              }}>
                {[0, 1, 2, 3].map((channel) => (
                  <div key={channel} style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                    <span style={{ fontSize: '0.75rem', color: 'var(--text-muted)' }}>iChannel{channel}</span>
                    <select
                      value={passInputs[activePass]?.[channel] || ''}
                      onChange={(e) => handleInputChange(activePass, channel, e.target.value)}
                      style={{
                        background: 'rgba(0, 0, 0, 0.3)',
                        border: '1px solid var(--border)',
                        color: '#fff',
                        padding: '8px',
                        borderRadius: '8px',
                        fontSize: '0.8rem',
                        outline: 'none',
                        cursor: 'pointer'
                      }}
                    >
                      <option value="">None</option>
                      <option value="BufferA">Buffer A</option>
                      <option value="BufferB">Buffer B</option>
                      <option value="BufferC">Buffer C</option>
                      <option value="BufferD">Buffer D</option>
                      <option value="Noise">Noise (Static)</option>
                      <option value="Checkerboard">Checkerboard (Static)</option>
                    </select>
                  </div>
                ))}
              </div>
            </div>
          </div>
        </div>
      </main>

      {isDragging && (
        <div style={{
          position: 'absolute',
          top: 0,
          left: 0,
          right: 0,
          bottom: 0,
          background: 'rgba(10, 10, 18, 0.85)',
          backdropFilter: 'blur(8px)',
          border: '2px dashed var(--accent-color)',
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          justifyContent: 'center',
          zIndex: 9999,
          pointerEvents: 'none'
        }}>
          <div style={{
            padding: '40px',
            borderRadius: '24px',
            background: 'rgba(255, 255, 255, 0.03)',
            border: '1px solid rgba(255, 255, 255, 0.05)',
            textAlign: 'center',
            boxShadow: '0 20px 50px rgba(0,0,0,0.5)',
            display: 'flex',
            flexDirection: 'column',
            alignItems: 'center',
            gap: '20px'
          }}>
            <div style={{
              width: '80px',
              height: '80px',
              borderRadius: '20px',
              background: 'var(--accent-gradient)',
              display: 'flex',
              alignItems: 'center',
              justifyContent: 'center',
              boxShadow: '0 0 30px var(--accent-glow)'
            }}>
              <svg style={{ width: '40px', height: '40px', color: '#fff' }} fill="none" stroke="currentColor" viewBox="0 0 24 24">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth="2" d="M7 16a4 4 0 01-.88-7.903A5 5 0 1115.9 6L16 6a5 5 0 011 9.9M15 13l-3-3m0 0l-3 3m3-3v12" />
              </svg>
            </div>
            <div>
              <h3 style={{ fontSize: '1.5rem', fontWeight: 800, margin: '0 0 8px 0' }}>Import Shader File</h3>
              <p style={{ color: 'var(--text-muted)', margin: 0, fontSize: '0.9rem' }}>
                Drop a <span style={{ color: 'var(--accent-color)' }}>.glsl</span>, <span style={{ color: 'var(--accent-color)' }}>.wgsl</span>, or <span style={{ color: 'var(--accent-color)' }}>.txt</span> file to load it in the editor.
              </p>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
