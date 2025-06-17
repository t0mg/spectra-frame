import { clamp } from './utils.js';

// --- Output Configuration ---
export const TARGET_OUTPUT_WIDTH = 1200;
export const TARGET_OUTPUT_HEIGHT = 1600;

// --- Color Palettes ---
export const PALETTES = {
  SPECTRA6: [  // 6-Color palette from my own measurement
    { r: 20, g: 20, b: 35 },   // Black
    { r: 175, g: 185, b: 185 },// White
    { r: 208, g: 195, b: 4 },  // Yellow
    { r: 170, g: 34, b: 34 },  // Red
    { r: 46, g: 110, b: 180 }, // Blue
    { r: 76, g: 122, b: 76 }   // Green
  ],
  MONOCHROME: [
    { r: 0, g: 0, b: 0 },
    { r: 255, g: 255, b: 255 }
  ],
  // Other palettes can be added here if needed later
};

// --- Constants for Dithering ---
const ERROR_FS_RIGHT = 7 / 16;
const ERROR_FS_BOTTOM_LEFT = 3 / 16;
const ERROR_FS_BOTTOM = 5 / 16;
const ERROR_FS_BOTTOM_RIGHT = 1 / 16;

const ERROR_ATKINSON_P1_Y0 = 1 / 8;
const ERROR_ATKINSON_P2_Y0 = 1 / 8;
const ERROR_ATKINSON_N1_Y1 = 1 / 8;
const ERROR_ATKINSON_P0_Y1 = 1 / 8;
const ERROR_ATKINSON_P1_Y1 = 1 / 8;
const ERROR_ATKINSON_P2_Y1 = 1 / 8;
const ERROR_ATKINSON_P0_Y2 = 1 / 8;

// --- Internal Canvases for Image Processing ---
let orientedSourceCanvas = null;
let orientedSourceCtx = null;
let sourceProcessingCanvas = null;
let sourceProcessingCtx = null;

function initializeOffscreenCanvases() {
    if (!orientedSourceCanvas) {
        orientedSourceCanvas = document.createElement('canvas');
        orientedSourceCtx = orientedSourceCanvas.getContext('2d', { willReadFrequently: true });
    }
    if (!sourceProcessingCanvas) {
        sourceProcessingCanvas = document.createElement('canvas');
        sourceProcessingCtx = sourceProcessingCanvas.getContext('2d', { willReadFrequently: true });
    }
}

export function getColors(paletteName) {
  return PALETTES[paletteName] || PALETTES.SPECTRA6;
}

export function findNearestColor(pixel, colors) {
  let nearestColor = colors[0];
  let minDistance = colorDistanceSquared(pixel, colors[0]);
  for (let i = 1; i < colors.length; i++) {
    const distance = colorDistanceSquared(pixel, colors[i]);
    if (distance < minDistance) {
      minDistance = distance;
      nearestColor = colors[i];
    }
  }
  return nearestColor;
}

export function colorDistanceSquared(c1, c2) {
  return Math.pow(c1.r - c2.r, 2) +
    Math.pow(c1.g - c2.g, 2) +
    Math.pow(c1.b - c2.b, 2);
}

export function getOrientedSourceCanvas(image, rotationAngle) {
  initializeOffscreenCanvases();
  let canvasWidth, canvasHeight;
  if (rotationAngle === 90 || rotationAngle === 270) {
    canvasWidth = image.height;
    canvasHeight = image.width;
  } else {
    canvasWidth = image.width;
    canvasHeight = image.height;
  }

  if (orientedSourceCanvas.width !== canvasWidth) orientedSourceCanvas.width = canvasWidth;
  if (orientedSourceCanvas.height !== canvasHeight) orientedSourceCanvas.height = canvasHeight;

  orientedSourceCtx.setTransform(1, 0, 0, 1, 0, 0);
  orientedSourceCtx.clearRect(0, 0, canvasWidth, canvasHeight);

  if (rotationAngle === 0) {
    orientedSourceCtx.drawImage(image, 0, 0, image.width, image.height);
  } else {
    orientedSourceCtx.translate(canvasWidth / 2, canvasHeight / 2);
    orientedSourceCtx.rotate(rotationAngle * Math.PI / 180);
    orientedSourceCtx.drawImage(image, -image.width / 2, -image.height / 2, image.width, image.height);
    orientedSourceCtx.setTransform(1, 0, 0, 1, 0, 0);
  }
  return orientedSourceCanvas;
}

