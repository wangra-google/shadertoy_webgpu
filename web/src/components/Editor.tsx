import React from 'react';
import CodeMirror from '@uiw/react-codemirror';
import { EditorView } from '@codemirror/view';
import { cpp } from '@codemirror/lang-cpp';
import { rust } from '@codemirror/lang-rust';

interface EditorProps {
  value: string;
  onChange: (value: string) => void;
  onCompile: () => void;
  language: 'glsl' | 'wgsl';
  onLanguageChange: (lang: 'glsl' | 'wgsl') => void;
  isCompiling: boolean;
  passes: string[];
  activePass: string;
  onActivePassChange: (passName: string) => void;
  passEnabled: Record<string, boolean>;
  onPassToggle: (passName: string) => void;
}

export const Editor: React.FC<EditorProps> = ({
  value,
  onChange,
  onCompile,
  language,
  onLanguageChange,
  isCompiling,
  passes,
  activePass,
  onActivePassChange,
  passEnabled,
  onPassToggle,
}) => {
  
  // Custom editor theme using CodeMirror 6 EditorView.theme
  const editorTheme = EditorView.theme({
    '&': {
      backgroundColor: '#0c0c16 !important',
      height: '100%',
      fontSize: '14px',
      fontFamily: 'var(--font-mono)',
    },
    '.cm-scroller': {
      overflow: 'auto',
    },
    '.cm-gutters': {
      backgroundColor: '#090910',
      color: '#4b5563',
      borderRight: '1px solid rgba(255, 255, 255, 0.05)',
    },
    '.cm-activeLine': {
      backgroundColor: 'rgba(255, 255, 255, 0.02)',
    },
    '.cm-activeLineGutter': {
      backgroundColor: 'rgba(255, 255, 255, 0.05)',
      color: '#fff',
    },
    '.cm-cursor': {
      borderLeftColor: 'var(--accent-color)',
      borderLeftWidth: '2px',
    },
  }, { dark: true });

  // Key map for compilation (Alt+Enter or Ctrl+Enter)
  const handleKeyDown = (e: React.KeyboardEvent) => {
    if ((e.altKey || e.ctrlKey) && e.key === 'Enter') {
      e.preventDefault();
      onCompile();
    }
  };

  return (
    <div 
      className="glass-panel" 
      style={{
        display: 'flex',
        flexDirection: 'column',
        height: '100%',
        borderRadius: '16px',
        overflow: 'hidden',
        boxShadow: '0 8px 32px 0 rgba(0, 0, 0, 0.2)'
      }}
      onKeyDown={handleKeyDown}
    >
      {/* Editor Header Panel */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        padding: '12px 16px',
        background: 'rgba(9, 9, 16, 0.8)',
        borderBottom: '1px solid var(--border)',
        flexWrap: 'wrap',
        gap: '12px'
      }}>
        {/* Pass Tabs */}
        <div style={{ display: 'flex', gap: '6px', flexWrap: 'wrap' }}>
          {passes.map((pass) => {
            const isImage = pass === 'Image';
            const isActive = pass === activePass;
            const isEnabled = passEnabled[pass] || isImage;
            
            return (
              <div 
                key={pass}
                style={{
                  display: 'flex',
                  alignItems: 'center',
                  background: isActive ? 'rgba(99, 102, 241, 0.15)' : 'rgba(255, 255, 255, 0.02)',
                  border: '1px solid ' + (isActive ? 'rgba(99, 102, 241, 0.3)' : 'var(--border)'),
                  borderRadius: '8px',
                  padding: '2px 8px',
                  gap: '4px',
                  transition: 'all 0.2s'
                }}
              >
                {!isImage && (
                  <input 
                    type="checkbox"
                    checked={isEnabled}
                    onChange={() => onPassToggle(pass)}
                    title={`Enable/Disable ${pass}`}
                    style={{ cursor: 'pointer', width: '14px', height: '14px' }}
                  />
                )}
                <button
                  onClick={() => onActivePassChange(pass)}
                  style={{
                    background: 'transparent',
                    border: 'none',
                    color: isEnabled ? (isActive ? '#fff' : 'var(--text-secondary)') : 'var(--text-muted)',
                    fontSize: '0.8rem',
                    fontWeight: isActive ? 700 : 500,
                    cursor: 'pointer',
                    padding: '4px 4px',
                    outline: 'none'
                  }}
                >
                  {pass}
                </button>
              </div>
            );
          })}
        </div>

        {/* Controls */}
        <div style={{ display: 'flex', alignItems: 'center', gap: '12px' }}>
          {/* Language Selector */}
          <div style={{
            display: 'flex',
            background: 'rgba(0, 0, 0, 0.3)',
            padding: '2px',
            borderRadius: '8px',
            border: '1px solid var(--border)'
          }}>
            <button
              onClick={() => onLanguageChange('glsl')}
              style={{
                background: language === 'glsl' ? 'rgba(255, 255, 255, 0.08)' : 'transparent',
                border: 'none',
                color: language === 'glsl' ? '#fff' : 'var(--text-secondary)',
                padding: '4px 10px',
                borderRadius: '6px',
                fontSize: '0.75rem',
                fontWeight: 600,
                cursor: 'pointer',
                transition: 'all 0.2s'
              }}
            >
              GLSL
            </button>
            <button
              onClick={() => onLanguageChange('wgsl')}
              style={{
                background: language === 'wgsl' ? 'rgba(255, 255, 255, 0.08)' : 'transparent',
                border: 'none',
                color: language === 'wgsl' ? '#fff' : 'var(--text-secondary)',
                padding: '4px 10px',
                borderRadius: '6px',
                fontSize: '0.75rem',
                fontWeight: 600,
                cursor: 'pointer',
                transition: 'all 0.2s'
              }}
            >
              WGSL
            </button>
          </div>

          {/* Compile Button */}
          <button
            onClick={() => onCompile()}
            disabled={isCompiling}
            className="glow-on-hover"
            style={{
              background: 'var(--accent-gradient)',
              border: 'none',
              color: '#fff',
              padding: '6px 16px',
              borderRadius: '8px',
              fontSize: '0.85rem',
              fontWeight: 600,
              cursor: 'pointer',
              display: 'flex',
              alignItems: 'center',
              gap: '8px',
              opacity: isCompiling ? 0.6 : 1
            }}
          >
            {isCompiling ? (
              <span className="spinner" style={{
                width: '12px',
                height: '12px',
                border: '2px solid rgba(255,255,255,0.3)',
                borderTopColor: '#fff',
                borderRadius: '50%',
                animation: 'spin 1s linear infinite'
              }} />
            ) : (
              <svg style={{ width: '12px', height: '12px' }} fill="currentColor" viewBox="0 0 24 24">
                <path d="M8 5v14l11-7z" />
              </svg>
            )}
            <span>Compile</span>
            <span style={{
              fontSize: '0.7rem',
              background: 'rgba(0,0,0,0.2)',
              padding: '1px 4px',
              borderRadius: '3px',
              color: 'rgba(255,255,255,0.7)',
              marginLeft: '4px',
              fontWeight: 400
            }}>Alt+Enter</span>
          </button>
        </div>
      </div>

      {/* Editor Body */}
      <div style={{ flex: 1, overflow: 'hidden', position: 'relative' }}>
        <CodeMirror
          value={value}
          height="100%"
          theme="dark"
          extensions={[editorTheme, language === 'glsl' ? cpp() : rust()]}
          onChange={(val) => onChange(val)}
          style={{ height: '100%' }}
        />
      </div>

      {/* CSS Spin Animation */}
      <style>{`
        @keyframes spin {
          to { transform: rotate(360deg); }
        }
      `}</style>
    </div>
  );
};

