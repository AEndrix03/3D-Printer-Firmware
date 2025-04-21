struct MotionCommand {
    float x, y, z, feedrate;
};

struct MotionPlan {
    int stepsX, stepsY, stepsZ;
    bool dirX, dirY, dirZ;
    unsigned int delayMicrosX, delayMicrosY, delayMicrosZ;
};

unsigned int computeDelayMicros(float feedrate, float minRate, float maxRate);

MotionPlan planMotion(const MotionCommand &cmd);