export function getProcessedSourceCanvas(image, brightness, contrast, gamma) {
  initializeOffscreenCanvases();
  if (sourceProcessingCanvas.width !== image.width || sourceProcessingCanvas.height !== image.height) {
    sourceProcessingCanvas.width = image.width;
    sourceProcessingCanvas.height = image.height;
  }

  sourceProcessingCtx.drawImage(image, 0, 0, image.width, image.height);
  const imageData = sourceProcessingCtx.getImageData(0, 0, image.width, image.height);
  const pixels = imageData.data;

  for (let i = 0; i < pixels.length; i += 4) {
    pixels[i] = clamp(pixels[i] + brightness);
    pixels[i + 1] = clamp(pixels[i + 1] + brightness);
    pixels[i + 2] = clamp(pixels[i + 2] + brightness);
  }

  for (let i = 0; i < pixels.length; i += 4) {
    pixels[i] = clamp((((pixels[i] / 255.0) - 0.5) * contrast + 0.5) * 255.0);
    pixels[i + 1] = clamp((((pixels[i + 1] / 255.0) - 0.5) * contrast + 0.5) * 255.0);
    pixels[i + 2] = clamp((((pixels[i + 2] / 255.0) - 0.5) * contrast + 0.5) * 255.0);
  }

  if (gamma > 0) {
    const invGamma = 1.0 / gamma;
    for (let i = 0; i < pixels.length; i += 4) {
      pixels[i] = clamp(255 * Math.pow(pixels[i] / 255.0, invGamma));
      pixels[i + 1] = clamp(255 * Math.pow(pixels[i + 1] / 255.0, invGamma));
      pixels[i + 2] = clamp(255 * Math.pow(pixels[i + 2] / 255.0, invGamma));
    }
  }

  sourceProcessingCtx.putImageData(imageData, 0, 0);
  return sourceProcessingCanvas;
}

function applyDistributedError(pixels, width, x, y, quantError, weight) {
  const index = (y * width + x) * 4;
  pixels[index] = clamp(pixels[index] + quantError.r * weight);
  pixels[index + 1] = clamp(pixels[index + 1] + quantError.g * weight);
  pixels[index + 2] = clamp(pixels[index + 2] + quantError.b * weight);
}

function distributeFloydSteinbergError(pixels, width, height, x, y, quantError) {
  if (x + 1 < width) applyDistributedError(pixels, width, x + 1, y, quantError, ERROR_FS_RIGHT);
  if (x > 0 && y + 1 < height) applyDistributedError(pixels, width, x - 1, y + 1, quantError, ERROR_FS_BOTTOM_LEFT);
  if (y + 1 < height) applyDistributedError(pixels, width, x, y + 1, quantError, ERROR_FS_BOTTOM);
  if (x + 1 < width && y + 1 < height) applyDistributedError(pixels, width, x + 1, y + 1, quantError, ERROR_FS_BOTTOM_RIGHT);
}

export function applyFloydSteinbergDithering(imageData, colors) {
  const pixels = imageData.data;
  for (let y = 0; y < imageData.height; y++) {
    for (let x = 0; x < imageData.width; x++) {
      const index = (y * imageData.width + x) * 4;
      const oldPixel = { r: pixels[index], g: pixels[index + 1], b: pixels[index + 2] };
      const newPixel = findNearestColor(oldPixel, colors);
      pixels[index] = newPixel.r; pixels[index + 1] = newPixel.g; pixels[index + 2] = newPixel.b;
      const quantError = { r: oldPixel.r - newPixel.r, g: oldPixel.g - newPixel.g, b: oldPixel.b - newPixel.b };
      distributeFloydSteinbergError(pixels, imageData.width, imageData.height, x, y, quantError);
    }
  }
}

function generateRandomNoise(width, height) {
  const noise = new Array(height);
  for (let y = 0; y < height; y++) {
    noise[y] = new Array(width);
    for (let x = 0; x < width; x++) {
      noise[y][x] = Math.random() * 255;
    }
  }
  return noise;
}

