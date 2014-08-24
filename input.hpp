
#include <boost/shared_ptr.hpp>
#include <SDL/SDL.h>

#include "display.hpp"

namespace Action
{
    enum Action
    {
        QUIT = 0,
        ENABLE,
        DISABLE,
        CONTINUE,
        DELETE_LAST,
    };
}

class Input
{
public:
    Input(Display &display);
    virtual ~Input();

    Action::Action process();

    bool add_region(const Action::Action action, const SDL_Rect &r);
    void draw_regions();
    
private:
    class InputImpl;
    typedef boost::shared_ptr < InputImpl > InputImplPtr;
    InputImplPtr impl_;
};
