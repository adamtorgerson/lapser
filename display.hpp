
#pragma once

#include <boost/shared_ptr.hpp>
#include <opencv2/core/core.hpp>

#include "config.hpp"

class Display
{
public:
    Display(Config &config);
    ~Display();

    void put_string(const std::string &str, const int x = 0, const int y = 0);
    void put_string(cv::Mat &frame, const std::string &str, const int x = 0, const int y = 0);
    void put_frame(cv::Mat &frame);
    void put_button(const std::string &text, const SDL_Rect &r);

    bool draw();

private:
    class DisplayImpl;
    typedef boost::shared_ptr < DisplayImpl > DisplayImplPtr;
    DisplayImplPtr impl_;
};