export function applyBlueNoiseDithering(imageData, colors) {
  const pixels = imageData.data;
  const noise = generateRandomNoise(imageData.width, imageData.height);
  for (let y = 0; y < imageData.height; y++) {
    for (let x = 0; x < imageData.width; x++) {
      const index = (y * imageData.width + x) * 4;
      const thresholdValue = noise[y][x];
      const grayscale = pixels[index] * 0.299 + pixels[index + 1] * 0.587 + pixels[index + 2] * 0.114;
      const ditheredGrayscale = grayscale > thresholdValue ? 255 : 0;
      const tempPixel = { r: ditheredGrayscale, g: ditheredGrayscale, b: ditheredGrayscale };
      const newPixel = findNearestColor(tempPixel, colors);
      pixels[index] = newPixel.r; pixels[index + 1] = newPixel.g; pixels[index + 2] = newPixel.b;
    }
  }
}

function distributeAtkinsonError(pixels, width, height, x, y, quantError) {
  if (x + 1 < width) applyDistributedError(pixels, width, x + 1, y, quantError, ERROR_ATKINSON_P1_Y0);
  if (x + 2 < width) applyDistributedError(pixels, width, x + 2, y, quantError, ERROR_ATKINSON_P2_Y0);
  if (x > 0 && y + 1 < height) applyDistributedError(pixels, width, x - 1, y + 1, quantError, ERROR_ATKINSON_N1_Y1);
  if (y + 1 < height) applyDistributedError(pixels, width, x, y + 1, quantError, ERROR_ATKINSON_P0_Y1);
  if (x + 1 < width && y + 1 < height) applyDistributedError(pixels, width, x + 1, y + 1, quantError, ERROR_ATKINSON_P1_Y1);
  if (x + 2 < width && y + 1 < height) applyDistributedError(pixels, width, x + 2, y + 1, quantError, ERROR_ATKINSON_P2_Y1);
  if (y + 2 < height) applyDistributedError(pixels, width, x, y + 2, quantError, ERROR_ATKINSON_P0_Y2);
}

export function applyAtkinsonDithering(imageData, colors) {
  const pixels = imageData.data;
  for (let y = 0; y < imageData.height; y++) {
    for (let x = 0; x < imageData.width; x++) {
      const index = (y * imageData.width + x) * 4;
      const oldPixel = { r: pixels[index], g: pixels[index + 1], b: pixels[index + 2] };
      const newPixel = findNearestColor(oldPixel, colors);
      pixels[index] = newPixel.r; pixels[index + 1] = newPixel.g; pixels[index + 2] = newPixel.b;
      const quantError = { r: oldPixel.r - newPixel.r, g: oldPixel.g - newPixel.g, b: oldPixel.b - newPixel.b };
      distributeAtkinsonError(pixels, imageData.width, imageData.height, x, y, quantError);
    }
  }
}

export function applyQuantizationOnly(imageData, colors) {
  const pixels = imageData.data;
  for (let y = 0; y < imageData.height; y++) {
    for (let x = 0; x < imageData.width; x++) {
      const index = (y * imageData.width + x) * 4;
      const oldPixel = { r: pixels[index], g: pixels[index + 1], b: pixels[index + 2] };
      const newPixel = findNearestColor(oldPixel, colors);
      pixels[index] = newPixel.r; pixels[index + 1] = newPixel.g; pixels[index + 2] = newPixel.b;
    }
  }
}

export function mapCIndexToRGB(cIndex) {
  const targetPalette = PALETTES.SPECTRA6;
  let paletteIndex = -1;
  switch (cIndex) {
    case 0: paletteIndex = 0; break;
    case 1: paletteIndex = 1; break;
    case 2: paletteIndex = 2; break;
    case 3: paletteIndex = 3; break;
    case 5: paletteIndex = 4; break;
    case 6: paletteIndex = 5; break;
    default: return targetPalette[0];
  }
  return (paletteIndex >= 0 && paletteIndex < targetPalette.length) ? targetPalette[paletteIndex] : targetPalette[0];
}

