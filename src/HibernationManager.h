#ifndef HIBERNATION_MANAGER_H
#define HIBERNATION_MANAGER_H

#include <Arduino.h>

class HibernationManager {
public:
    HibernationManager(unsigned long timeoutMillis);
    void resetActivityTimer();
    void pause();
    void resumeAndReset();
    bool shouldGoToSleep();
    void goToDeepSleep();
    void setInactivityTimeout(unsigned long timeoutMs);
    bool isPaused() const;

private:
    unsigned long inactivityTimeoutMillis;
    unsigned long lastActivityTimeMillis;
    bool paused;
};

#endif // HIBERNATION_MANAGER_H