#ifndef pose_estimation_h
#define pose_estimation_h

#include <Eigen/Eigen>
#include <cmath>

extern "C"
{
#include <apriltag/apriltag.h>
#include <apriltag/apriltag_pose.h>
#include <apriltag/tag16h5.h>
}

#include "Cameras.h"

#define M_TWOPI 2 * M_PI

#define CAM_CX 323
#define CAM_CY 245
#define CAM_FX 608
#define CAM_FY 608

#define TAG_SIZE 0.1524
#define INCH 39.37

typedef struct robot_position
{
    double x;
    double y;
    double z;
    double theta; // in radians
    int processing_time; // in microseconds
} robot_position;

void getRobotPosition(apriltag_detection_t *det, robot_position *pos, camInfo *cam);

#endif
