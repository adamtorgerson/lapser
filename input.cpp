
#include <map>
#include <iostream>

#include "input.hpp"

using namespace std;

class Input::InputImpl
{
public:
    InputImpl(Display &d)
    :
    display_(d) { }

    virtual ~InputImpl() { }

    Action::Action process()
    {
        SDL_Event event;
        while(SDL_PollEvent(&event))
        {
            switch(event.type)
            {
            case SDL_KEYDOWN:
                return Action::QUIT;
                break;
            case SDL_MOUSEBUTTONDOWN:
            {
                int x, y;
                SDL_GetMouseState(&x, &y);

                ActionMap::const_iterator i;
                for(i = map_.begin(); i != map_.end(); i++)
                {
                    const SDL_Rect &r = i->second;
                    if(x >= r.x && x <= r.x + r.w && y >= r.y && y <= r.y + r.h)
                    {
                        return i->first;
                    }
                }
                    
            }
                break;
            default:
                break;
            }
        }

        return Action::CONTINUE;
    }

    bool add_region(const Action::Action action, const SDL_Rect &r)
    {
        if(map_.find(action) != map_.end())
            return false;

        map_[action] = r;
        return true;
    }

    void draw_regions()
    {
        ActionMap::const_iterator i;
        for(i = map_.begin(); i != map_.end(); i++)
        {
            string s;
            switch(i->first)
            {
            case Action::ENABLE:
                s = "Enable";
                display_.put_button(s, i->second);
                break;
            case Action::DELETE_LAST:
                s = "Delete";
                display_.put_button(s, i->second);
                break;
            default:
                break;
            }
        }
    }
    
private:
    typedef map < Action::Action,  SDL_Rect > ActionMap;
    ActionMap map_;
    Display &display_;
};

Input::Input(Display &d)
    :
    impl_(new InputImpl(d))
{
}

Input::~Input()
{
}

Action::Action Input::process()
{
    return impl_->process();
}

bool Input::add_region(const Action::Action action, const SDL_Rect &r)
{
    return impl_->add_region(action, r);
}

void Input::draw_regions()
{
    impl_->draw_regions();
}
