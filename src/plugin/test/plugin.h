
#ifndef _H_PLUGIN_
#define _H_PLUGIN_


#ifdef __cplusplus
extern "C" {
#endif

int load();
int unload();
int doworker( struct AcceptedSession *p_session );

#ifdef __cplusplus
}
#endif

#endif