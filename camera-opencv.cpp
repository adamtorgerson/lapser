
#include <opencv2/highgui/highgui.hpp>

#include "camera-opencv.hpp"

class CameraOCV::CameraOCVImpl
{
public:
    CameraOCVImpl(const std::string path)
        :
        reader_(path)
    {

    }

    ~CameraOCVImpl()
    {

    }

    bool read_frame(cv::Mat &frame)
    {
        return reader_.read(frame);
    }
    
private:
    cv::VideoCapture reader_;
};

CameraOCV::CameraOCV(const std::string path)
    :
    impl_(new CameraOCVImpl(path))
{

}

CameraOCV::~CameraOCV()
{

}

bool CameraOCV::read_frame(cv::Mat &frame)
{ return impl_->read_frame(frame); }
