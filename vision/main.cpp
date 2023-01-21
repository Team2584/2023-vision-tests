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
    // flirCamera flir(0);
    depthCamera depth_blue(DEPTH_RED, 640, 480, 60);
    // depthCamera depth_red(DEPTH_RED, 640, 480, 60);

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
    nt::IntegerTopic sanitycheck = localTbl->GetIntegerTopic("sanitycheck");
    nt::IntegerEntry sanitycheckEntry = sanitycheck.GetEntry(0, {.periodic = 0.01});
    sanitycheckEntry.Set(1);

    // Other vision topics
    nt::BooleanTopic robot_pos_goodTopic = localTbl->GetBooleanTopic("robot_pos_good");
    nt::DoubleArrayTopic robot_pose_Topic = localTbl->GetDoubleArrayTopic("poseArray");
    nt::BooleanEntry robot_pos_goodEntry = robot_pos_goodTopic.GetEntry(false, {.periodic = 0.01});
    nt::DoubleArrayEntry robot_pose_Entry = robot_pose_Topic.GetEntry({}, {.periodic = 0.01});

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

        // flir.getFrame();
        depth_blue.getFrame();
        chrono::time_point start = chrono::steady_clock::now();
        // depth_red.getFrame();

        if (errno == EAGAIN)
        {
            printf("Unable to create the %d threads requested.\n", td->nthreads);
            exit(-1);
        }

        robot_pos_goodEntry.Set(false);

        std::vector<robot_position> poses =
            getPoses(depth_blue.grayFrame, depth_blue.colorFrame, &depth_blue.info, td);

        for (unsigned int i = 0; i < poses.size(); i++)
        {
            robot_position pos = poses[i];
            cout << pos.x << endl;
            cout << pos.y << endl;
            cout << pos.z << endl;
            chrono::time_point end = chrono::steady_clock::now();
            auto duration = chrono::duration_cast<chrono::microseconds>(end - start);
            int ms = duration.count();
            std::vector<double> entryVector = {pos.x, pos.y, pos.z, pos.theta, ms};
            robot_pose_Entry.Set(entryVector);
            nt_inst.Flush();
        }

        // drawMargins(flir.colorFrame);
        drawMargins(depth_blue.colorFrame);
        // drawMargins(depth_red.colorFrame);

        // imshow("flir", flir.colorFrame);
        // imshow("depth_blue", depth_blue.colorFrame);
        // imshow("depth_red", depth_red.colorFrame);

        if (waitKey(1) == 'q')
            break;

        // nt_inst.Flush();
    }

    apriltag_detector_destroy(td);
    tag16h5_destroy(tf);
    destroyAllWindows();

    return 0;
}
