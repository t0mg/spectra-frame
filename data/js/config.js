/**
 * Saves WiFi configuration to the device.
 * @param {string} ssid - The SSID of the WiFi network.
 * @param {string} password - The password for the WiFi network.
 * @returns {Promise<object>} A promise that resolves to an object { success: boolean, message: string }.
 */
export async function saveWifiConfig(ssid, password) {
  const formData = new FormData();
  formData.append('ssid', ssid);
  formData.append('password', password);

  try {
    const response = await fetch('/api/config', {
      method: 'POST',
      body: formData,
    });
    const responseText = await response.text();
    return { success: response.ok, message: responseText || (response.ok ? "Configuration saved. Restarting device..." : `Error: ${response.status}`) };
  } catch (error) {
    console.error('Error saving WiFi config:', error);
    return { success: false, message: `Network error: ${error.message}` };
  }
}

/**
 * Fetches the current device status (IP, thresholds, etc.).
 * @returns {Promise<object|null>} A promise that resolves to the status object or null on error.
 */
export async function getStatus() {
  try {
    const response = await fetch('/api/status');
    if (!response.ok) {
      throw new Error(`HTTP error! status: ${response.status}`);
    }
    return await response.json();
  } catch (error) {
    console.error('Error fetching device status:', error);
    return null;
  }
}

/**
 * Fetches the current touch sensor reading.
 * @returns {Promise<object|null>} A promise that resolves to {value: number, threshold: number} or null on error.
 */
export async function getTouchValue() {
  try {
    const response = await fetch('/api/touch_read');
    if (!response.ok) {
      return null; // Fail silently for polling
    }
    return await response.json();
  } catch (error) {
    // console.error('Error polling touch value:', error); // Can be noisy
    return null;
  }
}

/**
 * Sets the touch detection threshold on the device.
 * @param {number} threshold - The new threshold value.
 * @returns {Promise<object>} A promise that resolves to an object { success: boolean, message: string }.
 */
export async function setTouchThreshold(threshold) {
  const formData = new FormData();
  formData.append('threshold', threshold);
  try {
    const response = await fetch('/api/set_touch_threshold', { method: 'POST', body: formData });
    const text = await response.text();
    return { success: response.ok, message: text };
  } catch (error) {
    return { success: false, message: `Network error: ${error.message}` };
  }
}

/**
 * Sets the sleep delay on the device.
 * @param {number} minutes - The new sleep delay in minutes.
 * @returns {Promise<object>} A promise that resolves to an object { success: boolean, message: string }.
 */
export async function setSleepDelay(minutes) {
  const formData = new FormData();
  formData.append('delay_minutes', minutes);
  try {
    const response = await fetch('/api/set_sleep_delay', { method: 'POST', body: formData });
    const text = await response.text();
    return { success: response.ok, message: text };
  } catch (error) {
    return { success: false, message: `Network error: ${error.message}` };
  }
}

/**
 * Commands the device to enter deep sleep immediately.
 * @returns {Promise<object>} A promise that resolves to an object { success: boolean, message: string }.
 */
export async function sleepNow() {
  try {
    const response = await fetch('/api/sleep_now', { method: 'POST' });
    const text = await response.text();
    return { success: response.ok, message: text };
  } catch (error) {
    return { success: false, message: `Network error: ${error.message}` };
  }
}

/**
 * Fetches the current display update status.
 * @returns {Promise<object|null>} A promise that resolves to { status, message, inProgress, resetReason } or null on error.
 */
export async function getDisplayStatus() {
  try {
    const response = await fetch('/api/display_status', { cache: 'no-store' });
    if (!response.ok) {
      throw new Error(`HTTP error! status: ${response.status}`);
    }
    return await response.json();
  } catch (error) {
    console.error('Error fetching display status:', error);
    return null;
  }
}

/**
 * Sets the display orientation on the device.
 * @param {string} orientation - The new orientation ('portrait' or 'landscape').
 * @returns {Promise<object>} A promise that resolves to an object { success: boolean, message: string }.
 */
export async function setOrientation(orientation) {
  const formData = new FormData();
  formData.append('orientation', orientation);
  try {
    const response = await fetch('/api/set_orientation', { method: 'POST', body: formData });
    const text = await response.text();
    return { success: response.ok, message: text };
  } catch (error) {
    return { success: false, message: `Network error: ${error.message}` };
  }
}