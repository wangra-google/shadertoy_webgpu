// Shared state for capturing console output from libraries like glslang
export const lastGlslangErrors: string[] = [];
export const captureState = {
  isCollecting: false
};

const originalLog = console.log;
const originalWarn = console.warn;
const originalError = console.error;

console.log = (...args: any[]) => {
  if (captureState.isCollecting) {
    lastGlslangErrors.push(args.join(' '));
  }
  originalLog.apply(console, args);
};

console.warn = (...args: any[]) => {
  if (captureState.isCollecting) {
    lastGlslangErrors.push(args.join(' '));
  }
  originalWarn.apply(console, args);
};

console.error = (...args: any[]) => {
  if (captureState.isCollecting) {
    lastGlslangErrors.push(args.join(' '));
  }
  originalError.apply(console, args);
};

// Restore the original console functions to prevent global side effects
export function restoreConsole() {
  console.log = originalLog;
  console.warn = originalWarn;
  console.error = originalError;
}
