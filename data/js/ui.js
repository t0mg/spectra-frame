const DEFAULT_TAB = 'upload'; // Default tab to show

let _globalStatusElement = null; // Module-level variable for the global status element
let _globalProgressBarElement = null; // Module-level variable for the global progress bar element

export function getDomElements() {
  _globalStatusElement = document.getElementById('statusInfo');
  _globalProgressBarElement = document.getElementById('progressBar');

  return {
    // Dithering Tab Elements
    uploadInput: document.getElementById('uploadInput'),
    ditherAlgorithmSelect: document.getElementById('ditherAlgorithmSelect'),
    colorPaletteSelect: document.getElementById('colorPaletteSelect'),
    downloadButton: document.getElementById('download'),
    downloadCArrayButton: document.getElementById('downloadCArrayButton'),
    downloadBinButton: document.getElementById('downloadBinButton'),
    uploadToDeviceButton: document.getElementById('uploadToDeviceButton'),
    canvas: document.getElementById('canvas'),
    scalingOptionSelect: document.getElementById('scalingOption'),
    rotateSourceButton: document.getElementById('rotateSourceButton'),
    fitCanvasToScreenCheckbox: document.getElementById('fitCanvasToScreenCheckbox'),
    brightnessSlider: document.getElementById('brightnessSlider'),
    brightnessValueSpan: document.getElementById('brightnessValue'),
    contrastSlider: document.getElementById('contrastSlider'),
    contrastValueSpan: document.getElementById('contrastValue'),
    gammaSlider: document.getElementById('gammaSlider'),
    gammaValueSpan: document.getElementById('gammaValue'),
    resetBrightnessButton: document.getElementById('resetBrightness'),
    resetContrastButton: document.getElementById('resetContrast'),
    resetGammaButton: document.getElementById('resetGamma'),
    advancedToggleCheckbox: document.getElementById('advancedToggleCheckbox'),
    advancedControlsContainer: document.getElementById('advancedControlsContainer'),

    // Config Tab Elements
    wifiConfigForm: document.getElementById('wifiConfigForm'),
    wifiSsidInput: document.getElementById('wifiSsid'),
    wifiPasswordInput: document.getElementById('wifiPassword'),
    orientationRadios: document.querySelectorAll('input[name="orientation"]'),

    // Device Settings in Config Tab
    touchValueSpan: document.getElementById('touchValue'),
    touchValueBar: document.getElementById('touchValueBar'),
    touchThresholdInput: document.getElementById('touchThresholdInput'),
    saveTouchThresholdButton: document.getElementById('saveTouchThresholdButton'),
    sleepDelayInput: document.getElementById('sleepDelayInput'),
    saveSleepDelayButton: document.getElementById('saveSleepDelayButton'),
    sleepNowButton: document.getElementById('sleepNowButton'),

    // OTA Tab Elements
    otaConfigForm: document.getElementById('firmwareUploadForm'),
    otaFileInput: document.getElementById('updateFile'),
    otaUpdateType: document.getElementById('otaUpdateTypeSection'),
  };
}

export function initTabs() {
  const tabButtons = [...document.querySelectorAll(".tab-button")];
  const tabContents = [...document.querySelectorAll(".tab-content")];

  function showTab(tabIdToShow) {
    tabContents.forEach(content => {
      const isActive = content.id === tabIdToShow;
      content.classList.toggle('active', isActive);
    });

    tabButtons.forEach(button => {
      const isActive = button.dataset.tab === tabIdToShow;
      button.classList.toggle('active', isActive);
      button.setAttribute('aria-selected', isActive.toString());
      if (isActive) {
        // Dispatch a custom event that main.js can listen to
        document.dispatchEvent(new CustomEvent('tabShown', { detail: { tabId: tabIdToShow } }));
      }
    });
  }

  tabButtons.forEach(button => {
    button.addEventListener('click', () => {
      history.pushState(null, null, '#' + button.dataset.tab);
      showTab(button.dataset.tab);
    });
  });

  function onHashChange() {
    const targetTabId = window.location.hash.substring(1);
    if (targetTabId && tabButtons.some(button => button.dataset.tab === targetTabId)) {
      showTab(targetTabId);
    } else {
      showTab(DEFAULT_TAB);
    }
  }

  window.onhashchange = onHashChange;
  window.onload = onHashChange;
}

