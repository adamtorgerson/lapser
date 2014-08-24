
#include <signal.h>
#include <iomanip>
#include <boost/program_options.hpp>
#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/date_time.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include "SDL/SDL.h"

#include "config.hpp"
#include "display.hpp"
#include "camera-opencv.hpp"
#include "camera-mmal.hpp"
#include "input.hpp"

namespace po = boost::program_options;
namespace pt = boost::posix_time;
using namespace std;

//bool lapser_verbose = false;
#ifdef ARM_BUILD
static string camera_type = "mmal";
#else
static string camera_type = "opencv";
#endif
static string fb_device = "/dev/fb1";
static string config_filename = "config.json";
static string fourcc = "XVID";

static boost::recursive_mutex capture_mutex;
static cv::Mat shared_frame;
static bool shared_frame_updated;
static pt::ptime goal;
static pt::ptime last_shot;
static cv::VideoWriter *video_writer;
static ICamera *camera;

boost::posix_time::time_facet *facet;

static void signal_handler(int signum)
{
    if(signum != SIGUSR1)
    {
        //cout << "Exiting due to signal." << endl;
        cout << "Got signal" << endl;
        SDL_Quit();
        if(camera)
        {
            cout << "Deleting camera" << endl;
            delete camera;
        }
        if(video_writer)
        {
            cout << "Deleting video writer" << endl;
            delete video_writer;
        }
        exit(0);
    }
}

static int parse_arguments(int argc, char *argv[])
{
    bool args_valid = true; 

    po::options_description visible_options("Usage: lapser \n\nOptions are: ");
    visible_options.add_options()
        ("help", "Print this message.")
        //("verbose,v", "Print more information." )
        ("camera-type,c", po::value < string >(&camera_type), "Camera Type [ mmal, opencv ]")
        ("framebuffer-device,f", po::value < string >(&fb_device))
        ("config", po::value < string >(&config_filename), "Config file [ config.json ]")
        ("fourcc", po::value < string > (&fourcc), "Output file FOURCC code [ XVID ]")
        ;
    
    try
    {
        po::variables_map vm;        
        po::store(po::command_line_parser(argc, argv).options(visible_options).run(), vm);
        po::notify(vm);    

        if(vm.count("help")) 
        {
            cerr << visible_options << endl;
            return 1;
        }

        /*if(vm.count("verbose"))
        {
            lapser_verbose = true; 
            }*/

#ifdef ARM_BUILD
        if(camera_type != "mmal" && camera_type != "opencv")
        {
            cout << "Invalid camera type: " << camera_type << endl;
            args_valid = false;
        }
#else
        if(camera_type != "opencv")
        {
            cout << "Invalid camera type: " << camera_type << endl;
            args_valid = false;
        }
#endif

        if(fourcc.size() != 4)
        {
            cout << "Invalid FOURCC code (must be 4 characters)" << endl;
            args_valid = false;
        }

#ifdef ARM_BUILD
        if(fb_device != "/dev/fb0")
        {
            setenv("SDL_FBDEV", fb_device.c_str(), 1);
        }
        setenv("SDL_MOUSEDRV", "TSLIB", 1);
        setenv("SDL_MOUSEDEV", "/dev/input/touchscreen", 1);
#endif
        //setenv("SDL_NOMOUSE", "1", 1);
    }
    catch(const exception &e)
    {
        cerr << "Error " << e.what() << endl;
        args_valid = false; 
    }
    
    if(!args_valid)
    {
        cout << endl << visible_options << endl;
        return 2; 
    }

    return 0;
}

