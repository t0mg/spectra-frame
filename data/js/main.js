import * as UI from './ui.js';
import * as ImageProcessor from './image-processor.js';
import { WebGLImageProcessor } from './webgl-processor.js';
import * as Config from './config.js';
import { debounce } from './utils.js';

let dom = null; // Will be initialized in init()
let ctx = null; // Will be initialized in init()

let originalImage = null;
let currentRotationAngle = 0;
let webglProcessor = null;
let lastDitheredImageData = null; // Stores ImageData object
let touchPollInterval = null;

const DEFAULT_BRIGHTNESS = 0;
const DEFAULT_CONTRAST = 100; // Represents 1.00
const DEFAULT_GAMMA = 100;    // Represents 1.00

function resetAdjustmentSlidersToDefault() {
  UI.resetAdjustmentSliderDisplays(dom, {
    brightness: DEFAULT_BRIGHTNESS,
    contrast: DEFAULT_CONTRAST,
    gamma: DEFAULT_GAMMA
  });
}

async function applyDitheringAndStore() {
  if (!originalImage) {
    return;
  }
  await UI.updateStatus('Processing image...');
  UI.updateControlStates(dom, { isProcessing: true, currentIsBinMode: false });

  try {
    if (!webglProcessor) {
      webglProcessor = new WebGLImageProcessor();
    }

    const settings = UI.getSelectedValues(dom);
    /*
    const orientedImage = ImageProcessor.getOrientedSourceCanvas(originalImage, currentRotationAngle);
    const sourceToScale = ImageProcessor.getProcessedSourceCanvas(orientedImage, settings.brightness, settings.contrast, settings.gamma);

    UI.setCanvasDimensions(dom, ImageProcessor.TARGET_OUTPUT_WIDTH, ImageProcessor.TARGET_OUTPUT_HEIGHT);

    let scaleFactor;
    if (settings.scalingOption === 'cover') {
      scaleFactor = Math.max(ImageProcessor.TARGET_OUTPUT_WIDTH / sourceToScale.width, ImageProcessor.TARGET_OUTPUT_HEIGHT / sourceToScale.height);
    } else {
      scaleFactor = Math.min(ImageProcessor.TARGET_OUTPUT_WIDTH / sourceToScale.width, ImageProcessor.TARGET_OUTPUT_HEIGHT / sourceToScale.height);
    }

    const scaledWidth = sourceToScale.width * scaleFactor;
    const scaledHeight = sourceToScale.height * scaleFactor;
    const offsetX = (ImageProcessor.TARGET_OUTPUT_WIDTH - scaledWidth) / 2;
    const offsetY = (ImageProcessor.TARGET_OUTPUT_HEIGHT - scaledHeight) / 2;

    ctx.clearRect(0, 0, dom.canvas.width, dom.canvas.height);
    ctx.drawImage(sourceToScale, offsetX, offsetY, scaledWidth, scaledHeight);

    const imageData = ctx.getImageData(0, 0, dom.canvas.width, dom.canvas.height);
    const colors = ImageProcessor.getColors(settings.colorPalette);

    switch (settings.ditherAlgorithm) {
      case 'floyd-steinberg': ImageProcessor.applyFloydSteinbergDithering(imageData, colors); break;
      case 'atkinson': ImageProcessor.applyAtkinsonDithering(imageData, colors); break;
      case 'blue-noise': ImageProcessor.applyBlueNoiseDithering(imageData, colors); break;
      case 'none': ImageProcessor.applyQuantizationOnly(imageData, colors); break;
      default: ImageProcessor.applyQuantizationOnly(imageData, colors);
    }
      */

    const orientedSource = ImageProcessor.getOrientedSourceCanvas(originalImage, currentRotationAngle);

    // The WebGL processor handles adjustments. It will also handle dithering for GPU-based algorithms.
    webglProcessor.process(orientedSource, settings);

    let imageData = webglProcessor.getImageData();

    // Handle CPU dithering if needed.
    const colors = ImageProcessor.getColors(settings.colorPalette);
    if (settings.ditherAlgorithm === 'floyd-steinberg') {
      ImageProcessor.applyFloydSteinbergDithering(imageData, colors);
    } else if (settings.ditherAlgorithm === 'atkinson') {
      ImageProcessor.applyAtkinsonDithering(imageData, colors);
    }

    UI.setCanvasDimensions(dom, imageData.width, imageData.height);

    ctx.putImageData(imageData, 0, 0);
    lastDitheredImageData = imageData;
    UI.updateStatus('Image processed.');
  } catch (error) {
    console.error("Error applying dithering:", error);
    // UI.updateStatus(`Error processing image: ${error.message}`);
    const message = error.message.includes("WebGL") ? "A WebGL error occurred. Your browser might not support it or it could be disabled." : `Error processing image: ${error.message}`;
    UI.updateStatus(message, true);

  } finally {
    UI.updateControlStates(dom, { isProcessing: false, currentIsBinMode: (originalImage === null) });
  }
}

