#pragma once
#include <vector>
#include <algorithm>
#include <cmath>

enum class TimingMode
{
    VariableDt = 0,
    FixedDt = 1,
    SemiFixedDt = 2
};

enum class IntegratorType
{
    ExplicitEuler = 0,
    SemiImplicitEuler = 1,
    Verlet = 2
};

struct Keyframe1f
{
    float t = 0.0f;
    float v = 0.0f;
};

struct Keyframe3f
{
    float t = 0.0f;
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct MotionState
{
    float x = 0.0f;
    float v = 0.0f;
    float a = 0.0f;

    // needed for Verlet
    float prevX = 0.0f;
    bool verletInitialized = false;
};

struct AnimClip1f
{
    std::vector<Keyframe1f> keys;
    bool loop = true;

    float sample(float time) const;
};

struct AnimClip3f
{
    std::vector<Keyframe3f> keys;
    bool loop = true;

    void sample(float time, float& outX, float& outY, float& outZ) const;
};

struct AnimClock
{
    bool playing = true;
    TimingMode mode = TimingMode::FixedDt;

    float timeScale = 1.0f;

    // animation time
    float simTime = 0.0f;

    // frame dt from real world
    float frameDt = 0.0f;

    // fixed / semi-fixed
    float fixedDt = 1.0f / 60.0f;
    float accumulator = 0.0f;

    // protect against dt spikes
    float dtMax = 0.05f;     // 50 ms
    int maxSubsteps = 8;

    // internal
    float remainingDt = 0.0f;

    void reset();
    void beginFrame(float realDt);
    bool nextStep(float& outDt);
};

float lerp1f(float a, float b, float s);
float expDecayAlpha(float k, float dt);

// robust event check for looping clips
bool crossedEventTime(float tPrev, float tNow, float eventT, float duration, bool loop);

// integration
void integrateMotion(MotionState& s, float dt, IntegratorType type);
void applyExponentialDamping(MotionState& s, float k, float dt);