#include "main.h"
#include "Cameras.h"
#include "detection.h"
#include "graphics_helpers.h"
#include "pose_estimation.h"

using namespace std;
using namespace cv;

int total_hamm_hist[HAMM_HIST_MAX];
int hamm_hist[HAMM_HIST_MAX];

int main()
{
    flirCamera flir(0);
    depthCamera depth_blue(DEPTH_BLUE, 640, 480, 60);
    depthCamera depth_red(DEPTH_RED, 640, 480, 60);

    /**********************************************************************************************
     * AprilTags Setup *
     *******************/

    // Initialize tag detector with options
    apriltag_family_t *tf = NULL;
    tf = tag16h5_create();

    apriltag_detector_t *td = apriltag_detector_create();
    apriltag_detector_add_family_bits(td, tf, HAMMING_NUMBER);

    td->quad_decimate = QUAD_DECIMATE;
    td->quad_sigma = QUAD_SIGMA;
    td->nthreads = NTHREADS;
    td->debug = APRIL_DEBUG;
    td->refine_edges = REFINE_EDGES;

    memset(total_hamm_hist, 0, sizeof(int) * HAMM_HIST_MAX);

    /**********************************************************************************************
     * Network Tables Setup *
     ************************/

    // Create networktables instan8ce and a table for vision
    nt::NetworkTableInstance nt_inst = nt::NetworkTableInstance::GetDefault();

    // Setup networktable client
    nt_inst.StartClient4("jetson client");
    nt_inst.SetServerTeam(2584);
    nt_inst.StartDSClient();
    nt_inst.SetServer("host", NT_DEFAULT_PORT4);

    // Create tables
    shared_ptr<nt::NetworkTable> visionTbl = nt_inst.GetTable("vision");
    shared_ptr<nt::NetworkTable> localTbl = nt_inst.GetTable("vision/localization");

    // Make a sanity check topic and an entry to publish/read from it; set initial
    // value
    nt::IntegerTopic sanitycheck = visionTbl->GetIntegerTopic("sanitycheck");
    nt::IntegerEntry sanitycheckEntry = sanitycheck.GetEntry(0, {.periodic = 0.01});
    sanitycheckEntry.Set(1);

    // Other vision topics
    nt::BooleanTopic robot_pos_goodTopic = localTbl->GetBooleanTopic("robot_pos_good");
    nt::RawTopic robot_pose_1Topic = localTbl->GetRawTopic("pose1");

    nt::BooleanEntry robot_pos_goodEntry = robot_pos_goodTopic.GetEntry(false, {.periodic = 0.01});
    nt::RawEntry robot_pose_1Entry =
        robot_pose_1Topic.GetEntry("robot_position", {}, {.periodic = 0.01});

    /**********************************************************************************************
     * THE LOOP *
     ************/
    int counter = 2;

    while (true)
    {
        errno = 0;
        memset(hamm_hist, 0, sizeof(hamm_hist));

        // Make sure networktables is working
        sanitycheckEntry.Set(counter);
        counter++;

        flir.getFrame();
        depth_blue.getFrame();
        depth_red.getFrame();

        if (errno == EAGAIN)
        {
            printf("Unable to create the %d threads requested.\n", td->nthreads);
            exit(-1);
        }

        robot_pos_goodEntry.Set(false);

        zarray_t *poses = zarray_create(sizeof(robot_position *));

        getPoses(flir.grayFrame, flir.colorFrame, &flir.info, td, poses);
        getPoses(depth_blue.grayFrame, depth_blue.colorFrame, &depth_blue.info, td, poses);
        getPoses(depth_red.grayFrame, depth_red.colorFrame, &depth_red.info, td, poses);

        for (int i = 0; i < zarray_size(poses); i++)
        {
            robot_position *pos;
            zarray_get(poses, i, &pos);
        }

        drawMargins(flir.colorFrame);
        drawMargins(depth_blue.colorFrame);
        drawMargins(depth_red.colorFrame);

        imshow("flir", flir.colorFrame);
        imshow("depth_blue", depth_blue.colorFrame);
        imshow("depth_red", depth_red.colorFrame);


        if (waitKey(1) == 'q')
            break;

        // nt_inst.Flush();
    }

    apriltag_detector_destroy(td);
    tag16h5_destroy(tf);
    destroyAllWindows();

    return 0;
}
