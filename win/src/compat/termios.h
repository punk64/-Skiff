#pragma once
#include "Platform.h"

struct termios {
    unsigned int c_iflag;
    unsigned int c_oflag;
    unsigned int c_cflag;
    unsigned int c_lflag;
    unsigned char c_line;
    unsigned char c_cc[32];
    unsigned int c_ispeed;
    unsigned int c_ospeed;
};

#ifndef TCSANOW
#define TCSANOW 0
#endif
#ifndef B9600
#define B9600 9600
#endif
