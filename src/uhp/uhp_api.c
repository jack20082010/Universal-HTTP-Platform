/*
 * 
 * author	: jack
 * email	: xulongzhong2004@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */
  
#include "uhp_api.h"
#include "uhp_in.h"

struct HttpserverEnv*		g_env = NULL;

struct HttpserverEnv*  GetHttpserverEnv()
{
	return g_env;
}

int SetHttpserverEnv( struct HttpserverEnv* p_env )
{
	if( !p_env )
		return -1;
		
	g_env = p_env;
	
	return 0;
}

void* GetHttpserverReserver1( )
{
	if( !g_env )
		return NULL;

	return g_env->p_reserver1;
}

int SetHttpserverReserver1( void *p_reserver )
{
	if( !g_env )
		return -1;
	
	g_env->p_reserver1 = p_reserver;

	return 0;
}

void* GetHttpserverReserver2( )
{
	if( !g_env )
		return NULL;

	return g_env->p_reserver2;
}

int SetHttpserverReserver2( void *p_reserver )
{
	if( !g_env )
		return -1;
	
	g_env->p_reserver2 = p_reserver;

	return 0;
}

void* GetHttpserverReserver3( )
{
	if( !g_env )
		return NULL;

	return g_env->p_reserver3;
}

int SetHttpserverReserver4( void *p_reserver )
{
	if( !g_env )
		return -1;
	
	g_env->p_reserver4 = p_reserver;

	return 0;
}

void* GetHttpserverReserver5( )
{
	if( !g_env )
		return NULL;

	return g_env->p_reserver5;
}

int SetHttpserverReserver5( void *p_reserver )
{
	if( !g_env )
		return -1;
	
	g_env->p_reserver5 = p_reserver;

	return 0;
}

void* GetHttpserverReserver6( )
{
	if( !g_env )
		return NULL;

	return g_env->p_reserver6;
}

int SetHttpserverReserver6( void *p_reserver )
{
	if( !g_env )
		return -1;
	
	g_env->p_reserver6 = p_reserver;

	return 0;
}

struct NetAddress* GetSessionNetAddress( struct AcceptedSession *p_session )
{
	if( !p_session )
		return NULL;

	return &( p_session->netaddr );
}

struct HttpEnv* GetSessionHttpEnv( struct AcceptedSession *p_session )
{
	if( !p_session )
		return NULL;
		
	return p_session->http;
}


char* GetSessionResponse( struct AcceptedSession *p_session )
{
	if( !p_session )
		return NULL;
		
	return p_session->http_rsp_body;
}

int GetSessionResponseSize( struct AcceptedSession *p_session )
{
	if( !p_session )
		return -1;
		
	return p_session->body_len;
}

char* GetSessionCharset( struct AcceptedSession *p_session )
{
	if( !p_session )
		return NULL;
		
	return p_session->charset;
}

long GetSessionRequestTime( struct AcceptedSession *p_session )
{
	if( !p_session )
		return -1;
		
	return p_session->request_begin_time;
}

long GetSessionAcceptTime( struct AcceptedSession *p_session )
{
	if( !p_session )
		return -1;
		
	return p_session->accept_begin_time;
}

int GetPluginConfigInt1( )
{
	if( !g_env )
		return -1;
		
	return g_env->httpserver_conf.httpserver.plugin.int1;
}

int GetPluginConfigInt2( )
{
	if( !g_env )
		return -1;
	
	return g_env->httpserver_conf.httpserver.plugin.int2;
}

int GetPluginConfigInt3( )
{
	if( !g_env )
		return -1;
	
	return g_env->httpserver_conf.httpserver.plugin.int3;
}

int GetPluginConfigInt4( )
{
	if( !g_env )
		return -1;
	
	return g_env->httpserver_conf.httpserver.plugin.int4;
}

int GetPluginConfigInt5( )
{
	if( !g_env )
		return -1;
	
	return g_env->httpserver_conf.httpserver.plugin.int5;
}

int GetPluginConfigInt6( )
{
	if( !g_env )
		return -1;
	
	return g_env->httpserver_conf.httpserver.plugin.int6;
}


char* GetPluginConfigStr301( )
{
	if( !g_env )
		return NULL;
	
	return g_env->httpserver_conf.httpserver.plugin.str301;
}

char* GetPluginConfigStr302( )
{
	if( !g_env )
		return NULL;
	
	return g_env->httpserver_conf.httpserver.plugin.str302;
}

char* GetPluginConfigStr501( )
{
	if( !g_env )
		return NULL;
	
	return g_env->httpserver_conf.httpserver.plugin.str501;
}

char* GetPluginConfigStr502( )
{
	if( !g_env )
		return NULL;
	
	return g_env->httpserver_conf.httpserver.plugin.str502;
}


char* GetPluginConfigStr801( )
{
	if( !g_env )
		return NULL;
	
	return g_env->httpserver_conf.httpserver.plugin.str501;
}

char* GetPluginConfigStr802( )
{
	if( !g_env )
		return NULL;
	
	return g_env->httpserver_conf.httpserver.plugin.str802;
}

char* GetPluginConfigStr1281( )
{
	if( !g_env )
		return NULL;
	
	return g_env->httpserver_conf.httpserver.plugin.str1281;
}

char* GetPluginConfigStr1282( )
{
	if( !g_env )
		return NULL;
	
	return g_env->httpserver_conf.httpserver.plugin.str1282;
}

char* GetPluginConfigStr2551( )
{
	if( !g_env )
		return NULL;
	
	return g_env->httpserver_conf.httpserver.plugin.str2551;
}

char* GetPluginConfigStr2552( )
{
	if( !g_env )
		return NULL;
	
	return g_env->httpserver_conf.httpserver.plugin.str2552;
}

int InitPluginLogEnv( char* module_name )
{
	if( !g_env )
		return -1;
	
	return InitLogEnv(g_env, "Plugin", SERVER_AFTER_LOADCONFIG );
}

int CleanPluginLogEnv( )
{
	if( !g_env )
		return -1;
	
	return cleanLogEnv();
}

