
#pragma once

#include <opencv2/core/core.hpp>

class ICamera
{
public:
    virtual ~ICamera() { };

    virtual bool read_frame(cv::Mat &frame) = 0;
};
