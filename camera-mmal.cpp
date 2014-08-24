
#include <iostream>
#include <stdexcept>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <opencv2/highgui/highgui.hpp>

extern "C"
{
#include "bcm_host.h"
#include "interface/vcos/vcos.h"

#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_connection.h"

#include "RaspiCamControl.h"
#include "RaspiPreview.h"
#include "RaspiCLI.h"
//#include "RaspiTex.h"
}

#include "camera-mmal.hpp"
#include "config.hpp"

using namespace std;

// Standard port setting for the camera component
#define MMAL_CAMERA_PREVIEW_PORT 0
#define MMAL_CAMERA_VIDEO_PORT 1
#define MMAL_CAMERA_CAPTURE_PORT 2

// Stills format information
// 0 implies variable
#define STILLS_FRAME_RATE_NUM 0
#define STILLS_FRAME_RATE_DEN 1

/// Video render needs at least 2 buffers.
#define VIDEO_OUTPUT_BUFFERS_NUM 3

int mmal_status_to_int(MMAL_STATUS_T status);                           \

static void camera_control_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
   if (buffer->cmd == MMAL_EVENT_PARAMETER_CHANGED)
   {
   }
   else
   {
       //vcos_log_error("Received unexpected camera control callback event, 0x%08x", buffer->cmd);
   }

   mmal_buffer_header_release(buffer);
}

class CameraMMAL::CameraMMALImpl
{
private:
    struct PORT_USERDATA
    {
        VCOS_SEMAPHORE_T complete_semaphore; /// semaphore which is posted when we reach end of frame (indicates end of capture or fault)
        CameraMMALImpl *impl; /// pointer to our state in case required in callback
    };
    
public:
    CameraMMALImpl(const string path, const bool light_enable)
        :
        cam_preview_port_(NULL),
        cam_still_port_(NULL),
        preview_input_port_(NULL),
        encoder_input_port_(NULL),
        encoder_output_port_(NULL),
        preview_connection_(NULL),
        camera_(NULL),
        encoder_(NULL),
        encoder_connection_(NULL),
        pool_(NULL),
        next_frame_ms_(-1),
        output_file_(NULL),
        light_enable_(light_enable)
    {
        bcm_host_init();
        
        //vcos_log_register("lapser", VCOS_LOG_CATEGORY);

        Config config("config.json");
        if(!create_camera(config.get_capture_width(), config.get_capture_height()))
        {
            ostringstream oss;
            oss << "Couldn't create camera!";
            throw runtime_error(oss.str());
        }
        if(raspipreview_create(&preview_parameters_) != MMAL_SUCCESS)
        {
            cout << "Failed to create preview component" << endl;
            #warning destroy camear component
        }
        if(!create_encoder(85))
        {
            throw runtime_error("Couldn't create encoder!");
        }

        cam_preview_port_ = camera_->output[MMAL_CAMERA_PREVIEW_PORT];
        cam_still_port_ = camera_->output[MMAL_CAMERA_CAPTURE_PORT];
        encoder_input_port_ = encoder_->input[0];
        encoder_output_port_ = encoder_->output[0];

        preview_input_port_ = preview_parameters_.preview_component->input[0];
        if(connect_ports(cam_preview_port_, preview_input_port_, &preview_connection_))
        {
            VCOS_STATUS_T vcos_status;
            
            // Now connect the camera to the encoder
            if(!connect_ports(cam_still_port_, encoder_input_port_, &encoder_connection_))
            {
                cout << "Failed to connect camera video port to encoder input" << endl;
                #warning GOTO ERROR
                //goto error;
            }

            // Set up our userdata - this is passed though to the callback where we need the information.
            // Null until we open our filename
            callback_data_.impl = this;
            vcos_status = vcos_semaphore_create(&callback_data_.complete_semaphore, "lapser-sem", 0);

            vcos_assert(vcos_status == VCOS_SUCCESS);
        }
        else
        {
            cout << "Failed to connect camera to preview" << endl;
        }

        ostringstream oss;
        const int ex_fd = open("/sys/class/gpio/export", O_WRONLY);
        if(ex_fd > 0)
        {
            oss << "5";
            write(ex_fd, oss.str().c_str(), oss.str().size());
            close(ex_fd);

            oss.str("");
            oss << "/sys/class/gpio/gpio" << 5 << "/direction";
            const int dir_fd = open(oss.str().c_str(), O_WRONLY);
            oss.str("");
            oss << "out";
            write(dir_fd, oss.str().c_str(), oss.str().size());
            close(dir_fd);

            oss.str("");
            oss << "/sys/class/gpio/gpio" << 5 << "/value";
            const int set_fd = open(oss.str().c_str(), O_WRONLY);
            oss.str("");
            if(light_enable)
            {
                oss << "1";
            }
            else
            {
                oss << "0";
            }
            write(set_fd, oss.str().c_str(), oss.str().size());
            close(set_fd);
            
        }
        else
        {
            cout << "Failed to open /sys/class/gpio/export for camera light control" << endl;
        }
        
        strncpy(filename_, "lapserXXXXXX", sizeof("lapserXXXXXX"));
        mktemp(filename_);
    }

