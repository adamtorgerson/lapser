
set(ARCH "")

if(ARCH STREQUAL ARM)
  set(ARCH_CFLAGS "-march=armv6j -mfpu=vfp -mfloat-abi=hard")
  set(MMAL_SOURCES camera-mmal.cpp RaspiStuff.c)
  set(MMAL_LIBS mmal_core mmal_util mmal_vc_client vcos bcm_host)
  set(MMAL_INC ../userland/host_applications/linux/apps/raspicam ../userland ../userland/host_applications/linux/libs/bcm_host/include ../userland/build/inc/interface/vcos ../userland/interface/vmcs_host/linux)
  set(USE_MMAL -DARM_BUILD)
else()
  set(ARCH_CFLAGS)
  set(MMAL_SOURCES)
  set(MMAL_LIBS)
  set(MMAL_INC)
  set(USE_MMAL)
endif()
  
set(SOURCES main.cpp config.cpp camera-opencv.cpp ${MMAL_SOURCES} display.cpp input.cpp)

set(CMAKE_CXX_FLAGS "-Wall ${ARCH_CFLAGS} -ffunction-sections -fdata-sections ${USE_MMAL}")
set(CMAKE_C_FLAGS "-Wall ${ARCH_CFLAGS} -ffunction-sections -fdata-sections ${USE_MMAL}")
set(CMAKE_EXE_LINKER_FLAGS "-Wl,--gc-sections")

link_directories(/opt/vc/lib)
include_directories(/usr/include/jsoncpp ${MMAL_INC})

add_executable(lapser ${SOURCES})

target_link_libraries(lapser boost_program_options boost_thread boost_system boost_date_time
  jsoncpp opencv_core opencv_imgproc opencv_highgui SDL SDL_ttf ${MMAL_LIBS})
