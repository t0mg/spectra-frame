import * as UI from './ui.js';
import * as ImageProcessor from './image-processor.js';
import * as Config from './config.js';
import { debounce } from './utils.js';

const dom = UI.getDomElements();
const ctx = UI.getCanvasContext(dom);

let originalImage = null;
let currentRotationAngle = 0;
let lastDitheredImageData = null; // Stores ImageData object

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
  await UI.updateStatus(dom.statusInfo, 'Processing image...');
  UI.updateControlStates(dom, { isProcessing: true, currentIsBinMode: false });

  try {
    const settings = UI.getSelectedValues(dom);
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
    ctx.putImageData(imageData, 0, 0);
    lastDitheredImageData = imageData;
    UI.updateStatus(dom.statusInfo, 'Image processed.');
  } catch (error) {
    console.error("Error applying dithering:", error);
    UI.updateStatus(dom.statusInfo, `Error processing image: ${error.message}`);
  } finally {
    UI.updateControlStates(dom, { isProcessing: false, currentIsBinMode: (originalImage === null) });
  }
}

const debouncedApplyDithering = debounce(applyDitheringAndStore, 250);

const eventHandlers = {
  onFileUpload: (event) => {
    UI.updateStatus(dom.statusInfo, '');
    const file = event.target.files[0];
    if (!file) return;

    const reader = new FileReader();
    UI.updateControlStates(dom, { isProcessing: true, currentIsBinMode: (originalImage === null) });

    reader.onload = async (e) => {
      await new Promise(resolve => requestAnimationFrame(resolve)); // UI update
      try {
        if (file.name.toLowerCase().endsWith('.bin')) {
          UI.updateStatus(dom.statusInfo, 'Reading .bin file...');
          const arrayBuffer = e.target.result;
          const imageData = ImageProcessor.renderBinDataToImageData(arrayBuffer);
          UI.setCanvasDimensions(dom, imageData.width, imageData.height);
          ctx.putImageData(imageData, 0, 0);
          lastDitheredImageData = imageData;
          originalImage = null; // Signifies raw data loaded
          currentRotationAngle = 0;
          resetAdjustmentSlidersToDefault();
          UI.updateStatus(dom.statusInfo, '.bin file loaded. Dithering and image adjustments cannot be changed.');
          UI.applyCanvasFitClass(dom, UI.getSelectedValues(dom).fitToScreen);
        } else if (file.type.startsWith('image/')) {
          UI.updateStatus(dom.statusInfo, 'Loading image...');
          const img = new Image();
          img.onload = async () => {
            await new Promise(resolve => requestAnimationFrame(resolve));
            originalImage = img;
            currentRotationAngle = 0;
            resetAdjustmentSlidersToDefault();
            applyDitheringAndStore(); // This will set status and update controls
          };
          img.onerror = () => {
            UI.updateStatus(dom.statusInfo, "Error loading image file. It might be corrupted or unsupported.");
            UI.updateControlStates(dom, { isProcessing: false, currentIsBinMode: (originalImage === null) });
          };
          img.src = e.target.result;
        } else {
          UI.updateStatus(dom.statusInfo, "Unsupported file type. Please upload an image or .bin file.");
          dom.uploadInput.value = ''; // Reset file input
          UI.updateControlStates(dom, { isProcessing: false, currentIsBinMode: (originalImage === null) });
        }
      } catch (error) {
        console.error("Error processing file:", error);
        UI.updateStatus(dom.statusInfo, `Error: ${error.message}`);
        UI.updateControlStates(dom, { isProcessing: false, currentIsBinMode: (originalImage === null) });
      } finally {
        // Final state update, applyDitheringAndStore handles its own for image case
        if (file.name.toLowerCase().endsWith('.bin')) {
          UI.updateControlStates(dom, { isProcessing: false, currentIsBinMode: true });
        }
      }
    };
    reader.onerror = () => {
      UI.updateStatus(dom.statusInfo, "Error: Could not read file.");
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
      applyDitheringAndStore();
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
  },
  onDownloadPNG: () => {
    if (!lastDitheredImageData && !originalImage) {
      UI.updateStatus(dom.statusInfo, "Please load or process an image first.");
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
      UI.updateStatus(dom.statusInfo, "Please load or process an image first.");
    }
  },
  onDownloadBIN: async () => {
    if (!lastDitheredImageData && originalImage) await applyDitheringAndStore();
    if (lastDitheredImageData) {
      const byteArray = ImageProcessor.generateBinData(lastDitheredImageData);
      const blob = new Blob([byteArray], { type: 'application/octet-stream' });
      UI.triggerDownload(blob, 'dithered_image.bin');
    } else {
      UI.updateStatus(dom.statusInfo, "Please load or process an image first.");
    }
  },
  onUploadToDevice: async () => {
    UI.updateControlStates(dom, { isProcessing: true, currentIsBinMode: (originalImage === null) });
    UI.updateStatus(dom.statusInfo, 'Preparing image for upload...');
    await new Promise(resolve => requestAnimationFrame(resolve));

    let byteArray;
    try {
      if (!lastDitheredImageData && originalImage) {
        await applyDitheringAndStore(); // This will set lastDitheredImageData
      }
      if (lastDitheredImageData) {
        byteArray = ImageProcessor.generateBinData(lastDitheredImageData);
      } else {
        UI.updateStatus(dom.statusInfo, 'No image data to upload. Please load/process an image.');
        UI.updateControlStates(dom, { isProcessing: false, currentIsBinMode: (originalImage === null) });
        return;
      }

      if (byteArray) {
        UI.updateUploadProgress(dom, 0); // Reset progress bar
        UI.updateStatus(dom.statusInfo, 'Starting upload...');

        const formData = new FormData();
        const blob = new Blob([byteArray], { type: 'application/octet-stream' });
        formData.append('imageFile', blob, 'dithered_image.bin');

        const xhr = new XMLHttpRequest();
        xhr.open('POST', '/api/upload', true);

        xhr.upload.onprogress = function (event) {
          if (event.lengthComputable) {
            const percentComplete = (event.loaded / event.total) * 100;
            UI.updateUploadProgress(dom, percentComplete);
            UI.updateStatus(dom.statusInfo, `Uploading: ${Math.round(percentComplete)}%`);
          }
        };
        xhr.onload = function () {
          UI.updateUploadProgress(dom, 100); // Ensure it hides
          if (xhr.status >= 200 && xhr.status < 300) {
            UI.updateStatus(dom.statusInfo, `Upload successful! Server: ${xhr.responseText}`);
          } else {
            UI.updateStatus(dom.statusInfo, `Error: Upload failed. Status ${xhr.status}: ${xhr.responseText}`);
          }
          UI.updateControlStates(dom, { isProcessing: false, currentIsBinMode: (originalImage === null) });
        };
        xhr.onerror = function () {
          UI.updateUploadProgress(dom, 100);
          UI.updateStatus(dom.statusInfo, 'Error: Upload failed due to a network error.');
          UI.updateControlStates(dom, { isProcessing: false, currentIsBinMode: (originalImage === null) });
        };
        xhr.onabort = function () {
          UI.updateUploadProgress(dom, 100);
          UI.updateStatus(dom.statusInfo, 'Upload aborted.');
          UI.updateControlStates(dom, { isProcessing: false, currentIsBinMode: (originalImage === null) });
        };
        xhr.send(formData);
      }
    } catch (e) {
      console.error("Error during upload preparation:", e);
      UI.updateStatus(dom.statusInfo, `Error preparing for upload: ${e.message}`);
      UI.updateControlStates(dom, { isProcessing: false, currentIsBinMode: (originalImage === null) });
    }
  },
  onWifiConfigSave: async (event) => {
    event.preventDefault();
    UI.updateStatus(dom.wifiConfigStatus, "Saving WiFi configuration...", false);
    const ssid = dom.wifiSsidInput.value;
    const password = dom.wifiPasswordInput.value;

    const result = await Config.saveWifiConfig(ssid, password);
    UI.updateStatus(dom.wifiConfigStatus, result.message, !result.success);
    if (result.success) {
      // Optionally clear form or give further instructions
      // dom.wifiConfigForm.reset();
    }
  },
  onFirmwareUpload: async (event) => {
    event.preventDefault();
    const updateFile = dom.otaFileInput.files[0];
    const selectedUpdateType = dom.otaUpdateType.querySelector('input:checked').value;
    if (!updateFile) {
      UI.updateStatus(dom.otaStatus, 'No file selected for upload.', true);
      return;
    }

    const typeText = selectedUpdateType === 'firmware' ? 'Firmware' : 'Filesystem';
    UI.updateStatus(dom.otaStatus, `Uploading ${typeText}: 0%`);
    dom.otaProgressBar.style.display = 'block';
    dom.otaProgressBar.value = 0;

    const formData = new FormData();
    formData.append('updateType', selectedUpdateType);
    formData.append('updateFileSize', updateFile.size.toString());
    formData.append('updateFile', updateFile, updateFile.name);

    const xhr = new XMLHttpRequest();
    xhr.open('POST', '/api/update', true);

    xhr.upload.onprogress = function (event) {
      if (event.lengthComputable) {
        const percentComplete = Math.round((event.loaded / event.total) * 100);
        dom.otaProgressBar.value = percentComplete;
        UI.updateStatus(dom.otaStatus, `Uploading ${typeText}: ${percentComplete}%`);
      }
    };

    xhr.onload = function () {
      dom.otaProgressBar.style.display = 'none';
      if (xhr.status === 200) {
        UI.updateStatus(dom.otaStatus, `${xhr.responseText}`);
      } else {
        UI.updateStatus(dom.otaStatus, `${typeText} upload failed: ${xhr.status} ${xhr.responseText}`, true);
      }
    };

    xhr.onerror = function () {
      dom.otaProgressBar.style.display = 'none';
      UI.updateStatus(dom.otaStatus, `${typeText} upload error: Network error or server unavailable.`, true);
    };

    xhr.send(formData);
  }
};

function init() {
  UI.initTabs(dom);
  UI.initEventListeners(dom, eventHandlers);
  resetAdjustmentSlidersToDefault();
  UI.updateControlStates(dom, { isProcessing: false, currentIsBinMode: (originalImage === null) });
  UI.applyCanvasFitClass(dom, UI.getSelectedValues(dom).fitToScreen);
  if (dom.advancedControlsContainer) dom.advancedControlsContainer.style.display = 'none'; // Default hidden
  UI.updateStatus(dom.statusInfo, "Ready. Select an image or .bin file.");
}

document.addEventListener('DOMContentLoaded', init);