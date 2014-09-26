lapser
======

Time lapse video creator, targetted for the Raspberry Pi.

Dependencies for raspbian wheezer:

cmake
libboost-program-options1.50-dev
libboost-thread1.50-dev
libboost-system1.50-dev
libboost-date-time1.50-dev
libopencv-core-dev
libopencv-highgui-dev
libopencv-imgproc-dev
libjson-cpp-dev
libsdl1.2-dev
libsdl-ttf2.0-dev

- Pi Build -

I am building lapser for the Pi on the Pi itself, with the above
dependencies. Just run "cmake" in the lapser directory to configure,
but there are a couple other things to line up.

You will need to checkout the 'userland git':

https://github.com/raspberrypi/userland.git

And, set your path in the PI_USERLAND_PATH in CMakeLists.txt.

Also, in case I forgot, make sure the ARCH is set to ARM, as the PC
build will not use the MMAL interface.

For those new to cmake, you generally just have to run "cmake" until
it passes the configure stage, at which point a regular Makefile is
generated and you can just run "make". Do not edit the generated
Makefiles, the CMakeLists.txt is not that complicated.

- Display -

lapser has a very simple UI usable from a mouse or touchscreen. Right
now all it lets you do is disable and reenable time lapse recording.

For the Pi build, the code is currently set up to use /dev/fb1 as the
framebuffer device, and sets the mouse drive to tslib. This is easy to
change.

- Camera types -

MMAL is the Pi's native camera type. The OpenCV camera type is
provided for testing and development purposes, along with the tree.avi
file which is straight from OpenCV. This is 

- Config file explanation -

interval - Time interval between time lapse images, in ms. Note, that
  lapser attempts to take each picture at this precise interval,
  regardless of how long it actually takes the camera to take the
  picture. Thus, it is possible to set this value too low to be
  practically possible.
output-width - output time lapse video width, in pixels
output-height - output time lapse video height, in pixels
display-width - height of SDL display surface
display-height - width of SDL display surface
output-name - name of the time lapse output video
output-date - output a date stamp in the time lapse output video
font-name - truetype font file for font drawing activity
font-size - pixel size of font
font-color - hex RGB color (0xRRGGBB), currently needs to be
  converted to decimal as json does not support native hex
  
PHYSICAL CAMERA ONLY OPTIONS:
capture-height - height of time lapse output video, in pixels
capture-width - width of time lapse output video, in pixels
light-enable - enable/disable the camera light with this setting

OPENCV CAMERA ONLY OPTIONS:
capture-name - input file to read from for testing purposes