const debouncedApplyDithering = debounce(applyDitheringAndStore, 250);

function applyCanvasOrientation(orientation) {
    if (orientation === 'landscape') {
        dom.canvas.classList.add('canvas-landscape');
    } else {
        dom.canvas.classList.remove('canvas-landscape');
    }
}

async function pollDisplayUpdateStatus() {
  const startTime = Date.now();
  const timeoutMs = 90000; // 90 seconds timeout
  const pollIntervalMs = 2000; // Poll every 2 seconds

  // Give ESP32 a short moment to enter display refresh in main loop
  await new Promise(resolve => setTimeout(resolve, 1500));

  while (Date.now() - startTime < timeoutMs) {
    const elapsedSec = Math.round((Date.now() - startTime) / 1000);
    const data = await Config.getDisplayStatus();

    if (!data) {
      // Possible brownout reboot causing temporary WiFi disconnection
      UI.updateStatus(`Waiting for device response... (${elapsedSec}s)`);
    } else {
      if (data.status === 'success') {
        UI.updateStatus(`Display updated successfully! (${elapsedSec}s)`);
        UI.updateControlStates(dom, { isProcessing: false, currentIsBinMode: (originalImage === null) });
        return;
      }

      if (data.status === 'brownout') {
        const msg = data.message || 'Brownout reset detected during display refresh! Power supply/battery voltage dropped.';
        UI.updateStatus(`Error: ${msg}`, true);
        UI.updateControlStates(dom, { isProcessing: false, currentIsBinMode: (originalImage === null) });
        return;
      }

      if (data.status === 'error') {
        const msg = data.message || 'Display update failed.';
        UI.updateStatus(`Error: ${msg}`, true);
        UI.updateControlStates(dom, { isProcessing: false, currentIsBinMode: (originalImage === null) });
        return;
      }

      if (data.inProgress || data.status === 'updating') {
        UI.updateStatus(`Refreshing e-Paper display... (${elapsedSec}s)`);
      } else if (data.status === 'idle') {
        if (elapsedSec > 5) {
          UI.updateStatus('Display update complete.');
          UI.updateControlStates(dom, { isProcessing: false, currentIsBinMode: (originalImage === null) });
          return;
        }
        UI.updateStatus(`Refreshing e-Paper display... (${elapsedSec}s)`);
      }
    }

    await new Promise(resolve => setTimeout(resolve, pollIntervalMs));
  }

  UI.updateStatus('Display update timed out waiting for completion.', true);
  UI.updateControlStates(dom, { isProcessing: false, currentIsBinMode: (originalImage === null) });
}