    ~CameraMMALImpl()
    {
        
    }

    bool read_frame(cv::Mat &frame)
    {
        int keep_looping = 1;
        MMAL_STATUS_T status;

        while(keep_looping)
        {
            output_file_ = fopen(filename_, "wb");
            if(!output_file_)
            {
                // Notify user, carry on but discarding encoded output buffers
                //vcos_log_error("%s: Error opening output file: %s\nNo output file will be generated\n", __func__, filename_);
            }

            if(output_file_)
            {
                mmal_port_parameter_set_boolean(encoder_->output[0], MMAL_PARAMETER_EXIF_DISABLE, 1);
                
                // // There is a possibility that shutter needs to be set each loop.
                // status = mmal_port_parameter_set_uint32(camera_->control, MMAL_PARAMETER_SHUTTER_SPEED, camera_parameters_.shutter_speed);
                // if(status != MMAL_SUCCESS)
                // {
                //     vcos_log_error("Unable to set shutter speed");
                // }
                
                // Enable the encoder output port
                encoder_output_port_->userdata = (struct MMAL_PORT_USERDATA_T *)&callback_data_;

                // Enable the encoder output port and tell it its callback function
                status = mmal_port_enable(encoder_output_port_, encoder_buffer_callback);

                // Send all the buffers to the encoder output port
                int num = mmal_queue_length(pool_->queue);

                for (int q = 0; q < num; q++)
                {
                    MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(pool_->queue);

                    if (!buffer)
                    {
                        cout << "unable to get a buffer from pool queue" << endl;
                        //vcos_log_error("Unable to get a required buffer %d from pool queue", q);
                    }
                    
                    if (mmal_port_send_buffer(encoder_output_port_, buffer)!= MMAL_SUCCESS)
                    {
                        cout << "unable to send a buffer to encoder output port" << endl;
                        //vcos_log_error("Unable to send a buffer to encoder output port (%d)", q);
                    }
                }

                if (mmal_port_parameter_set_boolean(cam_still_port_, MMAL_PARAMETER_CAPTURE, 1) != MMAL_SUCCESS)
                {
                    cout << "failed to start capture" << endl;
                    //vcos_log_error("%s: Failed to start capture", __func__);
                }
                else
                {
                    // Wait for capture to complete
                    // For some reason using vcos_semaphore_wait_timeout sometimes returns immediately with bad parameter error
                    // even though it appears to be all correct, so reverting to untimed one until figure out why its erratic
                    vcos_semaphore_wait(&callback_data_.complete_semaphore);
                    keep_looping = 0;
                }

                fclose(output_file_);
                output_file_ = NULL;
                
                // Disable encoder output port
                status = mmal_port_disable(encoder_output_port_);
            }
            else
            {
                cout << "NO OUTPUT FILE" << endl;
            }
        } // end for (frame)

        vcos_semaphore_delete(&callback_data_.complete_semaphore);

        frame = cv::imread(filename_);
        
        return status == MMAL_SUCCESS;
    }

