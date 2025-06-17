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