const eventHandlers = {
  onFileUpload: (event) => {
    UI.updateStatus('');
    const file = event.target.files[0];
    if (!file) return;

    const reader = new FileReader();
    UI.updateControlStates(dom, { isProcessing: true, currentIsBinMode: (originalImage === null) });

    reader.onload = async (e) => {
      await new Promise(resolve => requestAnimationFrame(resolve)); // Allow UI to update
      try {
        if (file.name.toLowerCase().endsWith('.bin')) {
          UI.updateStatus('Reading .bin file...');
          const arrayBuffer = e.target.result;
          const imageData = ImageProcessor.renderBinDataToImageData(arrayBuffer);
          UI.setCanvasDimensions(dom, imageData.width, imageData.height);
          ctx.putImageData(imageData, 0, 0);
          lastDitheredImageData = imageData;
          originalImage = null; // Signifies raw data loaded
          currentRotationAngle = 0;
          resetAdjustmentSlidersToDefault();
          UI.updateStatus('.bin file loaded. Dithering and image adjustments cannot be changed.');
          UI.applyCanvasFitClass(dom, UI.getSelectedValues(dom).fitToScreen);
        } else if (file.type.startsWith('image/')) {
          UI.updateStatus('Loading image...');
          const img = new Image();
          img.onload = async () => {
            await new Promise(resolve => requestAnimationFrame(resolve));
            originalImage = img;
            currentRotationAngle = 0;
            resetAdjustmentSlidersToDefault();
            applyDitheringAndStore(); // This will set status and update controls
          };
          img.onerror = () => {
            UI.updateStatus("Error loading image file. It might be corrupted or unsupported.", true);
            UI.updateControlStates(dom, { isProcessing: false, currentIsBinMode: (originalImage === null) });
          };
          img.src = e.target.result;
        } else {
          UI.updateStatus("Unsupported file type. Please upload an image or .bin file.", true);
          dom.uploadInput.value = ''; // Reset file input
          UI.updateControlStates(dom, { isProcessing: false, currentIsBinMode: (originalImage === null) });
        }
      } catch (error) {
        console.error("Error processing file:", error);
        UI.updateStatus(`Error: ${error.message}`, true);
        UI.updateControlStates(dom, { isProcessing: false, currentIsBinMode: (originalImage === null) });
      } finally {
        // Final state update, applyDitheringAndStore handles its own for image case
        if (file.name.toLowerCase().endsWith('.bin')) {
          UI.updateControlStates(dom, { isProcessing: false, currentIsBinMode: true });
        }
      }
    };
    reader.onerror = () => {
      UI.updateStatus("Error: Could not read file.", true);
      UI.updateControlStates(dom, { isProcessing: false, currentIsBinMode: (originalImage === null) });
    };

    if (file.name.toLowerCase().endsWith('.bin')) {
      reader.readAsArrayBuffer(file);
    } else if (file.type.startsWith('image/')) {
      reader.readAsDataURL(file);
    }
  },
  onDitheringSettingChanged: () => {
    if (originalImage && !dom.ditherAlgorithmSelect.disabled) { // Check if control is active
      // applyDitheringAndStore();
      debouncedApplyDithering();
    }
  },
  onRotateSource: () => {
    if (originalImage && !dom.rotateSourceButton.disabled) {
      currentRotationAngle = (currentRotationAngle + 90) % 360;
      applyDitheringAndStore();
    }
  },
  onFitCanvasToggle: (isChecked) => {
    UI.applyCanvasFitClass(dom, isChecked);
  },
  onSliderChange: (sliderName, value) => {
    if (!originalImage || dom.brightnessSlider.disabled) return; // Check one slider, assumes all are same state
    if (sliderName === 'brightness') UI.updateAdjustmentValueDisplays(dom, { brightness: value });
    if (sliderName === 'contrast') UI.updateAdjustmentValueDisplays(dom, { contrast: value });
    if (sliderName === 'gamma') UI.updateAdjustmentValueDisplays(dom, { gamma: value });
    debouncedApplyDithering();
  },
  onResetAdjustment: (type) => {
    if (!originalImage || dom.resetBrightnessButton.disabled) return;
    if (type === 'brightness') dom.brightnessSlider.value = DEFAULT_BRIGHTNESS;
    if (type === 'contrast') dom.contrastSlider.value = DEFAULT_CONTRAST;
    if (type === 'gamma') dom.gammaSlider.value = DEFAULT_GAMMA;
    applyDitheringAndStore(); // Apply immediately
    UI.updateAdjustmentValueDisplays(dom, {
      brightness: dom.brightnessSlider.value, contrast: dom.contrastSlider.value, gamma: dom.gammaSlider.value
    });
  },
  onDownloadPNG: () => {
    if (!lastDitheredImageData && !originalImage) {
      UI.updateStatus("Please load or process an image first.", true);
      return;
    }
    if (!lastDitheredImageData && originalImage) { // Process first if not done
      applyDitheringAndStore().then(() => {
        if (lastDitheredImageData) UI.triggerDownload(dom.canvas.toDataURL(), 'dithered_image.png');
      });
    } else if (lastDitheredImageData) {
      UI.triggerDownload(dom.canvas.toDataURL(), 'dithered_image.png');
    }
  },
  onDownloadCArray: async () => {
    if (!lastDitheredImageData && originalImage) await applyDitheringAndStore();
    if (lastDitheredImageData) {
      const cArrayString = ImageProcessor.generateCArrayString(lastDitheredImageData);
      const blob = new Blob([cArrayString], { type: 'text/plain' });
      UI.triggerDownload(blob, 'ImageData.cpp');
    } else {
      UI.updateStatus("Please load or process an image first.", true);
    }
  },
  onDownloadBIN: async () => {
    if (!lastDitheredImageData && originalImage) await applyDitheringAndStore();
    if (lastDitheredImageData) {
      const byteArray = ImageProcessor.generateBinData(lastDitheredImageData);
      const blob = new Blob([byteArray], { type: 'application/octet-stream' });
      UI.triggerDownload(blob, 'dithered_image.bin');
    } else {
      UI.updateStatus("Please load or process an image first.", true);
    }
  },
  onUploadToDevice: async () => {
    UI.updateControlStates(dom, { isProcessing: true, currentIsBinMode: (originalImage === null) });
    UI.updateStatus('Preparing image for upload...');
    await new Promise(resolve => requestAnimationFrame(resolve));

    let byteArray;
    try {
      if (!lastDitheredImageData && originalImage) {
        await applyDitheringAndStore(); // This will set lastDitheredImageData
      }
      if (lastDitheredImageData) {
        byteArray = ImageProcessor.generateBinData(lastDitheredImageData);
      } else {
        UI.updateStatus('No image data to upload. Please load/process an image.');
        UI.updateControlStates(dom, { isProcessing: false, currentIsBinMode: (originalImage === null) });
        return;
      }

      if (byteArray) {
        UI.updateUploadProgress(0); // Reset progress bar
        UI.updateStatus('Starting upload...');

        const formData = new FormData();
        const blob = new Blob([byteArray], { type: 'application/octet-stream' });
        formData.append('imageFile', blob, 'dithered_image.bin');

        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/api/upload', true);

        xhr.upload.onprogress = function (event) {
          if (event.lengthComputable) {
            const percentComplete = (event.loaded / event.total) * 100;
            UI.updateUploadProgress(percentComplete);
            UI.updateStatus(`Uploading: ${Math.round(percentComplete)}%`);
          }
        };
        xhr.onload = async function () {
          UI.updateUploadProgress(100); // Ensure it hides
          if (xhr.status >= 200 && xhr.status < 300) {
            UI.updateStatus('Upload complete! Refreshing e-Paper display...');
            await pollDisplayUpdateStatus();
          } else {
            UI.updateStatus(`Error: Upload failed. Status ${xhr.status}: ${xhr.responseText}`, true);
            UI.updateControlStates(dom, { isProcessing: false, currentIsBinMode: (originalImage === null) });
          }
        };
        xhr.onerror = function () {
          UI.updateUploadProgress(100);
          UI.updateStatus('Error: Upload failed due to a network error.', true);
          UI.updateControlStates(dom, { isProcessing: false, currentIsBinMode: (originalImage === null) });
        };
        xhr.onabort = function () {
          UI.updateUploadProgress(100);
          UI.updateStatus('Upload aborted.', true);
          UI.updateControlStates(dom, { isProcessing: false, currentIsBinMode: (originalImage === null) });
        };
        xhr.send(formData);
      }
    } catch (e) {
      console.error("Error during upload preparation:", e);
      UI.updateStatus(`Error preparing for upload: ${e.message}`, true);
      UI.updateControlStates(dom, { isProcessing: false, currentIsBinMode: (originalImage === null) });
    }
  },
  onWifiConfigSave: async (event) => {
    event.preventDefault();
    UI.updateStatus("Saving WiFi configuration...");
    const ssid = dom.wifiSsidInput.value;
    const password = dom.wifiPasswordInput.value;

    const result = await Config.saveWifiConfig(ssid, password);
    UI.updateStatus(result.message, !result.success);
    if (result.success) {
      // Optionally clear form or give further instructions
      // dom.wifiConfigForm.reset();
    }
  },
  onOrientationChange: async (event) => {
    const orientation = event.target.value;
    applyCanvasOrientation(orientation);
    UI.updateStatus('Saving orientation...');
    const result = await Config.setOrientation(orientation);
    UI.updateStatus(result.message, !result.success);
  },
  onSaveTouchThreshold: async () => {
    const threshold = dom.touchThresholdInput.value;
    if (!threshold || isNaN(threshold)) {
      UI.updateStatus('Invalid threshold value.', true);
      return;
    }
    UI.updateStatus('Saving...');
    const result = await Config.setTouchThreshold(parseInt(threshold));
    UI.updateStatus(result.message, !result.success);
  },
  onSaveSleepDelay: async () => {
    const delay = dom.sleepDelayInput.value;
    if (!delay || isNaN(delay) || delay < 1) {
      UI.updateStatus('Invalid delay. Must be >= 1 minute.', true);
      return;
    }
    UI.updateStatus('Saving...');
    const result = await Config.setSleepDelay(parseInt(delay));
    UI.updateStatus(result.message, !result.success);
  },
  onSleepNow: async () => {
    if (confirm('Are you sure you want to put the device to sleep? You will need to touch it to wake it up again.')) {
      UI.updateStatus('Commanding device to sleep...');
      const result = await Config.sleepNow();
      if (result.success) {
        UI.updateStatus('Device is going to sleep. The web interface will become unresponsive.');
      } else {
        UI.updateStatus(`Error: ${result.message}`, true);
      }
    }
  },
  onFirmwareUpload: async (event) => {
    event.preventDefault();
    const updateFile = dom.otaFileInput.files[0];
    const selectedUpdateType = dom.otaUpdateType.querySelector('input:checked').value;
    if (!updateFile) {
      UI.updateStatus('No file selected for upload.', true);
      return;
    }

    const typeText = selectedUpdateType === 'firmware' ? 'Firmware' : 'Filesystem';
    UI.updateStatus(`Uploading ${typeText}: 0%`);
    UI.updateUploadProgress(0);

    const formData = new FormData();
    formData.append('updateType', selectedUpdateType);
    formData.append('updateFileSize', updateFile.size.toString());
    formData.append('updateFile', updateFile, updateFile.name);

    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/api/update', true);

    xhr.upload.onprogress = function (event) {
      if (event.lengthComputable) {
        const percentComplete = Math.round((event.loaded / event.total) * 100);
        UI.updateUploadProgress(percentComplete);
        UI.updateStatus(`Uploading ${typeText}: ${percentComplete}%`);
      }
    };

    xhr.onload = function () {
      UI.updateUploadProgress(100); // Hide global progress bar
      if (xhr.status === 200) {
        UI.updateStatus(`${xhr.responseText}`);
      } else {
        UI.updateStatus(`${typeText} upload failed: ${xhr.status}: ${xhr.responseText}`, true);
      }
    };

    xhr.onerror = function () {
      dom.otaProgressBar.style.display = 'none';
      UI.updateStatus(`${typeText} upload error: Network error or server unavailable.`, true);
    };

    xhr.send(formData);
  }
};

