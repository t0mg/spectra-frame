/**
 * Debounces a function, delaying its execution until after a certain time has passed
 * without new calls.
 * @param {Function} func - The function to debounce.
 * @param {number} delay - The debounce delay in milliseconds.
 * @returns {Function} The debounced function.
 */
export function debounce(func, delay) {
  let timeoutId;
  return function (...args) {
    clearTimeout(timeoutId);
    timeoutId = setTimeout(() => {
      func.apply(this, args);
    }, delay);
  };
}

export function clamp(value, min = 0, max = 255) {
  return Math.max(min, Math.min(value, max));
}