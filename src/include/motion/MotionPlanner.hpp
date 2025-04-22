struct MotionCommand {
    float x, y, z, feedrate;
};

struct MotionPlan {
    int stepsX, stepsY, stepsZ;
    bool dirX, dirY, dirZ;
    unsigned int delayMicrosX, delayMicrosY, delayMicrosZ;
};

unsigned int computeDelayMicros(float feedrate, float stepsPerMm, float minFeedrate, float maxFeedrate);

MotionPlan planMotion(const MotionCommand &cmd);
