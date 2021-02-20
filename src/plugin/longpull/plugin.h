
#ifndef _H_PLUGIN_
#define _H_PLUGIN_
#include "uhp_api.h"
#include "fasterhttp.h"

#define URI_MESSAGE 		"/longpull/message"
#define URI_NOTIFY 		"/longpull/notify"

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