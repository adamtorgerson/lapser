
#pragma once

#include <boost/shared_ptr.hpp>


#include "icamera.hpp"

class CameraMMAL : public ICamera
{
public:
    CameraMMAL(const std::string path, const bool light_enable);
    virtual ~CameraMMAL();
    
    // icamera
    bool read_frame(cv::Mat &frame);
    
private:
    class CameraMMALImpl;
    typedef boost::shared_ptr < CameraMMALImpl > CameraMMALImplPtr;
    CameraMMALImplPtr impl_;
};
