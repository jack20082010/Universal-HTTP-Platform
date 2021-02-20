/*
 * 
 * author	: jack
 * email	: xulongzhong2004@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */
  
#include "uhp_api.h"
#include "uhp_in.h"

HttpserverEnv*		g_env = NULL;
int			g_process_index = PROCESS_INIT;

HttpserverEnv*  UHPGetEnv()
{
	return g_env;
}

int UHPSetEnv( HttpserverEnv* p_env )
{
	if( !p_env )
		return -1;
		
	g_env = p_env;
	
	return 0;
}

void* UHPGetReserver1( )
{
	if( !g_env )
		return NULL;

	return g_env->p_reserver1;
}

int UHPSetReserver1( void *p_reserver )
{
	if( !g_env )
		return -1;
	
	g_env->p_reserver1 = p_reserver;

	return 0;
}

void* UHPGetReserver2( )
{
	if( !g_env )
		return NULL;

	return g_env->p_reserver2;
}

int UHPSetReserver2( void *p_reserver )
{
	if( !g_env )
		return -1;
	
	g_env->p_reserver2 = p_reserver;

	return 0;
}

void* UHPGetReserver3( )
{
	if( !g_env )
		return NULL;

	return g_env->p_reserver3;
}

int UHPSetReserver4( void *p_reserver )
{
	if( !g_env )
		return -1;
	
	g_env->p_reserver4 = p_reserver;

	return 0;
}

void* UHPGetReserver5( )
{
	if( !g_env )
		return NULL;

	return g_env->p_reserver5;
}

int UHPSetReserver5( void *p_reserver )
{
	if( !g_env )
		return -1;
	
	g_env->p_reserver5 = p_reserver;

	return 0;
}

void* UHPGetReserver6( )
{
	if( !g_env )
		return NULL;

	return g_env->p_reserver6;
}

int UHPSetReserver6( void *p_reserver )
{
	if( !g_env )
		return -1;
	
	g_env->p_reserver6 = p_reserver;

	return 0;
}

struct NetAddress* UHPGetSessionNetAddress( AcceptedSession *p_session )
{
	if( !p_session )
		return NULL;

	return &( p_session->netaddr );
}

struct HttpEnv* UHPGetSessionHttpEnv( AcceptedSession *p_session )
{
	if( !p_session )
		return NULL;
		
	return p_session->http;
}


char* UHPGetSessionResponse( AcceptedSession *p_session )
{
	if( !p_session )
		return NULL;
		
	return p_session->http_rsp_body;
}

int UHPGetSessionResponseSize( AcceptedSession *p_session )
{
	if( !p_session )
		return -1;
		
	return p_session->body_len;
}

char* UHPGetSessionCharset( AcceptedSession *p_session )
{
	if( !p_session )
		return NULL;
		
	return p_session->charset;
}

long UHPGetSessionRequestTime( AcceptedSession *p_session )
{
	if( !p_session )
		return -1;
		
	return p_session->request_begin_time;
}

long UHPGetSessionAcceptTime( AcceptedSession *p_session )
{
	if( !p_session )
		return -1;
		
	return p_session->accept_begin_time;
}

/*
int UHPSetSessionStatusCode( AcceptedSession *p_session, int status_code )
{
	if( !p_session )
		return -1;
		
	p_session->status_code = status_code;
	
	return 0;
}
*/

int UHPSetNotifyOtherSessions( AcceptedSession *p_current, fn_notify p_notify_cb, void *cb_arg )
{
	int			index;
	AcceptedSession		*p_session = NULL;
	setSession::iterator 	it;
	int			nret;
	
	for( index = 0; index < g_env->httpserver_conf.httpserver.server.epollThread; index++ )
	{
		pthread_mutex_lock( &g_env->thread_epoll[index].session_lock );
		it = g_env->thread_epoll[index].p_set_session->begin();
		while( it != g_env->thread_epoll[index].p_set_session->end() )
		{
			p_session = *it;
			if( p_session != p_current && p_session->status == SESSION_STATUS_HANG  )
			{
				ResetHttpBuffer( GetHttpResponseBuffer(p_session->http) );
				p_session->hang_status = 0;
				nret = p_notify_cb( p_current, p_session, cb_arg );
				if( nret )
					return nret;
				
				nret = AddEpollSendEvent( g_env, p_session );
				if( nret )
					return nret;
						
			}
			it++;
		}
		pthread_mutex_unlock( &g_env->thread_epoll[index].session_lock );
	}
	
	return 0;
			
}
int UHPGetReserveInt1( )
{
	if( !g_env )
		return -1;
		
	return g_env->httpserver_conf.httpserver.reserve.int1;
}