async function loadDeviceStatus() {
  const status = await Config.getStatus();
  if (status) {
    if (dom.touchThresholdInput) dom.touchThresholdInput.value = status.touchThreshold;
    if (dom.sleepDelayInput) dom.sleepDelayInput.value = status.inactivityTimeoutMinutes;
    if (dom.voltageSpan) dom.voltageSpan.textContent = `${status.batteryVoltage}V`;
    if (dom.orientationRadios) {
      dom.orientationRadios.forEach(radio => {
        radio.checked = (radio.value === status.orientation);
      });
      applyCanvasOrientation(status.orientation);
    }
  }
}

function startTouchPolling() {
  if (touchPollInterval) clearInterval(touchPollInterval); // Clear any existing interval
  touchPollInterval = setInterval(async () => {
    const data = await Config.getTouchValue();
    if (data && dom.touchValueSpan) {
      dom.touchValueSpan.textContent = `${data.value} (Threshold: ${data.threshold})`;
      // Update a progress bar for visualization
      if (dom.touchValueBar) {
        // Assuming a max value of ~65535 for touchRead for the progress bar
        const maxTouchVal = 65535;
        const progress = 100 - (data.value / maxTouchVal * 100);
        dom.touchValueBar.value = progress;
      }
    } else {
      UI.updateStatus("Error getting touch value. Is the device asleep?", true);
      stopTouchPolling()
    }
  }, 1000); // Poll every second
}

