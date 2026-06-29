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
fn fs_main(input : VertexOutput) -> @location(0) vec4<f32> {
    let uv = input.uv;
    let time = u_shadertoy.iTime;
    
    // Create a beautiful plasma wave
    let col = 0.5 + 0.5 * cos(vec3<f32>(time) + uv.xyx + vec3<f32>(0.0, 2.0, 4.0));
    
    return vec4<f32>(col, 1.0);
}`;

export default function App() {
  const [code, setCode] = useState(DEFAULT_WGSL);
  const [language, setLanguage] = useState<'glsl' | 'wgsl'>('wgsl');
  const [geometryType, setGeometryType] = useState<number>(0); // 0: Quad, 1: Cube, 2: Sphere
  const [logs, setLogs] = useState<LogMessage[]>([]);
  const [isCompiling, setIsCompiling] = useState(false);
  const [compilerReady, setCompilerReady] = useState(false);
  
  const engineRef = useRef<any>(null);

  // Helper to add logs programmatically
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

  // 1. Initialize the GLSL to WGSL compiler & Emscripten logs
  useEffect(() => {
    // Diagnostic fetch to verify WASM file accessibility
    fetch('/glslang.wasm')
      .then(res => {
        console.log("Diagnostic fetch /glslang.wasm status:", res.status, res.statusText);
      })
      .catch(err => {
        console.error("Diagnostic fetch /glslang.wasm failed:", err);
      });

    // Initialize compiler
    initCompiler('/glslang.wasm', '/twgsl.wasm')
      .then(() => {
        setCompilerReady(true);
        addLog("GLSL-to-WGSL Compiler initialized.", "info");
      })
      .catch(err => {
        console.error("Compiler initialization failed:", err);
        addLog(`Compiler initialization failed: ${err.message || err}`, "error");
      });

    // Capture Emscripten stdout/stderr logs via window callback
    (window as any).onWasmLog = (text: string, type: 'info' | 'warning' | 'error') => {
      addLog(text, type);
    };

    return () => {
      (window as any).onWasmLog = undefined;
    };
  }, []);

  // Handle language changes
  const handleLanguageChange = (lang: 'glsl' | 'wgsl') => {
    setLanguage(lang);
    setCode(lang === 'glsl' ? DEFAULT_GLSL : DEFAULT_WGSL);
    addLog(`Switched language to ${lang.toUpperCase()}`, "info");
  };

  // Compile and update the shader in the engine
  const handleCompile = async () => {
    if (!engineRef.current) {
      addLog("Cannot compile: WebGPU rendering engine is not initialized yet.", "error");
      return;
    }

    setIsCompiling(true);
    addLog(`Compiling ${language.toUpperCase()} shader...`, "info");

    try {
      let wgslCode = code;

      // 1. If GLSL, compile to WGSL first
      if (language === 'glsl') {
        if (!compilerReady) {
          throw new Error("GLSL compiler is still loading. Please wait.");
        }
        
        // Wrap the GLSL code first (or let C++ wrap it? No, in Web, C++ does not support GLSL compilation,
        // so we must wrap it in JS and compile the full shader).
        // Let's get the wrapped GLSL code from the engine!
        const wrappedGlsl = engineRef.current.get_wrapped_glsl(code);
        wgslCode = compileGLSLToWGSL(wrappedGlsl, 'fragment');
      }

      // 2. Upload the WGSL shader to the engine, passing a callback
      // Signature: update_shader(passName, shaderSource, language, stage, isShadertoy, callback)
      engineRef.current.update_shader(
        "Image",
        wgslCode,
        "wgsl",
        "fragment",
        true, // isShadertoy
        (success: boolean, errorMsg: string) => {
          setIsCompiling(false);
          if (success) {
            addLog("Shader compiled and updated successfully.", "info");
          } else {
            addLog(`Shader compilation failed: ${errorMsg}`, "error");
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
    
    // Compile the initial shader immediately once the engine is ready
    setTimeout(() => {
      handleCompile();
    }, 100);
  };

  return (
    <div style={{
      display: 'flex',
      flexDirection: 'column',
      height: '100vh',
      background: 'var(--bg-main)',
      color: 'var(--text-primary)'
    }}>
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
          {/* Logo / Icon */}
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

        {/* Header Status */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '16px' }}>
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
              value={code}
              onChange={setCode}
              onCompile={handleCompile}
              language={language}
              onLanguageChange={handleLanguageChange}
              isCompiling={isCompiling}
            />
          </div>
          <div style={{ flex: 0.9, overflow: 'hidden', minHeight: '180px' }}>
            <Diagnostics
              logs={logs}
              onClear={() => setLogs([])}
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
          {/* Viewport Card */}
          <div className="glass-panel" style={{
            flex: 1,
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
              />
            </div>
          </div>

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
            
            <p style={{ margin: 0, fontSize: '0.75rem', color: 'var(--text-muted)', lineHeight: 1.4 }}>
              * Choose between a fullscreen 2D screen-space shader or custom 3D vertex displacements mapped onto standard procedural models.
            </p>
          </div>
        </div>
      </main>
    </div>
  );
}
