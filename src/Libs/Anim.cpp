#include "Anim.h"

static float wrapTime(float t, float duration)
{
    if (duration <= 0.0f) return 0.0f;
    t = std::fmod(t, duration);
    if (t < 0.0f) t += duration;
    return t;
}

float lerp1f(float a, float b, float s)
{
    return a + (b - a) * s;
}

float expDecayAlpha(float k, float dt)
{
    return 1.0f - std::exp(-k * dt);
}

float AnimClip1f::sample(float time) const
{
    if (keys.empty()) return 0.0f;
    if (keys.size() == 1) return keys[0].v;

    float duration = keys.back().t;
    float tt = loop ? wrapTime(time, duration) : std::min(time, duration);

    int i1 = 1;
    while (i1 < (int)keys.size() && keys[i1].t < tt) i1++;

    if (i1 >= (int)keys.size()) return keys.back().v;

    int i0 = i1 - 1;
    const auto& a = keys[i0];
    const auto& b = keys[i1];

    float span = b.t - a.t;
    float s = (span <= 1e-6f) ? 0.0f : (tt - a.t) / span;

    return lerp1f(a.v, b.v, s);
}

void AnimClip3f::sample(float time, float& outX, float& outY, float& outZ) const
{
    if (keys.empty())
    {
        outX = outY = outZ = 0.0f;
        return;
    }

    if (keys.size() == 1)
    {
        outX = keys[0].x;
        outY = keys[0].y;
        outZ = keys[0].z;
        return;
    }

    float duration = keys.back().t;
    float tt = loop ? wrapTime(time, duration) : std::min(time, duration);

    int i1 = 1;
    while (i1 < (int)keys.size() && keys[i1].t < tt) i1++;

    if (i1 >= (int)keys.size())
    {
        outX = keys.back().x;
        outY = keys.back().y;
        outZ = keys.back().z;
        return;
    }

    int i0 = i1 - 1;
    const auto& a = keys[i0];
    const auto& b = keys[i1];

    float span = b.t - a.t;
    float s = (span <= 1e-6f) ? 0.0f : (tt - a.t) / span;

    outX = lerp1f(a.x, b.x, s);
    outY = lerp1f(a.y, b.y, s);
    outZ = lerp1f(a.z, b.z, s);
}

void AnimClock::reset()
{
    simTime = 0.0f;
    frameDt = 0.0f;
    accumulator = 0.0f;
    remainingDt = 0.0f;
}

void AnimClock::beginFrame(float realDt)
{
    if (!playing)
    {
        frameDt = 0.0f;
        remainingDt = 0.0f;
        return;
    }

    frameDt = std::min(realDt * timeScale, dtMax);

    switch (mode)
    {
    case TimingMode::VariableDt:
        remainingDt = frameDt;
        break;

    case TimingMode::FixedDt:
        accumulator += frameDt;
        remainingDt = 0.0f;
        break;

    case TimingMode::SemiFixedDt:
        remainingDt = frameDt;
        break;
    }
}

bool AnimClock::nextStep(float& outDt)
{
    outDt = 0.0f;

    switch (mode)
    {
    case TimingMode::VariableDt:
        if (remainingDt <= 0.0f) return false;
        outDt = remainingDt;
        simTime += outDt;
        remainingDt = 0.0f;
        return true;

    case TimingMode::FixedDt:
    {
        static int stepsThisFrame = 0;
        if (accumulator < fixedDt)
        {
            stepsThisFrame = 0;
            return false;
        }
        if (stepsThisFrame >= maxSubsteps)
        {
            accumulator = 0.0f;
            stepsThisFrame = 0;
            return false;
        }

        outDt = fixedDt;
        accumulator -= fixedDt;
        simTime += outDt;
        stepsThisFrame++;
        return true;
    }

    case TimingMode::SemiFixedDt:
    {
        static int stepsThisFrame = 0;
        if (remainingDt <= 0.0f)
        {
            stepsThisFrame = 0;
            return false;
        }
        if (stepsThisFrame >= maxSubsteps)
        {
            remainingDt = 0.0f;
            stepsThisFrame = 0;
            return false;
        }

        outDt = std::min(remainingDt, fixedDt);
        remainingDt -= outDt;
        simTime += outDt;
        stepsThisFrame++;
        return true;
    }
    }

    return false;
}

bool crossedEventTime(float tPrev, float tNow, float eventT, float duration, bool loop)
{
    if (!loop)
        return (tPrev < eventT && eventT <= tNow);

    float a = wrapTime(tPrev, duration);
    float b = wrapTime(tNow, duration);

    if (a <= b)
        return (a < eventT && eventT <= b);

    // wrapped around
    return (eventT > a || eventT <= b);
}

void integrateMotion(MotionState& s, float dt, IntegratorType type)
{
    switch (type)
    {
    case IntegratorType::ExplicitEuler:
        s.x += s.v * dt;
        s.v += s.a * dt;
        break;

    case IntegratorType::SemiImplicitEuler:
        s.v += s.a * dt;
        s.x += s.v * dt;
        break;

    case IntegratorType::Verlet:
        if (!s.verletInitialized)
        {
            s.prevX = s.x - s.v * dt;
            s.verletInitialized = true;
        }
        {
            float nextX = 2.0f * s.x - s.prevX + s.a * dt * dt;
            s.v = (nextX - s.prevX) / (2.0f * dt); // approximate velocity for debug
            s.prevX = s.x;
            s.x = nextX;
        }
        break;
    }
}

void applyExponentialDamping(MotionState& s, float k, float dt)
{
    s.v *= std::exp(-k * dt);
}