
#pragma once

#include <boost/shared_ptr.hpp>
#include <opencv2/core/core.hpp>

#include "icamera.hpp"

class CameraOCV : public ICamera
{
public:
    CameraOCV(const std::string path);
    virtual ~CameraOCV();
    
    // icamera
    bool read_frame(cv::Mat &frame);

private:
    class CameraOCVImpl;
    typedef boost::shared_ptr < CameraOCVImpl > CameraOCVImplPtr;
    CameraOCVImplPtr impl_;
};
