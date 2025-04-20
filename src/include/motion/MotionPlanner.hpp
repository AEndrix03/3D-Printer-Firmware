struct MotionCommand {
    float x, y, z, feedrate;
};

struct MotionPlan {
    int stepsX, stepsY, stepsZ;
    bool dirX, dirY, dirZ;
    unsigned int delayMicros;
};

MotionPlan planMotion(const MotionCommand &cmd);
