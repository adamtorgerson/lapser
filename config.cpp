
#include <iostream>
#include <fstream>
#include <json/json.h>

#include "config.hpp"

using namespace std;

class Config::ConfigImpl
{
public:
    ConfigImpl(const std::string &path)
        :
        path_(path)
    {
        Json::Reader reader;
        std::ifstream file;
        file.open(path.c_str());
        if(!reader.parse(file, root_))
        {
            std::cout << "Couldn't parse '" << path << "'" << std::endl;
            return;
        }
        
        validate();
    }
    
    virtual ~ConfigImpl() { }

    int get_interval()
    {
        return root_.get("interval", 69).asInt();
    }

    string get_capture_name()
    {
        return root_.get("capture-name", "").asString();
    }

    string get_output_name()
    {
        return root_.get("output-name", "").asString();
    }

    string get_font_name()
    {
        return root_.get("font-name", "font.ttf").asString();
    }

    int get_capture_width()
    {
        return root_.get("capture-width", 2592).asInt();
    }
    
    int get_capture_height()
    {
        return root_.get("capture-height", 1944).asInt();
    }
    
    int get_display_width()
    {
        return root_.get("display-width", 320).asInt();
    }
    
    int get_display_height()
    {
        return root_.get("display-height", 240).asInt();
    }
    
    int get_output_width()
    {
        return root_.get("output-width", 1024).asInt();
    }
    
    int get_output_height()
    {
        return root_.get("output-height", 768).asInt();
    }
    
    bool get_output_date()
    {
        return root_.get("output-date", false).asBool();
    }
    
    int get_font_size()
    {
        return root_.get("font-size", 18).asInt();
    }
    
    int get_font_color()
    {
        return root_.get("font-color", 0xff0000).asInt();
    }

    bool get_light_enable()
    {
        return root_.get("light-enable", true).asBool();
    }    
    
private:
    bool validate()
    {
        return true;
    }
    
    const std::string path_;
    Json::Value root_;
};

Config::Config(const std::string &path)
    :
    impl_(new ConfigImpl(path))
{ }

Config::~Config()
{ }

int Config::get_interval()
{ return impl_->get_interval(); }

string Config::get_capture_name()
{ return impl_->get_capture_name(); }

string Config::get_output_name()
{ return impl_->get_output_name(); }

string Config::get_font_name()
{ return impl_->get_font_name(); }

int Config::get_capture_width()
{ return impl_->get_capture_width(); }

int Config::get_capture_height()
{ return impl_->get_capture_height(); }

int Config::get_display_width()
{ return impl_->get_display_width(); }

int Config::get_display_height()
{ return impl_->get_display_height(); }

int Config::get_output_width()
{ return impl_->get_output_width(); }

int Config::get_output_height()
{ return impl_->get_output_height(); }

bool Config::get_output_date()
{ return impl_->get_output_date(); }

int Config::get_font_size()
{ return impl_->get_font_size(); }

int Config::get_font_color()
{ return impl_->get_font_color(); }

bool Config::get_light_enable()
{ return impl_->get_light_enable(); }