    void cleanup(void)
    {
        cout << "In cleanup" << endl;
        if(output_file_)
        {
            fclose(output_file_);
            output_file_ = NULL;
        }
        cout << "after fclose" << endl;
        unlink(filename_);
        cout << "after unlink: " << filename_ << endl;
    }
    
private:
    bool create_camera(const uint32_t width, const uint32_t height)
    {
        const uint32_t preview_w = 320;
        const uint32_t preview_h = 240;
        MMAL_COMPONENT_T *camera = NULL;
        MMAL_ES_FORMAT_T *format = NULL;
        MMAL_PORT_T *preview_port = NULL, *video_port = NULL, *still_port = NULL;
        MMAL_STATUS_T status;

        /* Create the component */
        status = mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &camera);

        if (status != MMAL_SUCCESS)
        {
            //vcos_log_error("Failed to create camera component");
            goto error;
        }

        if (!camera->output_num)
        {
            status = MMAL_ENOSYS;
            //vcos_log_error("Camera doesn't have output ports");
            goto error;
        }

        preview_port = camera->output[MMAL_CAMERA_PREVIEW_PORT];
        video_port = camera->output[MMAL_CAMERA_VIDEO_PORT];
        still_port = camera->output[MMAL_CAMERA_CAPTURE_PORT];

        // Enable the camera, and tell it its control callback function
        status = mmal_port_enable(camera->control, camera_control_callback);

        if (status != MMAL_SUCCESS)
        {
            //vcos_log_error("Unable to enable control port : error %d", status);
            goto error;
        }

        //  set up the camera configuration
        raspipreview_set_defaults(&preview_parameters_);
        raspicamcontrol_set_defaults(&camera_parameters_);
        {
            MMAL_PARAMETER_CAMERA_CONFIG_T cam_config =
                {
                    { MMAL_PARAMETER_CAMERA_CONFIG, sizeof(cam_config) },
                    width,
                    height,
                    0,
                    1,
                    preview_parameters_.previewWindow.width,
                    preview_parameters_.previewWindow.height,
                    3,
                    0,
                    0,
                    MMAL_PARAM_TIMESTAMP_MODE_RESET_STC
                };

            mmal_port_parameter_set(camera->control, &cam_config.hdr);
        }

        raspicamcontrol_set_all_parameters(camera, &camera_parameters_);
        
        // Now set up the port formats

        format = preview_port->format;
        format->encoding = MMAL_ENCODING_OPAQUE;
        format->encoding_variant = MMAL_ENCODING_I420;

        // Use a full FOV 4:3 mode
        format->es->video.width = VCOS_ALIGN_UP(preview_parameters_.previewWindow.width, 32);
        format->es->video.height = VCOS_ALIGN_UP(preview_parameters_.previewWindow.height, 16);
        format->es->video.crop.x = 0;
        format->es->video.crop.y = 0;
        format->es->video.crop.width = preview_parameters_.previewWindow.width;
        format->es->video.crop.height = preview_parameters_.previewWindow.height;
        format->es->video.frame_rate.num = PREVIEW_FRAME_RATE_NUM;
        format->es->video.frame_rate.den = PREVIEW_FRAME_RATE_DEN;

        status = mmal_port_format_commit(preview_port);
        if (status != MMAL_SUCCESS)
        {
            //vcos_log_error("camera viewfinder format couldn't be set");
            cout << "camera viewfinder format couldn't be set" << endl;
            goto error;
        }

        // Set the same format on the video  port (which we dont use here)
        mmal_format_full_copy(video_port->format, format);
        status = mmal_port_format_commit(video_port);

        if (status  != MMAL_SUCCESS)
        {
            //vcos_log_error("camera video format couldn't be set");
            cout << "camera video format couldn't be set" << endl;
            goto error;
        }

