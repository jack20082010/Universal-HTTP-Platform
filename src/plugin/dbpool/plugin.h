
#ifndef _H_PLUGIN_
#define _H_PLUGIN_

#include "uhp_api.h"

#ifdef __cplusplus
extern "C" {
#endif

int Load();
int Unload();
int Doworker( AcceptedSession *p_session );
int OnRequest( AcceptedSession *p_session );
int OnResponse( AcceptedSession *p_session );

#ifdef __cplusplus
}
#endif

#endif