export function initEventListeners(dom, handlers) {
  dom.uploadInput.addEventListener('change', handlers.onFileUpload);
  dom.ditherAlgorithmSelect.addEventListener('change', handlers.onDitheringSettingChanged);
  dom.colorPaletteSelect.addEventListener('change', handlers.onDitheringSettingChanged);
  dom.scalingOptionSelect.addEventListener('change', handlers.onDitheringSettingChanged);
  dom.rotateSourceButton.addEventListener('click', handlers.onRotateSource);
  dom.fitCanvasToScreenCheckbox.addEventListener('change', (e) => handlers.onFitCanvasToggle(e.target.checked));

  dom.brightnessSlider.addEventListener('input', () => handlers.onSliderChange('brightness', dom.brightnessSlider.value));
  dom.contrastSlider.addEventListener('input', () => handlers.onSliderChange('contrast', dom.contrastSlider.value));
  dom.gammaSlider.addEventListener('input', () => handlers.onSliderChange('gamma', dom.gammaSlider.value));

  dom.resetBrightnessButton.addEventListener('click', () => handlers.onResetAdjustment('brightness'));
  dom.resetContrastButton.addEventListener('click', () => handlers.onResetAdjustment('contrast'));
  dom.resetGammaButton.addEventListener('click', () => handlers.onResetAdjustment('gamma'));

  dom.downloadButton.addEventListener('click', handlers.onDownloadPNG);
  dom.downloadCArrayButton.addEventListener('click', handlers.onDownloadCArray);
  dom.downloadBinButton.addEventListener('click', handlers.onDownloadBIN);
  dom.uploadToDeviceButton.addEventListener('click', handlers.onUploadToDevice);

  if (dom.advancedToggleCheckbox && dom.advancedControlsContainer) {
    dom.advancedToggleCheckbox.addEventListener('change', () => {
      dom.advancedControlsContainer.style.display = dom.advancedToggleCheckbox.checked ? 'block' : 'none';
    });
  }

  // Config Tab
  dom.wifiConfigForm.addEventListener('submit', handlers.onWifiConfigSave);

  dom.orientationRadios.forEach(radio => {
    radio.addEventListener('change', handlers.onOrientationChange);
  });

  // Device Settings in Config Tab
  dom.saveTouchThresholdButton.addEventListener('click', handlers.onSaveTouchThreshold);
  dom.saveSleepDelayButton.addEventListener('click', handlers.onSaveSleepDelay);
  dom.sleepNowButton.addEventListener('click', handlers.onSleepNow);

  // OTA Tab
  dom.otaConfigForm.addEventListener('submit', handlers.onFirmwareUpload);
}

export async function updateStatus(message, isError = false) {
  await new Promise(resolve => requestAnimationFrame(resolve));
  if (!_globalStatusElement) return; // Ensure it's initialized
  _globalStatusElement.textContent = message;
  _globalStatusElement.style.color = isError ? 'red' : '';
  await new Promise(resolve => requestAnimationFrame(resolve));
}

export function updateUploadProgress(percentage) {
  if (!_globalProgressBarElement) return; // Ensure it's initialized
  _globalProgressBarElement.value = percentage;
  // Only show the progress bar if there's actual progress (not 0 or 100)
  _globalProgressBarElement.style.display = (percentage > 0 && percentage < 100) ? 'block' : 'none';
  if (percentage === 100) { // Hide after completion
    _globalProgressBarElement.style.display = 'none';
  }
}

export function updateControlStates(dom, { isProcessing = false, currentIsBinMode = false }) {
  const allInteractiveControls = [...document.querySelectorAll('input,button:not(.tab-button),select')];
  const imageProcessingControls = [
    dom.ditherAlgorithmSelect, dom.colorPaletteSelect, dom.scalingOptionSelect,
    dom.rotateSourceButton, dom.brightnessSlider, dom.contrastSlider, dom.gammaSlider,
    dom.resetBrightnessButton, dom.resetContrastButton, dom.resetGammaButton
  ];

  allInteractiveControls.forEach(control => {
    if (control) control.disabled = isProcessing;
  });

  if (!isProcessing && currentIsBinMode) {
    imageProcessingControls.forEach(control => {
      if (control) control.disabled = true;
    });
    if (dom.colorPaletteSelect) dom.colorPaletteSelect.value = "SPECTRA6"; // Default for bin
  }
}

export function resetAdjustmentSliderDisplays(dom, defaults) {
  dom.brightnessSlider.value = defaults.brightness;
  dom.brightnessValueSpan.textContent = defaults.brightness;
  dom.contrastSlider.value = defaults.contrast;
  dom.contrastValueSpan.textContent = (defaults.contrast / 100).toFixed(2);
  dom.gammaSlider.value = defaults.gamma;
  dom.gammaValueSpan.textContent = (defaults.gamma / 100).toFixed(2);
}

export function updateAdjustmentValueDisplays(dom, values) {
  if (values.brightness !== undefined) dom.brightnessValueSpan.textContent = values.brightness;
  if (values.contrast !== undefined) dom.contrastValueSpan.textContent = (values.contrast / 100).toFixed(2);
  if (values.gamma !== undefined) dom.gammaValueSpan.textContent = (values.gamma / 100).toFixed(2);
}

export function getCanvasContext(dom) {
  return dom.canvas.getContext('2d', { willReadFrequently: true });
}

export function getCanvas(dom) {
  return dom.canvas;
}

export function setCanvasDimensions(dom, width, height) {
  dom.canvas.width = width;
  dom.canvas.height = height;
}

export function applyCanvasFitClass(dom, fit) {
  if (fit) {
    dom.canvas.classList.add('canvas-fit-screen');
  } else {
    dom.canvas.classList.remove('canvas-fit-screen');
  }
}

export function triggerDownload(blob, filename) {
  const link = document.createElement('a');
  link.href = URL.createObjectURL(blob);
  link.download = filename;
  document.body.appendChild(link); // Required for Firefox
  link.click();
  document.body.removeChild(link);
  URL.revokeObjectURL(link.href);
}

export function getSelectedValues(dom) {
  return {
    ditherAlgorithm: dom.ditherAlgorithmSelect.value,
    colorPalette: dom.colorPaletteSelect.value,
    scalingOption: dom.scalingOptionSelect.value,
    brightness: parseInt(dom.brightnessSlider.value),
    contrast: parseFloat(dom.contrastSlider.value) / 100,
    gamma: parseFloat(dom.gammaSlider.value) / 100,
    fitToScreen: dom.fitCanvasToScreenCheckbox.checked
  };
}