export function renderBinDataToImageData(arrayBuffer) {
  const expectedSize = TARGET_OUTPUT_HEIGHT * Math.floor(TARGET_OUTPUT_WIDTH / 2);
  if (arrayBuffer.byteLength !== expectedSize) {
    throw new Error(`Invalid .bin file size. Expected ${expectedSize} bytes, got ${arrayBuffer.byteLength} bytes.`);
  }

  const byteArray = new Uint8Array(arrayBuffer);
  const outputImageData = new ImageData(TARGET_OUTPUT_WIDTH, TARGET_OUTPUT_HEIGHT);
  const pixels = outputImageData.data;
  let byteIndex = 0;

  for (let y = 0; y < TARGET_OUTPUT_HEIGHT; y++) {
    for (let xPair = 0; xPair < Math.floor(TARGET_OUTPUT_WIDTH / 2); xPair++) {
      const byte = byteArray[byteIndex++];
      const cIdx1 = (byte >> 4) & 0x0F;
      const cIdx2 = byte & 0x0F;

      const rgb1 = mapCIndexToRGB(cIdx1);
      const rgb2 = mapCIndexToRGB(cIdx2);

      const pixel1CanvasIdx = (y * TARGET_OUTPUT_WIDTH + (xPair * 2)) * 4;
      pixels[pixel1CanvasIdx] = rgb1.r; pixels[pixel1CanvasIdx + 1] = rgb1.g; pixels[pixel1CanvasIdx + 2] = rgb1.b; pixels[pixel1CanvasIdx + 3] = 255;

      const pixel2CanvasIdx = (y * TARGET_OUTPUT_WIDTH + (xPair * 2 + 1)) * 4;
      pixels[pixel2CanvasIdx] = rgb2.r; pixels[pixel2CanvasIdx + 1] = rgb2.g; pixels[pixel2CanvasIdx + 2] = rgb2.b; pixels[pixel2CanvasIdx + 3] = 255;
    }
  }
  return outputImageData;
}

const C_EXPORT_PALETTE = PALETTES.SPECTRA6;

function findNearestCExportPaletteIndex(pixel) {
  let nearestIndex = 0;
  let minDistance = colorDistanceSquared(pixel, C_EXPORT_PALETTE[0]);
  for (let i = 1; i < C_EXPORT_PALETTE.length; i++) {
    const distance = colorDistanceSquared(pixel, C_EXPORT_PALETTE[i]);
    if (distance < minDistance) {
      minDistance = distance;
      nearestIndex = i;
    }
  }
  return nearestIndex;
}

export function mapPixelToCIndex(pixelColor) {
  const paletteIndex = findNearestCExportPaletteIndex(pixelColor);
  return paletteIndex > 3 ? paletteIndex + 1 : paletteIndex;
}

export function generateCArrayString(imageData) {
  const pixels = imageData.data;
  const width = imageData.width;
  const height = imageData.height;

  let cArrayString = `#include "ImageData.h"\n`;
  cArrayString += `// 6 Color Image Data ${width}*${height} \n`;
  const arraySize = height * Math.floor(width / 2);
  cArrayString += `const unsigned char Image6color[${arraySize}] = {\n`;

  let byteCount = 0;
  for (let j = 0; j < height; j++) {
    for (let i = 0; i < Math.floor(width / 2); i++) {
      const idx1 = (j * width + (i * 2)) * 4;
      const color1 = { r: pixels[idx1], g: pixels[idx1 + 1], b: pixels[idx1 + 2] };
      const c1 = mapPixelToCIndex(color1);

      const idx2 = (j * width + (i * 2 + 1)) * 4;
      const color2 = { r: pixels[idx2], g: pixels[idx2 + 1], b: pixels[idx2 + 2] };
      const c2 = mapPixelToCIndex(color2);

      const packedByte = (c1 << 4) | c2;
      cArrayString += `0x${packedByte.toString(16).padStart(2, '0').toUpperCase()},`;

      byteCount++;
      if (byteCount % 16 === 0 && byteCount > 0 && byteCount < arraySize) {
        cArrayString += "\n";
      }
    }
  }
  if (arraySize > 0 && cArrayString.endsWith(',')) {
    cArrayString = cArrayString.slice(0, -1);
  }
  cArrayString += "\n};\n";
  return cArrayString;
}

export function generateBinData(imageData) {
  const pixels = imageData.data;
  const width = imageData.width;
  const height = imageData.height;

  const bufferSize = height * Math.floor(width / 2);
  const byteArray = new Uint8Array(bufferSize);
  let byteIndex = 0;

  for (let j = 0; j < height; j++) {
    for (let i = 0; i < Math.floor(width / 2); i++) {
      const idx1 = (j * width + (i * 2)) * 4;
      const color1 = { r: pixels[idx1], g: pixels[idx1 + 1], b: pixels[idx1 + 2] };
      const c1 = mapPixelToCIndex(color1);

      const idx2 = (j * width + (i * 2 + 1)) * 4;
      const color2 = { r: pixels[idx2], g: pixels[idx2 + 1], b: pixels[idx2 + 2] };
      const c2 = mapPixelToCIndex(color2);

      const packedByte = (c1 << 4) | c2;
      byteArray[byteIndex++] = packedByte;
    }
  }
  return byteArray;
}