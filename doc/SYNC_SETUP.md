# Two-Way Sync Setup Guide

This guide explains how to set up a free "Sync Server" using Google Apps Script and Google Drive to allow two SpectraFrames to stay in sync.

## 1. Create a placeholder file on Google Drive

1.  Upload a dummy file named `image.bin` to your Google Drive.
2.  Right-click the file -> **Get link**.
3.  Set access to "Anyone with the link" (don't worry, we will add an API key for security).
4.  Copy the **File ID** from the URL: `https://drive.google.com/file/d/FILE_ID/view?usp=sharing`.

## 2. Create the Google Apps Script

1.  Go to [script.google.com](https://script.google.com/).
2.  Click **New Project**.
3.  Replace the code with the following:

```javascript
// --- CONFIGURATION ---
const DRIVE_FILE_ID = 'YOUR_FILE_ID_HERE';
const API_KEY = 'YOUR_SECRET_PASSWORD'; // Set this to something unique
// ---------------------

function doPost(e) {
  if (e.parameter.apiKey !== API_KEY) return ContentService.createTextOutput("Unauthorized");

  try {
    const file = DriveApp.getFileById(DRIVE_FILE_ID);
    // bytes will be available if Content-Type was application/octet-stream
    const bytes = e.postData.bytes || Utilities.newBlob(e.postData.contents).getBytes();
    file.setContent(Utilities.newBlob(bytes, 'application/octet-stream', 'image.bin'));
    return ContentService.createTextOutput("Success");
  } catch (err) {
    return ContentService.createTextOutput("Error: " + err.message);
  }
}

function doGet(e) {
  if (e.parameter.apiKey !== API_KEY) return ContentService.createTextOutput("Unauthorized");

  try {
    const file = DriveApp.getFileById(DRIVE_FILE_ID);
    const etag = file.getLastUpdated().getTime().toString();

    // Check ETag provided by device
    if (e.parameter.etag === etag) {
      return ContentService.createTextOutput("304");
    }

    const downloadUrl = "https://drive.google.com/uc?export=download&id=" + DRIVE_FILE_ID;
    return ContentService.createTextOutput(etag + "|" + downloadUrl);
  } catch (err) {
    return ContentService.createTextOutput("Error: " + err.message);
  }
}
```

4.  Click **Deploy** -> **New Deployment**.
5.  Select type: **Web App**.
6.  Description: `SpectraFrame Sync`.
7.  Execute as: **Me**.
8.  Who has access: **Anyone**. (The script itself handles the API key authentication).
9.  Click **Deploy**, authorize the permissions, and copy the **Web App URL**.

## 3. Configure your SpectraFrames

1.  Open the Web Interface of your SpectraFrame.
2.  Go to **Settings** -> **Two-Way Sync**.
3.  Check **Enable Sync**.
4.  Paste the **Web App URL** into the **Sync URL** field.
5.  Enter your **API Key** (the secret password you set in the script).
6.  Set a **Check Interval** (e.g., 1 hour).
7.  Click **Save Sync Settings**.

Repeat this for the second SpectraFrame using the *same* URL and API Key.

## How it works

- When you upload an image to **Frame A**, it automatically pushes it to your Google Drive via the script.
- **Frame B** wakes up periodically (based on the interval), checks if the file on Drive is newer than its local copy, and downloads it if it has changed.
- If you upload to **Frame B**, it pushes to Drive, and **Frame A** will eventually pull it.
