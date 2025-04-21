#include "../include/motion/MotionPlanner.hpp"
#include "../include/motion/MotionConfig.hpp"
#include <math.h>

unsigned int computeDelayMicros(float feedrate, float minRate, float maxRate) {
    float factor = feedrate / 10000.0f;
    if (factor > 1.0f) factor = 1.0f;
    if (factor < 0.0f) factor = 0.0f;

    float speed = minRate + factor * (maxRate - minRate);
    float delay_s = 1.0f / (speed / 60.0f); // mm/min → mm/s → sec per passo
    return (unsigned int) (delay_s * 1e6f);
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

    plan.delayMicrosX = computeDelayMicros(cmd.feedrate, MotionConfig::MIN_FEEDRATE_X, MotionConfig::MAX_FEEDRATE_X);
    plan.delayMicrosY = computeDelayMicros(cmd.feedrate, MotionConfig::MIN_FEEDRATE_Y, MotionConfig::MAX_FEEDRATE_Y);
    plan.delayMicrosZ = computeDelayMicros(cmd.feedrate, MotionConfig::MIN_FEEDRATE_Z, MotionConfig::MAX_FEEDRATE_Z);

    return plan;
}