        // Ensure there are enough buffers to avoid dropping frames
        if (video_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
            video_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;

        format = still_port->format;

        // Set our stills format on the stills (for encoder) port
        format->encoding = MMAL_ENCODING_OPAQUE;
        format->es->video.width = VCOS_ALIGN_UP(width, 32);
        format->es->video.height = VCOS_ALIGN_UP(height, 16);
        format->es->video.crop.x = 0;
        format->es->video.crop.y = 0;
        format->es->video.crop.width = width;
        format->es->video.crop.height = height;
        format->es->video.frame_rate.num = STILLS_FRAME_RATE_NUM;
        format->es->video.frame_rate.den = STILLS_FRAME_RATE_DEN;


        status = mmal_port_format_commit(still_port);

        if (status != MMAL_SUCCESS)
        {
            //vcos_log_error("camera still format couldn't be set");
            cout << "camera still format couldn't be set" << endl;
            goto error;
        }

        /* Ensure there are enough buffers to avoid dropping frames */
        if (still_port->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
            still_port->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;

        /* Enable component */
        status = mmal_component_enable(camera);

        if (status != MMAL_SUCCESS)
        {
            //vcos_log_error("camera component couldn't be enabled");
            cout << "camera component couldn't be enabled" << endl;
            goto error;
        }

        camera_ = camera;

        return status == MMAL_SUCCESS;

    error:
        //cout << "THERE WAS A GHETTO GOTO ERROR" << endl;
        
        if (camera)
            mmal_component_destroy(camera);

        return status == MMAL_SUCCESS;
    }

    bool create_encoder(const int quality)
    {
        MMAL_COMPONENT_T *encoder = 0;
        MMAL_PORT_T *encoder_input = NULL, *encoder_output = NULL;
        MMAL_STATUS_T status;
        MMAL_POOL_T *pool;

        status = mmal_component_create(MMAL_COMPONENT_DEFAULT_IMAGE_ENCODER, &encoder);

        if (status != MMAL_SUCCESS)
        {
            cout << "unable to create jpeg encoder compoenent" << endl;
            //vcos_log_error("Unable to create JPEG encoder component");
            goto error;
        }

        if (!encoder->input_num || !encoder->output_num)
        {
            status = MMAL_ENOSYS;
            cout << "jpeg encoder doesn't have ports" << endl;
            //vcos_log_error("JPEG encoder doesn't have input/output ports");
            goto error;
        }

        encoder_input = encoder->input[0];
        encoder_output = encoder->output[0];

        // We want same format on input and output
        mmal_format_copy(encoder_output->format, encoder_input->format);

        // Specify out output format
        encoder_output->format->encoding = MMAL_ENCODING_JPEG;

        encoder_output->buffer_size = encoder_output->buffer_size_recommended;

        if (encoder_output->buffer_size < encoder_output->buffer_size_min)
            encoder_output->buffer_size = encoder_output->buffer_size_min;

        encoder_output->buffer_num = encoder_output->buffer_num_recommended;

        if (encoder_output->buffer_num < encoder_output->buffer_num_min)
            encoder_output->buffer_num = encoder_output->buffer_num_min;

        // Commit the port changes to the output port
        status = mmal_port_format_commit(encoder_output);

        if (status != MMAL_SUCCESS)
        {
            cout << "unable to set format on video encoder output port" << endl;
            //vcos_log_error("Unable to set format on video encoder output port");
            goto error;
        }

        // Set the JPEG quality level
        status = mmal_port_parameter_set_uint32(encoder_output, MMAL_PARAMETER_JPEG_Q_FACTOR, quality);

        if (status != MMAL_SUCCESS)
        {
            cout << "unable to set jpeg quality" << endl;
            //vcos_log_error("Unable to set JPEG quality");
            goto error;
        }

        //  Enable component
        status = mmal_component_enable(encoder);

        if (status  != MMAL_SUCCESS)
        {
            cout << "unable to enable video encoder component" << endl;
            //vcos_log_error("Unable to enable video encoder component");
            goto error;
        }

        /* Create pool of buffer headers for the output port to consume */
        pool = mmal_port_pool_create(encoder_output, encoder_output->buffer_num, encoder_output->buffer_size);

        if (!pool)
        {
            cout << "failed to create buffer header pool for encoder output port" << endl; 
            //vcos_log_error("Failed to create buffer header pool for encoder output port %s", encoder_output->name);
        }

        pool_ = pool;
        encoder_ = encoder;

        return status == MMAL_SUCCESS;

    error:

        if (encoder)
            mmal_component_destroy(encoder);

        return status == MMAL_SUCCESS;
    }

    bool connect_ports(MMAL_PORT_T *out, MMAL_PORT_T *in, MMAL_CONNECTION_T **connection)
    {
        MMAL_STATUS_T status;

        status =  mmal_connection_create(connection, out, in, MMAL_CONNECTION_FLAG_TUNNELLING | MMAL_CONNECTION_FLAG_ALLOCATION_ON_INPUT);
        
        if (status == MMAL_SUCCESS)
        {
            status =  mmal_connection_enable(*connection);
            if (status != MMAL_SUCCESS)
                mmal_connection_destroy(*connection);
        }
        
        return status == MMAL_SUCCESS;
    }
    
    static void encoder_buffer_callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
    {
        int complete = 0;

        // We pass our file handle and other stuff in via the userdata field.

        PORT_USERDATA *pData = (PORT_USERDATA *)port->userdata;

        if(pData)
        {
            uint32_t bytes_written = buffer->length;

            if(buffer->length && pData->impl->output_file_)
            {
                mmal_buffer_header_mem_lock(buffer);

                bytes_written = fwrite(buffer->data, 1, buffer->length, pData->impl->output_file_);

                mmal_buffer_header_mem_unlock(buffer);
            }

            // We need to check we wrote what we wanted - it's possible we have run out of storage.
            if(bytes_written != buffer->length)
            {
                //vcos_log_error("Unable to write buffer to file - aborting");
                complete = 1;
            }

            // Now flag if we have completed
            if (buffer->flags & (MMAL_BUFFER_HEADER_FLAG_FRAME_END | MMAL_BUFFER_HEADER_FLAG_TRANSMISSION_FAILED))
                complete = 1;
        }
        else
        {
            //            vcos_log_error("Received a encoder buffer callback with no state");
        }

        // release buffer back to the pool
        mmal_buffer_header_release(buffer);

        // and send one back to the port (if still open)
        if(port->is_enabled)
        {
            MMAL_STATUS_T status = MMAL_SUCCESS;
            MMAL_BUFFER_HEADER_T *new_buffer;
            
            new_buffer = mmal_queue_get(pData->impl->pool_->queue);
            
            if(new_buffer)
            {
                status = mmal_port_send_buffer(port, new_buffer);
            }
            if(!new_buffer || status != MMAL_SUCCESS)
                ;//vcos_log_error("Unable to return a buffer to the encoder port");
        }
        
        if(complete)
            vcos_semaphore_post(&(pData->complete_semaphore));
    }    
    
    MMAL_PORT_T *cam_preview_port_;
    MMAL_PORT_T *cam_still_port_;
    MMAL_PORT_T *preview_input_port_;
    MMAL_PORT_T *encoder_input_port_;
    MMAL_PORT_T *encoder_output_port_;    
    RASPICAM_CAMERA_PARAMETERS camera_parameters_; /// Camera setup parameters
    RASPIPREVIEW_PARAMETERS preview_parameters_;
    MMAL_CONNECTION_T *preview_connection_;
    MMAL_COMPONENT_T *camera_;    /// Pointer to the camera component
    MMAL_COMPONENT_T *encoder_;
    MMAL_CONNECTION_T *encoder_connection_;
    MMAL_POOL_T *pool_;
    int64_t next_frame_ms_;
    FILE *output_file_;
    char filename_[256];
    
    PORT_USERDATA callback_data_;
    const bool light_enable_;
};

CameraMMAL::CameraMMAL(const string path, const bool light_enable)
    :
    impl_(new CameraMMALImpl(path, light_enable))
{

}

CameraMMAL::~CameraMMAL()
{
    cout << "In camera dtor..." << endl;
    impl_->cleanup();
}

bool CameraMMAL::read_frame(cv::Mat &frame)
{ return impl_->read_frame(frame); }
