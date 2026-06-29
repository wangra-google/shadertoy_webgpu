import React, { useEffect, useRef, useState } from 'react';

interface ViewportProps {
  onEngineReady: (engine: any) => void;
  geometryType: number; // 0: Quad, 1: Cube, 2: Sphere
}

export const Viewport: React.FC<ViewportProps> = ({ onEngineReady, geometryType }) => {
  const canvasRef = useRef<HTMLCanvasElement>(null);
  const [fps, setFps] = useState<string>('0.0');
  const [elapsedTime, setElapsedTime] = useState<string>('0.0s');
  const [resolution, setResolution] = useState<string>('0x0');
  const [isInitialized, setIsInitialized] = useState(false);
  const [errorMsg, setErrorMsg] = useState<string | null>(null);
  const engineRef = useRef<any>(null);
  const animationFrameRef = useRef<number | null>(null);

  // Mouse state
  const mouseRef = useRef({ x: 0, y: 0, z: 0, w: 0 });
  const isMouseDownRef = useRef(false);

  const initializingRef = useRef(false);
  const initializedRef = useRef(false);

  useEffect(() => {
    const initEngine = async () => {
      if (!canvasRef.current || initializingRef.current || initializedRef.current) return;
      
      initializingRef.current = true;
      setErrorMsg(null);
      
      try {
        const createEngineModule = (window as any).createEngineModule;
        if (!createEngineModule) {
          setErrorMsg("WASM module factory not loaded yet.");
          initializingRef.current = false;
          return;
        }

        // Initialize the modularized WASM engine, passing stdout/stderr log capturers
        const Module = await createEngineModule({
          print: (text: string) => {
            console.log("WASM:", text);
            if ((window as any).onWasmLog) {
              (window as any).onWasmLog(text, 'info');
            }
          },
          printErr: (text: string) => {
            console.error("WASM Error:", text);
            const type = text.toLowerCase().includes('warning') ? 'warning' : 'error';
            if ((window as any).onWasmLog) {
              (window as any).onWasmLog(text, type);
            }
          }
        });

        const canvas = canvasRef.current;
        const dpr = window.devicePixelRatio || 1;
        const width = canvas.clientWidth * dpr;
        const height = canvas.clientHeight * dpr;
        canvas.width = width;
        canvas.height = height;
        setResolution(`${width}x${height}`);

        console.log("Initializing EngineCore with selector: #viewport-canvas");
        const engine = new Module.EngineCore("#viewport-canvas");
        
        console.log("Awaiting engine.initialize...");
        // engine.initialize is async due to Emscripten Asyncify wrapping WebGPU adapter/device requests
        const success = await engine.initialize(width, height);
        
        if (!success) {
          setErrorMsg("Failed to initialize WebGPU. Your browser/hardware may not support WebGPU.");
          initializingRef.current = false;
          return;
        }

        console.log("EngineCore initialized successfully.");
        engineRef.current = engine;
        engine.set_geometry_type(geometryType);
        onEngineReady(engine);
        setIsInitialized(true);
        initializedRef.current = true;
        initializingRef.current = false;

        // Setup rendering loop
        const startTime = performance.now();
        let lastTime = startTime;
        let frameCount = 0;
        let fpsAccumulator = 0;
        let fpsFrameCount = 0;

        const tick = (timestamp: number) => {
          if (!engineRef.current) return;

          const totalTime = (timestamp - startTime) / 1000.0;
          const timeDelta = (timestamp - lastTime) / 1000.0;
          lastTime = timestamp;

          setElapsedTime(`${totalTime.toFixed(1)}s`);

          fpsAccumulator += timeDelta;
          fpsFrameCount++;
          if (fpsAccumulator >= 0.5) {
            const calculatedFps = fpsFrameCount / fpsAccumulator;
            setFps(calculatedFps.toFixed(1));
            fpsAccumulator = 0;
            fpsFrameCount = 0;
          }

          const m = mouseRef.current;
          
          engine.set_uniforms(
            totalTime,
            timeDelta,
            1.0 / Math.max(timeDelta, 0.001),
            frameCount,
            m.x, m.y, m.z, m.w
          );

          engine.tick(timestamp);
          frameCount++;
          animationFrameRef.current = requestAnimationFrame(tick);
        };

        animationFrameRef.current = requestAnimationFrame(tick);
      } catch (err: any) {
        console.error("Error initializing engine:", err);
        setErrorMsg(`Initialization error: ${err.message || err}`);
        initializingRef.current = false;
      }
    };

    // Check if Emscripten module factory is ready on window
    if ((window as any).createEngineModule) {
      initEngine();
    } else {
      const checkInterval = setInterval(() => {
        if ((window as any).createEngineModule) {
          clearInterval(checkInterval);
          initEngine();
        }
      }, 100);

      return () => clearInterval(checkInterval);
    }

    return () => {
      if (animationFrameRef.current) {
        cancelAnimationFrame(animationFrameRef.current);
      }
      if (engineRef.current) {
        // Explicitly delete the C++ object to free WASM heap memory
        engineRef.current.delete();
        engineRef.current = null;
      }
    };
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // Handle Resize
  useEffect(() => {
    const handleResize = () => {
      if (!canvasRef.current || !engineRef.current) return;
      const canvas = canvasRef.current;
      const dpr = window.devicePixelRatio || 1;
      const width = canvas.clientWidth * dpr;
      const height = canvas.clientHeight * dpr;
      canvas.width = width;
      canvas.height = height;
      setResolution(`${width}x${height}`);
      engineRef.current.resize(width, height);
    };

    window.addEventListener('resize', handleResize);
    return () => window.removeEventListener('resize', handleResize);
  }, []);

  // Handle Geometry Change
  useEffect(() => {
    if (engineRef.current && isInitialized) {
      engineRef.current.set_geometry_type(geometryType);
    }
  }, [geometryType, isInitialized]);

  // Mouse event handlers to simulate Shadertoy iMouse behavior
  const handleMouseMove = (e: React.MouseEvent<HTMLCanvasElement>) => {
    if (!canvasRef.current) return;
    const rect = canvasRef.current.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    // Invert Y to match OpenGL/Shadertoy screen coordinates (Y starts at bottom)
    const x = (e.clientX - rect.left) * dpr;
    const y = (rect.height - (e.clientY - rect.top)) * dpr;

    if (isMouseDownRef.current) {
      mouseRef.current.x = x;
      mouseRef.current.y = y;
    }
  };

  const handleMouseDown = (e: React.MouseEvent<HTMLCanvasElement>) => {
    if (!canvasRef.current) return;
    isMouseDownRef.current = true;
    const rect = canvasRef.current.getBoundingClientRect();
    const dpr = window.devicePixelRatio || 1;
    const x = (e.clientX - rect.left) * dpr;
    const y = (rect.height - (e.clientY - rect.top)) * dpr;

    mouseRef.current.x = x;
    mouseRef.current.y = y;
    mouseRef.current.z = x; // positive z means mouse is down
    mouseRef.current.w = y;
  };

  const handleMouseUp = () => {
    isMouseDownRef.current = false;
    // Shadertoy encodes "mouse up" by making z and w negative
    mouseRef.current.z = -Math.abs(mouseRef.current.z);
    mouseRef.current.w = -Math.abs(mouseRef.current.w);
  };

  return (
    <div style={{ position: 'relative', width: '100%', height: '100%', overflow: 'hidden', background: '#020205' }}>
      <canvas
        id="viewport-canvas"
        ref={canvasRef}
        onMouseMove={handleMouseMove}
        onMouseDown={handleMouseDown}
        onMouseUp={handleMouseUp}
        onMouseLeave={handleMouseUp}
        style={{ width: '100%', height: '100%', display: 'block' }}
      />

      {/* Overlay Stats - Premium Glassmorphic design */}
      <div className="glass-panel" style={{
        position: 'absolute',
        top: '16px',
        right: '16px',
        padding: '12px 16px',
        borderRadius: '12px',
        pointerEvents: 'none',
        display: 'flex',
        flexDirection: 'column',
        gap: '6px',
        fontSize: '0.8rem',
        color: 'var(--text-secondary)'
      }}>
        <div style={{ display: 'flex', justifyContent: 'space-between', gap: '24px' }}>
          <span>FPS:</span>
          <span style={{ color: 'var(--text-primary)', fontWeight: 600 }}>{fps}</span>
        </div>
        <div style={{ display: 'flex', justifyContent: 'space-between', gap: '24px' }}>
          <span>Time:</span>
          <span style={{ color: 'var(--text-primary)', fontWeight: 600 }}>{elapsedTime}</span>
        </div>
        <div style={{ display: 'flex', justifyContent: 'space-between', gap: '24px' }}>
          <span>Resolution:</span>
          <span style={{ color: 'var(--text-primary)', fontWeight: 600 }}>{resolution}</span>
        </div>
        <div style={{ display: 'flex', justifyContent: 'space-between', gap: '24px' }}>
          <span>Geometry:</span>
          <span style={{ color: 'var(--accent-color)', fontWeight: 600 }}>
            {geometryType === 0 ? '2D Quad' : geometryType === 1 ? '3D Cube' : '3D Sphere'}
          </span>
        </div>
      </div>

      {/* Error Overlay */}
      {errorMsg && (
        <div style={{
          position: 'absolute',
          inset: 0,
          background: 'rgba(5, 5, 10, 0.85)',
          backdropFilter: 'blur(8px)',
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          justifyContent: 'center',
          padding: '24px',
          textAlign: 'center',
          color: 'var(--color-error)'
        }}>
          <svg style={{ width: '48px', height: '48px', marginBottom: '16px' }} fill="none" stroke="currentColor" viewBox="0 0 24 24">
            <path strokeLinecap="round" strokeLinejoin="round" strokeWidth="2" d="M12 9v2m0 4h.01m-6.938 4h13.856c1.54 0 2.502-1.667 1.732-3L13.732 4c-.77-1.333-2.694-1.333-3.464 0L3.34 16c-.77 1.333.192 3 1.732 3z" />
          </svg>
          <h3 style={{ fontSize: '1.2rem', fontWeight: 700, marginBottom: '8px', color: '#fff' }}>WebGPU Initialization Failed</h3>
          <p style={{ maxWidth: '400px', fontSize: '0.9rem', color: 'var(--text-secondary)' }}>{errorMsg}</p>
        </div>
      )}
    </div>
  );
};
