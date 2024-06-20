
#ifndef __PCHAR_IOCTL_H
#define __PCHAR_IOCTL_H

#include "linux/ioctl.h"

typedef struct {
	short size;
	short avail;
	short len;
}info_t;

#define FIFO_CLEAR _IO('x',1)
#define FIFO_INFO _IOR('x', 2, info_t)
#define FIFO_RESIZE _IOW('x',3,long)
#endif
