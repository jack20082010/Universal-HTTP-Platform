
#ifndef _H_PLUGIN_
#define _H_PLUGIN_

#include "uhp_api.h"

#ifdef __cplusplus
extern "C" {
#endif

int Load();
int Unload();
int OnHeartbeat();

#ifdef __cplusplus
}
#endif

#endif