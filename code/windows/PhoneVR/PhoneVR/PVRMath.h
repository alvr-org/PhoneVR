#pragma once
#include "PVRGlobals.h"

#include "Eigen"

class PoseEstimQueue {
	Eigen::Quaternionf pos[3];
	float t[3];
	int idx = 0;
public:
	void enqueue(const Eigen::Quaternionf &quat, float deltaT);
	Eigen::Quaternionf getQuatIn(float deltaT);
};

extern PoseEstimQueue prePEQ, postPEQ;

Eigen::Quaternionf PVRMat34ToQuat(float(*mat)[3][4]);


inline bool isValidOrient(Eigen::Quaternionf &quat) {
	float lenSqrd = quat.w() * quat.w() + quat.x() * quat.x() + quat.y() * quat.y() + quat.z() * quat.z();
	return lenSqrd > 0.9f && lenSqrd < 1.1f;
}