static void capture_thread(Config &config, Display &display)
{
    try
    {
        const int interval = config.get_interval();
        //cout << "interval: " << interval << endl;
    
        cv::Mat frame(config.get_capture_height(), config.get_capture_width(), CV_8UC3);
        if(camera_type == "opencv")
        {
            camera = new CameraOCV(config.get_capture_name());
        }
#ifdef ARM_BUILD
        else if(camera_type == "mmal")
        {
            camera = new CameraMMAL(config.get_capture_name(), config.get_light_enable());
        }
#endif

        if(!camera)
        {
            cout << "Couldn't create camera of type: " << camera_type << " (was it compiled in?)" << endl;
            SDL_Quit();
            exit(-1);
        }
        
        pt::ptime last = pt::microsec_clock::local_time();
        goal = last;
        last_shot = last;
    
        while(1)
        {
            const pt::ptime current = pt::microsec_clock::local_time();
            
            if(current >= goal)
            {
                cout << "Taking picture!" << endl;
                
                if(!camera->read_frame(frame))
                {
                    //cout << "Capture ended" << endl;
                    break;
                }
                const pt::ptime post_pic = pt::microsec_clock::local_time();
                cout << "Picture took: " << post_pic - current << endl;

                if(current - goal < pt::microseconds(50000))
                {
                    // more precise if we only update based on goal
                    goal = goal + pt::microseconds(interval * 1000);
                }
                else
                {
                    cout << "Resetting goal..." << endl;
                    goal = current + pt::microseconds(interval * 1000);
                }

                cv::Size desired(config.get_output_width(), config.get_output_height());
                if(!video_writer)
                {
                    //cout << "Creating new VideoWriter" << endl;
                    video_writer = new cv::VideoWriter(config.get_output_name(), CV_FOURCC(fourcc[0], fourcc[1], fourcc[2], fourcc[3]), 30,
                                                       desired);
                }

                if(desired != frame.size())
                {
                    cv::Mat vid_frame;
                    cv::resize(frame, vid_frame, desired);
                    *video_writer << vid_frame;
                }
                else
                    *video_writer << frame;
                
                {
                    boost::recursive_mutex::scoped_lock lock(capture_mutex);
                    if(frame.type() != shared_frame.type() || frame.size() != shared_frame.size())
                    {
                        shared_frame = cv::Mat(frame.rows, frame.cols, frame.type());
                    }
                    frame.copyTo(shared_frame);
                    shared_frame_updated = true;
                }
            
                last_shot = current;
            }            

            boost::this_thread::sleep(pt::microseconds(33333));
            last = current;
        }
    
        delete camera;
    }
    catch(const exception &e)
    {
        cout << "Capture thread got exception: " << e.what() << endl;
        SDL_Quit();
        exit(-1);
    }
}

int main(int argc, char *argv[])
{
    facet = new boost::posix_time::time_facet("%Y-%b-%d %k:%M:%S");
    cout.imbue(locale(cout.getloc(), facet));
    
    if(parse_arguments(argc, argv) != 0)
    {
        return -1;
    }

    signal(SIGINT, signal_handler);

    bool enabled = true;
    try
    {
        Config config(config_filename);

        Display display(config);
        Input input(display);
        
        //r.x = 200;
        //r.y = 200;
        //input.add_region(Action::DELETE_LAST, r);
        
        cv::Mat frame(config.get_capture_height(), config.get_capture_width(), CV_8UC3);
        boost::thread cap_thread(&capture_thread, config, display);
        bool running = true;
        while(running)
        {
            const pt::ptime current = pt::microsec_clock::local_time();
            
            if(shared_frame_updated)
            {
                boost::recursive_mutex::scoped_lock lock(capture_mutex);
                if(frame.type() != shared_frame.type() || frame.size() != shared_frame.size())
                {
                    frame = cv::Mat(shared_frame.rows, shared_frame.cols, shared_frame.type());
                }
                shared_frame.copyTo(frame);
                shared_frame_updated = false;
            }
            display.put_frame(frame);

            SDL_Rect r;
            r.x = 200;
            r.y = 0;
            r.w = 120;
            r.h = 40;
            if(enabled)
            {
                input.add_region(Action::DISABLE, r);
                
                ostringstream oss;
                oss << "Next shot in: ";
                const pt::time_duration remaining = goal - current;
                oss << remaining.seconds() << "." << remaining.fractional_seconds() / (int)pow(10, remaining.num_fractional_digits() - 1);
                display.put_string(oss.str(), 0, -1);
            }
            else
            {
                input.add_region(Action::ENABLE, r);
                
                display.put_string("Time lapse disabled", 0, -1);
            }

            // display current time
            ostringstream oss;
            oss.imbue(locale(cout.getloc(), facet));
            oss << last_shot << "." << last_shot.time_of_day().fractional_seconds() / (int)pow(10, last_shot.time_of_day().num_fractional_digits() - 1);
            display.put_string(oss.str());
            
            Action::Action a = input.process();
            switch(a)
            {
            case Action::QUIT:
                running = false;
                break;
            case Action::ENABLE:
                cout << "Got enable" << endl;
                enabled = true;
                input.remove_region(Action::ENABLE);
                break;
            case Action::DISABLE:
                cout << "Got disable" << endl;
                enabled = false;
                input.remove_region(Action::DISABLE);
                break;
            case Action::DELETE_LAST:
                cout << "Got delete" << endl;
                break;
            case Action::CONTINUE:
                break;
            }

            input.draw_regions();
            if(!display.draw())
                running = false;
            
            boost::this_thread::sleep(pt::microseconds(33333));
        }
    }
    catch(const exception &e)
    {
        cerr << "Exception: " << e.what() << endl;
        SDL_Quit();
        exit(-1);
    }

    SDL_Quit();
    
    return 0;
}
