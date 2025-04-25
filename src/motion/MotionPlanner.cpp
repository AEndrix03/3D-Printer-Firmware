#include "../include/motion/MotionPlanner.hpp"
#include "../include/motion/MotionConfig.hpp"
#include <math.h>

unsigned int computeDelayMicros(float feedrate, float stepsPerMm, float minFeedrate, float maxFeedrate) {
    // Clamp feedrate in mm/min
    if (feedrate < minFeedrate) feedrate = minFeedrate;
    if (feedrate > maxFeedrate) feedrate = maxFeedrate;

    // Convert mm/min → mm/sec
    float speedMmSec = feedrate / 60.0f;

    // Compute step rate
    float stepRate = speedMmSec * stepsPerMm; // steps/sec

    // Avoid division by 0
    if (stepRate <= 0.0f) return 500;

    // Compute delay per step in microseconds
    float delay_s = 1.0f / stepRate;
    unsigned int delay_us = (unsigned int) (delay_s * 1e6f);

    // Safety clamp: prevent microsecond delay too small (AVR can't handle < 100 us well)
    if (delay_us < 200) delay_us = 200;

    return delay_us;
}


MotionPlan planMotion(const MotionCommand &cmd) {
    MotionPlan plan;

    float sx = cmd.x * MotionConfig::STEPS_PER_MM_X;
    float sy = cmd.y * MotionConfig::STEPS_PER_MM_Y;
    float sz = cmd.z * MotionConfig::STEPS_PER_MM_Z;

    plan.stepsX = (int) (fabs(sx) + 0.5f);
    plan.stepsY = (int) (fabs(sy) + 0.5f);
    plan.stepsZ = (int) (fabs(sz) + 0.5f);

    plan.dirX = cmd.x >= 0;
    plan.dirY = cmd.y >= 0;
    plan.dirZ = cmd.z >= 0;

    plan.delayMicrosX = computeDelayMicros(cmd.feedrate, MotionConfig::STEPS_PER_MM_X, MotionConfig::MIN_FEEDRATE_X,
                                           MotionConfig::MAX_FEEDRATE_X);
    plan.delayMicrosY = computeDelayMicros(cmd.feedrate, MotionConfig::STEPS_PER_MM_Y, MotionConfig::MIN_FEEDRATE_Y,
                                           MotionConfig::MAX_FEEDRATE_Y);
    plan.delayMicrosZ = computeDelayMicros(cmd.feedrate, MotionConfig::STEPS_PER_MM_Z, MotionConfig::MIN_FEEDRATE_Z,
                                           MotionConfig::MAX_FEEDRATE_Z);

    return plan;
}
