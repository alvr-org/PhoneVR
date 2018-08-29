#include "PVRMath.h"

using namespace Eigen;

namespace {
    const float avgDeltaT = 0.0083f;
}

PoseEstimQueue prePEQ, postPEQ;


//ios
//swapped pitch and roll + -90° pitch
// -90° pitch * 90° roll * orient * -90° roll
//pose.qRotation = Mul(Mul({ 0.5, -0.5, 0.5, 0.5 }, { orientBuf[0], orientBuf[1], orientBuf[2], orientBuf[3] }), { 0.70710678118, 0, 0, -0.70710678118 });


// dq = q2 * inv(q1)
// dq/dt = v = Id.slerp(1/dt, dq)
// v = v0 + at
// q = q0 + vt + 1/2 at^2

void PoseEstimQueue::enqueue(const Quaternionf &quat, float deltaT) {
    pos[idx] = quat;
    t[idx] = deltaT / avgDeltaT;
    //spd[idx] = Quaternionf::Identity().slerp(1.f / t[idx], pos[mod3(idx - 1)].inverse() * quat);
    idx = mod3(idx + 1);
}

Quaternionf	PoseEstimQueue::getQuatIn(float deltaT) {
    return pos[idx] * (pos[mod3(idx - 1)].inverse() * pos[idx]).slerp(deltaT / avgDeltaT / 2 / t[idx], Quaternionf::Identity());
    //return pos[idx] * Quaternionf::Identity().slerp(deltaT / avgDeltaT / 2 / t[idx], (pos[mod3(idx - 1)].inverse() * pos[idx]).inverse());
    // * Quaternionf::Identity().slerp(deltaT * deltaT / avgDeltaT / avgDeltaT / 2 / t[idx], spd[mod3(idx - 1)].inverse() * spd[idx]);
}

Quaternionf PVRMat34ToQuat(float(*mat)[3][4]) {
    Matrix3f emat;
    for (size_t x = 0; x < 3; x++)
        for (size_t y = 0; y < 3; y++)
            emat(x, y) = (*mat)[x][y];
    return Quaternionf(emat);
}