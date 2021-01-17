
#ifndef _H_PLUGIN_
#define _H_PLUGIN_
#include "uhp_api.h"

#ifdef __cplusplus
extern "C" {
#endif

int Load();
int Unload();
int Doworker( AcceptedSession *p_session );
int OnException( AcceptedSession *p_session, int errcode, char *errmsg );

#ifdef __cplusplus
}
#endif

#endif