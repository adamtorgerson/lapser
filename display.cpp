
#include <stdexcept>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>

#include "display.hpp"

using namespace std;

class Display::DisplayImpl
{
public:
    DisplayImpl(Config &config)
    :
    config_(config),
    screen_(NULL),
    font_(NULL),
    font_height_(-1)
    {
        SDL_Init(SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE);
        TTF_Init();
        
        screen_ = SDL_SetVideoMode(config.get_display_width(), config.get_display_height(), 0, SDL_HWSURFACE | SDL_DOUBLEBUF);
        if(!screen_)
        {
            ostringstream oss;
            oss << "Couldn't open screen";
            throw runtime_error(oss.str());
        }

        //SDL_ShowCursor(0);

        font_ = TTF_OpenFont(config_.get_font_name().c_str(), config_.get_font_size());
        if(!font_)
        {
            ostringstream oss;
            oss << "Couldn't open font: " << config_.get_font_name();
            throw runtime_error(oss.str());            
        }
        TTF_SetFontStyle(font_, TTF_STYLE_NORMAL);
        TTF_SetFontHinting(font_, TTF_HINTING_LIGHT);
        TTF_SetFontOutline(font_, 1);
        font_height_ = TTF_FontHeight(font_);
    }
    
    ~DisplayImpl()
    {
        SDL_FreeSurface(screen_);
        TTF_CloseFont(font_);
    }

    void put_string_surf(SDL_Surface *out, const string &str, const int x, const int y)
    {
        SDL_Color color = { 255, 0, 0 };
        SDL_Surface *surface = TTF_RenderUTF8_Blended(font_, str.c_str(), color);
        SDL_SetAlpha(surface, (SDL_RLEACCEL | SDL_SRCALPHA), SDL_ALPHA_TRANSPARENT);

        SDL_Rect dest;
        dest.x = x;
        dest.y = y >= 0 ? y : screen_->h - font_height_;
        SDL_BlitSurface(surface, NULL, out, &dest);
        SDL_FreeSurface(surface);        
    }
    
    void put_string(const string &str, const int x, const int y)
    {
        const int color_i = config_.get_font_color();
        SDL_Color color = { (uint8_t)((color_i & 0xff0000) >> 16), (uint8_t)((color_i & 0x00ff00) >> 8), (uint8_t)(color_i & 0x0000ff) };
        SDL_Surface *surface = TTF_RenderUTF8_Blended(font_, str.c_str(), color);
        SDL_SetAlpha(surface, (SDL_RLEACCEL | SDL_SRCALPHA), SDL_ALPHA_TRANSPARENT);

        SDL_Rect dest;
        dest.x = x;
        dest.y = y >= 0 ? y : screen_->h - font_height_;
        SDL_BlitSurface(surface, NULL, screen_, &dest);
        SDL_FreeSurface(surface);
    }
    
    void put_frame(cv::Mat &frame)
    {
        cv::Mat resized;
        const double w_ratio = (double)frame.cols / screen_->w;
        const double h_ratio = (double)frame.rows / screen_->h;
        const double ratio = w_ratio > h_ratio ? h_ratio : w_ratio;
        //cout << "Ratio is " << ratio << " screen " << screen_->w << "x" << screen_->h << " frame " << frame.cols << "x" << frame.rows << endl;
        
        cv::resize(frame, resized, cv::Size(frame.cols / ratio, frame.rows / ratio));

        SDL_Surface *surf = SDL_CreateRGBSurfaceFrom(resized.data, resized.cols, resized.rows,
                                                     8 * resized.step / resized.cols, resized.step,
                                                     0xff0000, 0x00ff00, 0x0000ff, 0);
        SDL_BlitSurface(surf, NULL, screen_, NULL);
        SDL_FreeSurface(surf);
    }

    void put_button(const string &text, const SDL_Rect &r)
    {
        cv::Mat button(r.h, r.w, CV_8UC3);
        button = cv::Scalar(255, 255, 255);
        cv::rectangle(button, cv::Point(0, 0), cv::Point(r.w - 1, r.h - 1), cv::Scalar(255, 0, 0));
        cv::rectangle(button, cv::Point(2, 2), cv::Point(r.w - 3, r.h - 3), cv::Scalar(255, 0, 0));

        SDL_Surface *surf = SDL_CreateRGBSurfaceFrom(button.data, button.cols, button.rows,
                                                     8 * button.step / button.cols, button.step,
                                                     0xff0000, 0x00ff00, 0x0000ff, 0);
        SDL_SetColorKey(surf, true, SDL_MapRGB(screen_->format, 255, 255, 255));

        put_string_surf(surf, text, 20, 8);
        
        SDL_Rect r_copy = r;
        SDL_BlitSurface(surf, NULL, screen_, &r_copy);
        SDL_FreeSurface(surf);
    }
    
    bool draw()
    {
        SDL_Flip(screen_);
        return true;
    }

private:
    Config &config_;
    SDL_Surface *screen_;
    TTF_Font *font_;
    int font_height_;
};

Display::Display(Config &config)
    :
    impl_(new DisplayImpl(config))
{
}

Display::~Display()
{
}

void Display::put_string(const string &str, const int x, const int y)
{ impl_->put_string(str, x, y); }

void Display::put_string(cv::Mat &frame, const string &str, const int x, const int y)
{
    SDL_Surface *surf = SDL_CreateRGBSurfaceFrom(frame.data, frame.cols, frame.rows,
                                                 8 * frame.step / frame.cols, frame.step,
                                                 0xff0000, 0x00ff00, 0x0000ff, 0);

    impl_->put_string_surf(surf, str, x, y);
    SDL_FreeSurface(surf);
}

void Display::put_frame(cv::Mat &frame)
{ impl_->put_frame(frame); }

void Display::put_button(const string &text, const SDL_Rect &r)
{ impl_->put_button(text, r); }

bool Display::draw()
{ return impl_->draw(); }
