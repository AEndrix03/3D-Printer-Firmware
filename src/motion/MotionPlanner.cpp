#include "../include/motion/MotionPlanner.hpp"
#include "../include/motion/MotionConfig.hpp"
#include <math.h>

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

    plan.delayMicros = 100; // valore temporaneo

    return plan;
}
