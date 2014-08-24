
#pragma once

#include <boost/shared_ptr.hpp>

class Config
{
public:
    Config(const std::string &path);
    ~Config();

    int get_interval();
    std::string get_capture_name();
    std::string get_output_name();
    std::string get_font_name();
    int get_capture_width();
    int get_capture_height();
    int get_display_width();
    int get_display_height();
    int get_output_width();
    int get_output_height();
    bool get_output_date();
    int get_font_size();
    int get_font_color();
    bool get_light_enable();
    
protected:
    class ConfigImpl;
    typedef boost::shared_ptr < ConfigImpl > ConfigImplPtr;
    ConfigImplPtr impl_;
    
};
