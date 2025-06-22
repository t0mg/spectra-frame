#include "HibernationManager.h"
#include "Debug.h"
#include "EPD_13in3e.h" // For EPD_13IN3E_Sleep
#include "DEV_Config.h" // For DEV_Module_Exit

HibernationManager::HibernationManager(unsigned long timeoutMillis) {
    inactivityTimeoutMillis = timeoutMillis;
    lastActivityTimeMillis = millis(); // Initialize with current time
    paused = false;
}

void HibernationManager::resetActivityTimer() {
    lastActivityTimeMillis = millis();
    Debugln("Hibernation: Activity timer reset.");
}

void HibernationManager::pause() {
    paused = true;
    Debugln("Hibernation: Timer paused.");
}

void HibernationManager::resumeAndReset() {
    paused = false;
    resetActivityTimer(); // Reset activity timer as resuming from a significant operation is an activity
    Debugln("Hibernation: Timer resumed and activity reset.");
}

bool HibernationManager::shouldGoToSleep() {
    if (paused) return false; // Don't go to sleep if paused
    if (millis() - lastActivityTimeMillis > inactivityTimeoutMillis) {
        Debugln("Hibernation: Inactivity timeout reached.");
        return true;
    }
    return false;
}

void HibernationManager::goToDeepSleep() {
    Debugln("Hibernation: Preparing for deep sleep...");

    // Wait for E-Paper to be idle before sleeping
    Debug("Hibernation: Checking E-Paper busy status... ");
    while (!DEV_Digital_Read(EPD_BUSY_PIN)) { // LOW is busy
        Debug(".");
        delay(100);
    }
    Debugln(" E-Paper is idle.");

    Debugln("Hibernation: Putting E-Paper to Sleep (EPD_13IN3E_Sleep)...");
    EPD_13IN3E_Sleep(); 

    Debugln("Hibernation: Exiting E-Paper Module (DEV_Module_Exit)...");
    DEV_Module_Exit(); 

    Debugln("Hibernation: Entering ESP32 deep sleep indefinitely.");
    #ifdef USE_DEBUG
    Serial.flush(); 
    #endif
    esp_deep_sleep_start();
}

void HibernationManager::setInactivityTimeout(unsigned long timeoutMs) {
    inactivityTimeoutMillis = timeoutMs;
    resetActivityTimer();
}

bool HibernationManager::isPaused() const {
    return paused;
}