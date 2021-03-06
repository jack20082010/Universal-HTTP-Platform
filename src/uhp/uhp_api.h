/*
 * 
 * author	: jack
 * email	: xulongzhong2004@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */
  
#ifndef _UHP_API_H_
#define _UHP_API_H_

typedef struct HttpserverEnv HttpserverEnv;
typedef struct AcceptedSession AcceptedSession;

#ifdef __cplusplus
extern "C" {
#endif

typedef int  (*fn_notify)( AcceptedSession *p_current, AcceptedSession *p_other, void* arg );

HttpserverEnv*  UHPGetEnv();

void* UHPGetReserver1( );
int UHPSetReserver1( void *p_reserver );

void* UHPGetReserver2( );
int UHPSetReserver2( void *p_reserver );

void* UHPGetReserver3( );
int UHPSetReserver3( void *p_reserver );

void* UHPGetReserver4( );
int UHPSetReserver4( void *p_reserver );

void* UHPGetReserver5( );
int UHPSetReserver5( void *p_reserver );

void* UHPGetReserver6( );
int UHPSetReserver6( void *p_reserver );


struct NetAddress* UHPGetSessionNetAddress( AcceptedSession *p_session );
struct HttpEnv* UHPGetSessionHttpEnv( AcceptedSession *p_session );
char* UHPGetSessionResponse( AcceptedSession *p_session );
int UHPGetSessionResponseSize( AcceptedSession *p_session );
char* UHPGetSessionCharset( AcceptedSession *p_session );
long UHPGetSessionRequestTime( AcceptedSession *p_session );
long UHPGetSessionAcceptTime( AcceptedSession *p_session );
//int UHPSetSessionStatusCode( AcceptedSession *p_session, int status_code );
int UHPSetNotifyOtherSessions( AcceptedSession *p_current, fn_notify p_notify_cb, void *cb_arg );


int UHPInitLogEnv( );
int UHPCleanLogEnv( );

int UHPGetReserveInt1( );
int UHPGetReserveInt2( );

int UHPGetReserveInt3( );
int UHPGetReserveInt4( );
int UHPGetReserveInt5( );
int UHPGetReserveInt6( );

char* UHPGetReserveStr301( );
char* UHPGetReserveStr302( );

char* UHPGetReserveStr501( );
char* UHPGetReserveStr502( );
char* UHPGetReserveStr801( );
char* UHPGetReserveStr802( );

char* UHPGetReserveStr1281( );
char* UHPGetReserveStr1282( );
char* UHPGetReserveStr2551( );
char* UHPGetReserveStr2552( );

char* UHPGetDBIp( );
int   UHPGetDBPort( );
char* UHPGetDBUserName( );
char* UHPGetDBPassword( );
char* UHPGetDBName( );
int UHPGetMinConnections( );
int UHPGetMaxConnections( );

void* UHPGetDBPool( );
int UHPSetDBPool( void* pool );

#ifdef __cplusplus
}
#endif

#endif
