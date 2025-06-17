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

private:
    unsigned long inactivityTimeoutMillis;
    unsigned long lastActivityTimeMillis;
    bool isPaused;
};

#endif // HIBERNATION_MANAGER_H