function stopTouchPolling() {
  if (touchPollInterval) {
    clearInterval(touchPollInterval);
    touchPollInterval = null;
  }
}

async function checkInitialDisplayStatus() {
  const data = await Config.getDisplayStatus();
  if (data) {
    if (data.status === 'brownout') {
      UI.updateStatus(`Warning: ${data.message || 'Previous display update caused a brownout reset!'}`, true);
    } else if (data.status === 'error') {
      UI.updateStatus(`Notice: Previous display update failed (${data.message || 'Error'}).`, true);
    }
  }
}

function init() {
  dom = UI.getDomElements();
  ctx = UI.getCanvasContext(dom);
  UI.initTabs(dom);
  UI.initEventListeners(dom, eventHandlers);
  resetAdjustmentSlidersToDefault();
  UI.updateControlStates(dom, { isProcessing: false, currentIsBinMode: (originalImage === null) });
  UI.applyCanvasFitClass(dom, UI.getSelectedValues(dom).fitToScreen);
  if (dom.advancedControlsContainer) dom.advancedControlsContainer.style.display = 'none'; // Default hidden
  loadDeviceStatus();
  checkInitialDisplayStatus();

  // Listen for the custom event from ui.js to start/stop polling
  document.addEventListener('tabShown', (e) => {
    UI.updateStatus(""); // Clear status when switching tabs
    if (e.detail.tabId === 'config') {
      loadDeviceStatus(); // Refresh status when tab is shown
      startTouchPolling();
    } else {
      stopTouchPolling();
    }
  });

}

document.addEventListener('DOMContentLoaded', init);