int UHPGetReserveInt2( )
{
	if( !g_env )
		return -1;
	
	return g_env->httpserver_conf.httpserver.reserve.int2;
}

int UHPGetReserveInt3( )
{
	if( !g_env )
		return -1;
	
	return g_env->httpserver_conf.httpserver.reserve.int3;
}

int UHPGetReserveInt4( )
{
	if( !g_env )
		return -1;
	
	return g_env->httpserver_conf.httpserver.reserve.int4;
}

int UHPGetReserveInt5( )
{
	if( !g_env )
		return -1;
	
	return g_env->httpserver_conf.httpserver.reserve.int5;
}

int UHPGetReserveInt6( )
{
	if( !g_env )
		return -1;
	
	return g_env->httpserver_conf.httpserver.reserve.int6;
}


char* UHPGetReserveStr301( )
{
	if( !g_env )
		return NULL;
	
	return g_env->httpserver_conf.httpserver.reserve.str301;
}

char* UHPGetReserveStr302( )
{
	if( !g_env )
		return NULL;
	
	return g_env->httpserver_conf.httpserver.reserve.str302;
}

char* UHPGetReserveStr501( )
{
	if( !g_env )
		return NULL;
	
	return g_env->httpserver_conf.httpserver.reserve.str501;
}

char* UHPGetReserveStr502( )
{
	if( !g_env )
		return NULL;
	
	return g_env->httpserver_conf.httpserver.reserve.str502;
}


char* UHPGetReserveStr801( )
{
	if( !g_env )
		return NULL;
	
	return g_env->httpserver_conf.httpserver.reserve.str501;
}

char* UHPGetReserveStr802( )
{
	if( !g_env )
		return NULL;
	
	return g_env->httpserver_conf.httpserver.reserve.str802;
}

char* UHPGetReserveStr1281( )
{
	if( !g_env )
		return NULL;
	
	return g_env->httpserver_conf.httpserver.reserve.str1281;
}

char* UHPGetReserveStr1282( )
{
	if( !g_env )
		return NULL;
	
	return g_env->httpserver_conf.httpserver.reserve.str1282;
}

char* UHPGetReserveStr2551( )
{
	if( !g_env )
		return NULL;
	
	return g_env->httpserver_conf.httpserver.reserve.str2551;
}

char* UHPGetReserveStr2552( )
{
	if( !g_env )
		return NULL;
	
	return g_env->httpserver_conf.httpserver.reserve.str2552;
}

int UHPCleanLogEnv( )
{
	if( !g_env )
		return -1;
	
	return CleanLogEnv();
}

int UHPInitLogEnv( )
{
	char 	module_name[50];
	
	if( !g_env )
		return -1;
	
	memset( module_name, 0, sizeof(module_name) );
	snprintf( module_name, sizeof(module_name)-1, "worker_%d", g_process_index+1 );
	
	return InitLogEnv( g_env, module_name, SERVER_AFTER_LOADCONFIG );
}

char* UHPGetDBIp( )
{
	if( !g_env )
		return NULL;
	
	return g_env->httpserver_conf.httpserver.database.ip;
}
int UHPGetDBPort( )
{
	if( !g_env )
		return -1;
	
	return g_env->httpserver_conf.httpserver.database.port;
}

char* UHPGetDBUserName( )
{
	if( !g_env )
		return NULL;
	
	return g_env->httpserver_conf.httpserver.database.username;
}

char* UHPGetDBPassword( )
{
	if( !g_env )
		return NULL;
	
	return g_env->httpserver_conf.httpserver.database.password;
}

char* UHPGetDBName( )
{
	if( !g_env )
		return NULL;
	
	return g_env->httpserver_conf.httpserver.database.dbname;
}

int UHPGetMinConnections( )
{
	if( !g_env )
		return -1;
	
	return g_env->httpserver_conf.httpserver.database.minConnections;
}

int UHPGetMaxConnections( )
{
	if( !g_env )
		return -1;
	
	return g_env->httpserver_conf.httpserver.database.maxConnections;

}

void* UHPGetDBPool( )
{
	if( !g_env )
		return NULL ;

	return g_env->p_dbpool;
}

int UHPSetDBPool( void* pool )
{
	if( !g_env )
		return -1;
	
	g_env->p_dbpool = pool;
	
	return 0;
}


