import React, { useEffect, useRef } from 'react';

export interface LogMessage {
  id: string;
  text: string;
  type: 'info' | 'error' | 'warning';
  timestamp: Date;
}

interface DiagnosticsProps {
  logs: LogMessage[];
  onClear: () => void;
}

export const Diagnostics: React.FC<DiagnosticsProps> = ({ logs, onClear }) => {
  const containerRef = useRef<HTMLDivElement>(null);

  // Auto-scroll to bottom on new logs
  useEffect(() => {
    if (containerRef.current) {
      containerRef.current.scrollTop = containerRef.current.scrollHeight;
    }
  }, [logs]);

  const getLogColor = (type: 'info' | 'error' | 'warning') => {
    switch (type) {
      case 'error':
        return 'var(--color-error)';
      case 'warning':
        return 'var(--color-warning)';
      case 'info':
      default:
        return 'var(--color-info)';
    }
  };

  const getLogBg = (type: 'info' | 'error' | 'warning') => {
    switch (type) {
      case 'error':
        return 'rgba(244, 63, 94, 0.05)';
      case 'warning':
        return 'rgba(245, 158, 11, 0.05)';
      case 'info':
      default:
        return 'rgba(14, 165, 233, 0.03)';
    }
  };

  const formatTime = (date: Date) => {
    return date.toTimeString().split(' ')[0];
  };

  return (
    <div 
      className="glass-panel animate-slide-in" 
      style={{
        display: 'flex',
        flexDirection: 'column',
        height: '100%',
        borderRadius: '16px',
        overflow: 'hidden',
        boxShadow: '0 8px 32px 0 rgba(0, 0, 0, 0.2)'
      }}
    >
      {/* Header */}
      <div style={{
        display: 'flex',
        alignItems: 'center',
        justifyContent: 'space-between',
        padding: '12px 16px',
        background: 'rgba(9, 9, 16, 0.8)',
        borderBottom: '1px solid var(--border)',
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: '8px' }}>
          <span style={{ fontSize: '0.85rem', fontWeight: 700, letterSpacing: '0.5px', textTransform: 'uppercase', color: 'var(--text-secondary)' }}>
            Console & Diagnostics
          </span>
          {logs.length > 0 && (
            <span style={{
              fontSize: '0.75rem',
              background: 'rgba(255,255,255,0.08)',
              padding: '2px 6px',
              borderRadius: '10px',
              color: 'var(--text-primary)',
              fontWeight: 600
            }}>
              {logs.length}
            </span>
          )}
        </div>
        <button
          onClick={onClear}
          style={{
            background: 'transparent',
            border: 'none',
            color: 'var(--text-muted)',
            fontSize: '0.8rem',
            cursor: 'pointer',
            padding: '4px 8px',
            borderRadius: '4px',
            transition: 'all 0.2s'
          }}
          onMouseEnter={(e) => (e.currentTarget.style.color = '#fff')}
          onMouseLeave={(e) => (e.currentTarget.style.color = 'var(--text-muted)')}
        >
          Clear Console
        </button>
      </div>

      {/* Logs View */}
      <div 
        ref={containerRef}
        style={{
          flex: 1,
          overflowY: 'auto',
          padding: '8px 16px',
          background: '#07070d',
          fontFamily: 'var(--font-mono)',
          fontSize: '0.85rem',
          lineHeight: '1.5',
          display: 'flex',
          flexDirection: 'column',
          gap: '2px'
        }}
      >
        {logs.length === 0 ? (
          <div style={{
            display: 'flex',
            alignItems: 'center',
            justifyContent: 'center',
            height: '100%',
            color: 'var(--text-muted)',
            fontSize: '0.8rem',
            fontStyle: 'italic'
          }}>
            No compilation logs. Press Compile or Alt+Enter to build shaders.
          </div>
        ) : (
          logs.map((log) => (
            <div 
              key={log.id}
              style={{
                display: 'flex',
                gap: '12px',
                padding: '6px 8px',
                borderRadius: '6px',
                background: getLogBg(log.type),
                borderLeft: `3px solid ${getLogColor(log.type)}`
              }}
            >
              <span style={{ color: 'var(--text-muted)', minWidth: '60px', userSelect: 'none' }}>
                [{formatTime(log.timestamp)}]
              </span>
              <span style={{ 
                color: log.type === 'error' ? 'var(--color-error)' : log.type === 'warning' ? 'var(--color-warning)' : 'var(--text-primary)',
                whiteSpace: 'pre-wrap',
                wordBreak: 'break-all'
              }}>
                {log.text}
              </span>
            </div>
          ))
        )}
      </div>
    </div>
  );
};
