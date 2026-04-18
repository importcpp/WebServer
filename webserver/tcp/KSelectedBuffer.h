#pragma once

#ifdef USE_RINGBUFFER
#include "webserver/tcp/KRingBuffer.h"
#else
#include "webserver/tcp/KBuffer.h"
#endif
