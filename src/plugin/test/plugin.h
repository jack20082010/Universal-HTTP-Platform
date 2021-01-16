
#ifndef _H_PLUGIN_
#define _H_PLUGIN_


#ifdef __cplusplus
extern "C" {
#endif

int Load();
int Unload();
int Doworker( struct AcceptedSession *p_session );

#ifdef __cplusplus
}
#endif

#endif