#include "../include/motion/MotionPlanner.hpp"
#include "../include/motion/MotionConfig.hpp"

uint16_t computeDelayMicros(float feedrate, float stepsPerMm, float minFeedrate, float maxFeedrate) {
    if (feedrate < minFeedrate) feedrate = minFeedrate;
    if (feedrate > maxFeedrate) feedrate = maxFeedrate;

    const float stepRate = (feedrate / 60.0f) * stepsPerMm; // step/s
    if (stepRate <= 16.0f) return 62500; // limite inferiore: evita overflow di uint16_t

    const uint32_t us = (uint32_t) (1e6f / stepRate);
    if (us < MotionConfig::MIN_STEP_INTERVAL_US) return MotionConfig::MIN_STEP_INTERVAL_US;
    return (uint16_t) us;
}
