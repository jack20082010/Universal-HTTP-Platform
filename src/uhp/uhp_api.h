/*
 * 
 * author	: jack
 * email	: xulongzhong2004@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */
  
#ifndef _UHP_API_H_
#define _UHP_API_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct HttpserverEnv HttpserverEnv;
typedef struct AcceptedSession AcceptedSession;

void* GetHttpserverReserver1( );
int SetHttpserverReserver1( void *p_reserver );

void* GetHttpserverReserver2( );
int SetHttpserverReserver2( void *p_reserver );

void* GetHttpserverReserver3( );
int SetHttpserverReserver3( void *p_reserver );

void* GetHttpserverReserver4( );
int SetHttpserverReserver4( void *p_reserver );

void* GetHttpserverReserver5( );
int SetHttpserverReserver5( void *p_reserver );

void* GetHttpserverReserver6( );
int SetHttpserverReserver6( void *p_reserver );


struct NetAddress* GetSessionNetAddress( struct AcceptedSession *p_session );
struct HttpEnv* GetSessionHttpEnv( struct AcceptedSession *p_session );
char* GetSessionResponse( struct AcceptedSession *p_session );
int GetSessionResponseSize( struct AcceptedSession *p_session );
char* GetSessionCharset( struct AcceptedSession *p_session );
long GetSessionRequestTime( struct AcceptedSession *p_session );
long GetSessionAcceptTime( struct AcceptedSession *p_session );

int InitPluginLogEnv( );
int CleanPluginLogEnv( );

int GetPluginConfigInt1( );
int GetPluginConfigInt2( );

int GetPluginConfigInt3( );
int GetPluginConfigInt4( );
int GetPluginConfigInt5( );
int GetPluginConfigInt6( );

char* GetPluginConfigStr301( );
char* GetPluginConfigStr302( );

char* GetPluginConfigStr501( );
char* GetPluginConfigStr502( );
char* GetPluginConfigStr801( );
char* GetPluginConfigStr802( );

char* GetPluginConfigStr1281( );
char* GetPluginConfigStr1282( );
char* GetPluginConfigStr2551( );
char* GetPluginConfigStr2552( );


#ifdef __cplusplus
}
#endif

#endif
