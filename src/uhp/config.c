/*
 * author	: jack
 * email	: xulongzhong2004@163.com
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "uhp_in.h"
#include <sys/mman.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>

#include "IDL_httpserver_conf.dsc.LOG.c"
#define DEFAULT_MAX_TASK_COUNT		1000*10000
#define DEFAULT_MAX_EXIT_TIME		10
#define DEFAULT_EPOLL_THREAD		2
#define DEFAULT_HANG_UP_TIMEOUT		30

int InitConfigFiles( HttpserverEnv *p_env )
{
	char		pathname[ UTIL_MAXLEN_FILENAME + 1 ] ;
	char		pathfilename[ UTIL_MAXLEN_FILENAME + 1 ];
	httpserver_conf	httpserver_conf ;
	char		*file_content = NULL ;
	int		file_len ;
	int		nret = 0 ;
	
	memset( pathname , 0x00 , sizeof(pathname) );
	snprintf( pathname , sizeof(pathname)-1 , "%s/etc" , secure_getenv("HOME") );
	if( access( pathname , X_OK ) == 0 )
	{
		printf( "WARN : %s exist !\n" , pathname );
	}
	else
	{
		mkdir( pathname , 0755 );
		printf( "mkdir %s ok\n" , pathname );
	}

	memset( pathfilename , 0x00 , sizeof(pathfilename) );
	snprintf( pathfilename , sizeof(pathfilename)-1 , "%s/etc/%s" , secure_getenv("HOME") , SERVER_CONF_FILENAME );
	if( access( pathfilename , R_OK ) == 0 )
	{
		printf( "WARN : %s exist !\n" , pathfilename );
	}
	else
	{ 
		char ip[15];
		
		memset( ip, 0, sizeof(ip) );
		GetLocalIp( NULL, NULL , 0 , ip , sizeof(ip) );
		
		DSCINIT_httpserver_conf( &httpserver_conf );
		
		strncpy( httpserver_conf.httpserver.server.ip , ip, sizeof(httpserver_conf.httpserver.server.ip)-1 );
		strncpy( httpserver_conf.httpserver.server.port , "10608", sizeof(httpserver_conf.httpserver.server.port)-1 );
		httpserver_conf.httpserver.server.maxChildProcessExitTime = DEFAULT_MAX_EXIT_TIME;
		strncpy( httpserver_conf.httpserver.server.restartWhen, "", sizeof(httpserver_conf.httpserver.server.restartWhen)-1 );

		httpserver_conf.httpserver.server.maxTaskcount = 100*10000;
		httpserver_conf.httpserver.server.taskTimeoutPercent = 95;
		httpserver_conf.httpserver.server.perfmsEnable = 1;
		httpserver_conf.httpserver.server.totalTimeout = 20;
		httpserver_conf.httpserver.server.keepAliveIdleTimeout= 300;
		httpserver_conf.httpserver.server.keepAliveMaxTime= 1800;
		httpserver_conf.httpserver.server.hangUpTimeout= 60;
		httpserver_conf.httpserver.server.showStatusInterval= 10;
		httpserver_conf.httpserver.server.processCount = 1;
		httpserver_conf.httpserver.server.listenBacklog = 1024;
		httpserver_conf.httpserver.server.maxHttpResponse = 1024;
		httpserver_conf.httpserver.server.epollThread = 2;

		httpserver_conf.httpserver.threadpool.minThreads = 10;
		httpserver_conf.httpserver.threadpool.maxThreads = 100;
		httpserver_conf.httpserver.threadpool.taskTimeout = 60;
		httpserver_conf.httpserver.threadpool.threadWaitTimeout = 2;
		httpserver_conf.httpserver.threadpool.threadSeed = 5;
		
		httpserver_conf.httpserver.database.port = 3306;
		httpserver_conf.httpserver.database.minConnections = 10;
		httpserver_conf.httpserver.database.maxConnections= 100;
		
		snprintf( httpserver_conf.httpserver.database.path , sizeof(httpserver_conf.httpserver.database.path)-1 , "%s/lib/libuhp_dbpool.so" , secure_getenv("HOME") );
		strncpy( httpserver_conf.httpserver.database.username, "mysqldb", sizeof(httpserver_conf.httpserver.database.username)-1 );
		strncpy( httpserver_conf.httpserver.database.password, "mysqldb", sizeof(httpserver_conf.httpserver.database.password)-1 );
		strncpy( httpserver_conf.httpserver.database.ip, ip, sizeof(httpserver_conf.httpserver.database.ip)-1 );
		strncpy( httpserver_conf.httpserver.database.dbname, "mysqldb", sizeof(httpserver_conf.httpserver.database.dbname)-1 );
		
		snprintf( httpserver_conf.httpserver.outputPlugins[0].path , sizeof(httpserver_conf.httpserver.outputPlugins[0].path)-1 , "%s/lib/libseqmysql_output.so" , secure_getenv("HOME") );
		strncpy( httpserver_conf.httpserver.outputPlugins[0].uri, "/sequence", sizeof(httpserver_conf.httpserver.outputPlugins[0].uri)-1 );
		strncpy( httpserver_conf.httpserver.outputPlugins[0].contentType, "application/json", sizeof(httpserver_conf.httpserver.outputPlugins[0].contentType)-1 );
		httpserver_conf.httpserver.outputPlugins[0].timeout = 60;
		
		httpserver_conf.httpserver.reserve.int1 = 1;
		httpserver_conf.httpserver.reserve.int2 = 4;
		
		if( p_env->cmd_param._rotate_size )
			strncpy( httpserver_conf.httpserver.log.rotate_size , p_env->cmd_param._rotate_size , sizeof(httpserver_conf.httpserver.log.rotate_size) );
		else
			strcpy( httpserver_conf.httpserver.log.rotate_size , "10MB" );
			
		if( p_env->cmd_param._main_loglevel )
			strncpy( httpserver_conf.httpserver.log.main_loglevel , p_env->cmd_param._main_loglevel , sizeof(httpserver_conf.httpserver.log.main_loglevel) );
		else
			strcpy( httpserver_conf.httpserver.log.main_loglevel , "INFO" );
		
		if( p_env->cmd_param._monitor_loglevel )
			strncpy( httpserver_conf.httpserver.log.monitor_loglevel , p_env->cmd_param._monitor_loglevel , sizeof(httpserver_conf.httpserver.log.monitor_loglevel) );
			strcpy( httpserver_conf.httpserver.log.monitor_loglevel , "INFO" );

		if( p_env->cmd_param._worker_loglevel )
			strncpy( httpserver_conf.httpserver.log.worker_loglevel , p_env->cmd_param._worker_loglevel , sizeof(httpserver_conf.httpserver.log.worker_loglevel) );
		else
			strcpy( httpserver_conf.httpserver.log.worker_loglevel , "ERROR" );
		

		nret = DSCSERIALIZE_JSON_DUP_httpserver_conf( & httpserver_conf , "GB18030" , & file_content , NULL , & file_len ) ;
		if( nret )
		{
			printf( "DSCSERIALIZE_JSON_DUP_httpserver_conf failed[%d]\n" , nret );
			return HTTP_INTERNAL_SERVER_ERROR;
		}
		
		nret = WriteEntireFile( pathfilename , file_content , file_len ) ;
		if( nret )
		{
			printf( "WriteEntireFile[%s] failed[%d]\n" , pathfilename , nret );
			free( file_content );
			return HTTP_INTERNAL_SERVER_ERROR;
		}
		
		free( file_content );
		
		printf( "%s created\n" , pathfilename );
	}
	
	return 0;
}

int LoadConfig( HttpserverEnv *p_env, httpserver_conf *p_conf )
{
	char		*file_content = NULL ;
	int		file_len ;
	int		nret = 0 ;
	
	/* 读取httpserver主配置文件 */
	file_content = StrdupEntireFile( p_env->server_conf_pathfilename , & file_len ) ;
	if( file_content == NULL )
	{
		ERRORLOGSG( "StrdupEntireFile[%s] failed" , p_env->server_conf_pathfilename );
		return -1;
	}
	
	/* 直接解析httpserver主配置 */
	nret = DSCDESERIALIZE_JSON_httpserver_conf( (char*)"GB18030" , file_content , & file_len , p_conf ) ;
	free( file_content );
	if( nret )
	{
		ERRORLOGSG( "DSCDESERIALIZE_JSON_httpserver_conf[%s] failed[%d]" , p_env->server_conf_pathfilename , nret );
		return -2;
	}
	
	if( p_env->httpserver_conf.httpserver.server.maxTaskcount <= 0 )
		p_env->httpserver_conf.httpserver.server.maxTaskcount = DEFAULT_MAX_TASK_COUNT;
	
	if( p_env->httpserver_conf.httpserver.server.maxChildProcessExitTime <= 0 )
		p_env->httpserver_conf.httpserver.server.maxChildProcessExitTime = DEFAULT_MAX_EXIT_TIME;
	
	if( p_env->httpserver_conf.httpserver.server.maxHttpResponse <= 0 )
		p_env->httpserver_conf.httpserver.server.maxHttpResponse = MAX_RESPONSE_BODY_LEN;
	
	if( p_env->httpserver_conf.httpserver.server.epollThread <= 0 )
		p_env->httpserver_conf.httpserver.server.epollThread = DEFAULT_EPOLL_THREAD;
	
	if( p_env->httpserver_conf.httpserver.server.hangUpTimeout <= 0 )
		p_env->httpserver_conf.httpserver.server.hangUpTimeout = DEFAULT_HANG_UP_TIMEOUT;
	
	INFOLOGSG( "Load config file[%s] ok" , p_env->server_conf_pathfilename );
	
	/*DSCLOG_httpserver_conf( & (p_env->httpserver_conf) );*/

	return 0;
}



