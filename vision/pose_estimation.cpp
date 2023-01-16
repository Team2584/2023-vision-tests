#include "pose_estimation.h"
#include "main.h"

using namespace std;

//----------- From AprilNav -----------------------------
// Normalize angle to be within the interval [-pi,pi].
inline double standardRad(double t)
{
    if (t >= 0.)
    {
        t = fmod(t + M_PI, M_TWOPI) - M_PI;
    }
    else
    {
        t = fmod(t - M_PI, -M_TWOPI) + M_PI;
    }
    return t;
}

// Convert rotation matrix to Euler angles
void wRo_to_euler(const Eigen::Matrix3d &wRo, double &yaw, double &pitch, double &roll)
{
    yaw = standardRad(atan2(wRo(1, 0), wRo(0, 0)));
    double c = cos(yaw);
    double s = sin(yaw);
    pitch = standardRad(atan2(-wRo(2, 0), wRo(0, 0) * c + wRo(1, 0) * s));
    roll = standardRad(atan2(wRo(0, 2) * s - wRo(1, 2) * c, -wRo(0, 1) * s + wRo(1, 1) * c));
}

Eigen::Matrix4d getRelativeTransform(apriltag_detection_t *det, double tag_size, double fx,
                                     double fy, double px, double py)
{
    std::vector<cv::Point3f> objPts;
    std::vector<cv::Point2f> imgPts;
    double s = tag_size / 2.;
    objPts.push_back(cv::Point3f(-s, -s, 0));
    objPts.push_back(cv::Point3f(s, -s, 0));
    objPts.push_back(cv::Point3f(s, s, 0));
    objPts.push_back(cv::Point3f(-s, s, 0));

    std::pair<float, float> p1(det->p[0][0], det->p[0][1]);
    std::pair<float, float> p2(det->p[1][0], det->p[1][1]);
    std::pair<float, float> p3(det->p[2][0], det->p[2][1]);
    std::pair<float, float> p4(det->p[3][0], det->p[3][1]);
    imgPts.push_back(cv::Point2f(p1.first, p1.second));
    imgPts.push_back(cv::Point2f(p2.first, p2.second));
    imgPts.push_back(cv::Point2f(p3.first, p3.second));
    imgPts.push_back(cv::Point2f(p4.first, p4.second));

    cv::Mat rvec, tvec;
    cv::Matx33d cameraMatrix(fx, 0, px, 0, fy, py, 0, 0, 1);
    cv::Vec<float, 5> distParam(0.09116903370720442, 0.2567349843314421, -0.003936586357063021,
                                0.001658039412119442, -1.633408316803933);
    cv::solvePnP(objPts, imgPts, cameraMatrix, distParam, rvec, tvec);
    cv::Matx33d r;
    cv::Rodrigues(rvec, r);
    Eigen::Matrix3d wRo;
    wRo << r(0, 0), r(0, 1), r(0, 2), r(1, 0), r(1, 1), r(1, 2), r(2, 0), r(2, 1), r(2, 2);

    Eigen::Matrix4d T;
    T.topLeftCorner(3, 3) = wRo;
    T.col(3).head(3) << tvec.at<double>(0), tvec.at<double>(1), tvec.at<double>(2);
    T.row(3) << 0, 0, 0, 1;

    return T;
}

void getRelativeTranslationRotation(apriltag_detection_t *det, double tag_size, double fx,
                                    double fy, double px, double py, Eigen::Vector3d &trans,
                                    Eigen::Matrix3d &rot)
{
    Eigen::Matrix4d T = getRelativeTransform(det, tag_size, fx, fy, px, py);

    // converting from camera frame (z forward, x right, y down) to
    // object frame (x forward, y left, z up)
    Eigen::Matrix4d M;
    M << 0, 0, 1, 0, -1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 0, 1;
    Eigen::Matrix4d MT = M * T;
    // translation vector from camera to the April tag
    trans = MT.col(3).head(3);
    // orientation of April tag with respect to camera: the camera
    // convention makes more sense here, because yaw,pitch,roll then
    // naturally agree with the orientation of the object
    rot = T.block(0, 0, 3, 3);
}

// ------------------ NOT from AprilNav ---------------------------------
Eigen::Vector3d getRealTranslationRotation(Eigen::Vector3d pt, Eigen::Matrix3d rot)
{
    // cout << "Point:" << pt << endl;
    // cout << "Rotation Matrix:\n" << rot << endl;
    Eigen::Vector3d newpt = -rot.transpose() * pt;

    // cout << "Rotated Point:\n" << newpt << endl;
    return newpt;
}

Eigen::Matrix3d rotation_from_euler(double roll, double pitch, double yaw)
{
    // roll and pitch and yaw in radians
    double su = sin(roll);
    double cu = cos(roll);
    double sv = sin(pitch);
    double cv = cos(pitch);
    double sw = sin(yaw);
    double cw = cos(yaw);
    Eigen::Matrix3d Rot_matrix(3, 3);
    Rot_matrix(0, 0) = cv * cw;
    Rot_matrix(0, 1) = su * sv * cw - cu * sw;
    Rot_matrix(0, 2) = su * sw + cu * sv * cw;
    Rot_matrix(1, 0) = cv * sw;
    Rot_matrix(1, 1) = cu * cw + su * sv * sw;
    Rot_matrix(1, 2) = cu * sv * sw - su * cw;
    Rot_matrix(2, 0) = -sv;
    Rot_matrix(2, 1) = su * cv;
    Rot_matrix(2, 2) = cu * cv;
    return Rot_matrix;
}

double aprilGetPose(apriltag_detection_t *det, apriltag_pose_t *pose)
{
}

void getRobotPosition(apriltag_detection_t *det, robot_position *pos)
{
    Eigen::Vector3d tag_trans;
    Eigen::Matrix3d tag_rot;
    Eigen::Matrix3d RZ;
    Eigen::Matrix3d RX;
    double rotX;
    double rotY;
    double rotZ;

    getRelativeTranslationRotation(det, TAG_SIZE, CAM_FX, CAM_FY, CAM_CX, CAM_CY, tag_trans,
                                   tag_rot);
    wRo_to_euler(tag_rot, rotY, rotZ, rotX);
    double newRotx = rotX * -1;
    double newRoty = rotY;
    double newRotz = rotZ * -1;
    Eigen::Matrix3d rotationMatrix = rotation_from_euler(newRoty, newRotx, newRotz);
    // cout << "Translation\n " << tag_trans * 39.37008 << endl;
    // cout << "Rotation\n " << tag_rot << endl;
    // printf("Better Rotation\n %f\n %f\n %f\n", rotX, rotY, rotZ);

    // printf("Pre Rotated: \n x: %f\n h: %f\n z: %f\n", linX, linY, linZ);

    // Tags can only be read upside-down if rotZ isn't flipped
    Eigen::Vector3d point = getRealTranslationRotation(tag_trans, rotationMatrix);

    // printf("Rotated Point\n x: %f\n y: %f\n\n", point(0), point(1));
    pos->x = -point(0);
    pos->y = point(1);
    pos->z = point(2);
    pos->theta = rotZ;
}
