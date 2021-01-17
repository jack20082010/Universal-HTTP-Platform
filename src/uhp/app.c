/*
 * author	: jack 
 * email	: xulongzhong2004@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include <openssl/ssl.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include "uhp_in.h"
#include "uhp_api.h"

#define		MAX_SOURCEIP_LEN	30
/*tbmessage数据状态*/
#define TBMESAGE_STATUS_INIT		0
#define TBMESAGE_STATUS_PUBLISH		1

static int SetSessionResponse( struct AcceptedSession *p_session , int errcode, char *format , ...  ) 
{
	va_list			valist ;
	char			errmsg[ 1024 + 1 ];
	
	va_start( valist , format );
	memset( errmsg , 0x00 , sizeof(errmsg) );
	vsnprintf( errmsg , sizeof(errmsg)-1 , format , valist );
	va_end( valist );
	
	if( errcode == HTTP_OK )
		errcode = 0;
	
	snprintf( p_session->http_rsp_body, p_session->body_len -1, "{ \"errorCode\": \"%d\", \"errorMessage\": \"%s\" }", errcode, errmsg );
	
	return 0;
}

static char* ToUpperStr( char *str )
{
	size_t	i;
	size_t	len;
	
	len = strlen( str );
	for( i = 0; i < len; i++ )
	{
		str[i] = toupper( str[i] );
	}
	
	return str;
}

static int NSPIconv( char* fromcode, char* tocode, char *in, char *out, size_t outSize )
{
	int 	nret;
	iconv_t conv;
	size_t inLen;
	
	if( !fromcode || !tocode || !in || !out )
		return -1;
	
	inLen = strlen( in );
	if( strcmp( fromcode, tocode ) == 0 )
	{
		if( outSize <= inLen )
		{
			ERRORLOGSG( "outSize too small");
			return -1;
		}
		strncpy( out, in, inLen );
		
		return 0;
	}
	
	conv = iconv_open( tocode, fromcode );
	if( conv < 0 )
	{
		ERRORLOGSG( "iconv_open failed  errno[%d]", errno );
		return -1;
	}
	
	DEBUGLOGSG( "iconv in[%s] out[%s] fromcode[%s] tocode[%s] errno[%d]", in, out , fromcode, tocode, errno );
	nret = iconv( conv, &in, &inLen, &out, &outSize );
	if( nret < 0 )
	{
		ERRORLOGSG( "iconv failed in[%s] out[%s] fromcode[%s] tocode[%s] errno[%d]", in, out , fromcode, tocode, errno );
		iconv_close( conv );
		return -1;
	}
	
	iconv_close( conv );
	
	return 0;
}

int ConvertReponseBody( struct AcceptedSession *p_accepted_session, char* body_convert, int body_size )
{
	int 	nret;
	char	charset_upper[10+1];
	
	/*判断是转换为大写，但是不改变原值*/
	memset( charset_upper, 0, sizeof(charset_upper) ); 
	strncpy( charset_upper, p_accepted_session->charset, sizeof(charset_upper)-1 );
	ToUpperStr( charset_upper );

	nret = NSPIconv("GB18030", charset_upper, p_accepted_session->http_rsp_body, body_convert, body_size );
	if( nret )
	{
		ERRORLOGSG( "NSPIconv failed[%d] charset[%s] http_rsp_body[%s]" , nret, p_accepted_session->charset, p_accepted_session->http_rsp_body );
		SetSessionResponse( p_accepted_session, HTTP_NOT_ACCEPTABLE, HTTP_NOT_ACCEPTABLE_T );
		return -1;
	}
	
	return 0;
}


int ThreadBegin( void *arg, int threadno )
{
	threadinfo_t		*p_threadinfo = (threadinfo_t*)arg;
	HttpserverEnv 		*p_env = NULL;
	char			module_name[50];
	
	ResetAllHttpStatus();
	p_env = (HttpserverEnv*)threadpool_getReserver1( p_threadinfo->p_pool );
	if( !p_env )
		return -1;
	
	memset( module_name, 0, sizeof(module_name) );
	snprintf( module_name, 15,"Thread_%d", threadno ); 
	prctl( PR_SET_NAME, module_name );
	
	memset( module_name, 0, sizeof(module_name) );
	snprintf( module_name, sizeof(module_name) - 1, "worker_%d", g_process_index+1 );
	/* 设置日志环境 */
	InitLogEnv( p_env, module_name, SERVER_AFTER_LOADCONFIG );
	INFOLOGSG(" InitLogEnv module_name[%s] threadno[%d] ok", module_name, threadno );
	
	return 0;
}

int ThreadRunning( void *arg, int threadno )
{
	threadinfo_t		*p_threadinfo = (threadinfo_t*)arg;
	HttpserverEnv 	*p_env = NULL;
	
	p_env = (HttpserverEnv*)threadpool_getReserver1( p_threadinfo->p_pool );
	if( !p_env )
		return -1;
	
	if( p_threadinfo->cmd == 'L' ) /*线程内应用初始化*/
	{
		char	module_name[50];

		memset( module_name, 0, sizeof(module_name) );
		snprintf( module_name, 15,"Thread_%d", threadno ); 
		prctl( PR_SET_NAME, module_name );
		
		memset( module_name, 0, sizeof(module_name) );
		snprintf( module_name, sizeof(module_name) - 1, "worker_%d", g_process_index+1 );
		/* 设置日志环境 */
		InitLogEnv( p_env, module_name, SERVER_AFTER_LOADCONFIG );
		INFOLOGSG(" reload config module_name[%s] threadno[%d] ok", module_name, threadno );
		
		p_threadinfo->cmd = 0;
	}
	
	/*空闲时候输出日志*/
	if( p_threadinfo->status == THREADPOOL_THREAD_WAITED )
	{
		struct timeval	now_time;
		
		gettimeofday( &now_time, NULL );
		if( ( now_time.tv_sec - p_env->last_show_status_timestamp ) >= p_env->httpserver_conf.httpserver.server.showStatusInterval )
		{
			DEBUGLOGSG(" Thread Is Idling  threadno[%d] ", threadno );
		}
	}

	return 0;
}

int ThreadExit( void *arg, int threadno )
{
	threadinfo_t	*p_threadinfo = (threadinfo_t*)arg;
	HttpserverEnv *p_env = NULL;
	
	p_env = (HttpserverEnv*)threadpool_getReserver1( p_threadinfo->p_pool );
	if( !p_env )
		return -1;
	
	CleanLogEnv();
	p_threadinfo->cmd = 0;
	
	INFOLOGSG("Thread Exit threadno[%d] ", threadno );

	return 0;
}

static int GenerateHttpResponse( HttpserverEnv *p_env, struct AcceptedSession *p_accepted_session, BOOL bError, BOOL dbReturn )
{
	int 			status_code = HTTP_OK;
	int 			nret ;
	uint			connection_value;
	//struct timeval		*ptv_end = NULL;
	
	
	if( bError && p_accepted_session->http_rsp_body[0] != 0 )
	{
		char	body_convert[MAX_RESPONSE_BODY_LEN];
		
		memset( body_convert, 0, sizeof(body_convert) );
		status_code = HTTP_INTERNAL_SERVER_ERROR;
		nret = ConvertReponseBody( p_accepted_session, body_convert, sizeof(body_convert) );
		if( nret )
		{
			ERRORLOGSG( "ConvertReponseBody failed[%d]" , nret );
		}
		else
		{
			strncpy( p_accepted_session->http_rsp_body, body_convert, p_accepted_session->body_len-1 );
		}
		
	}
	else if( p_accepted_session->http_rsp_body[0] == 0 )
	{
		SetSessionResponse( p_accepted_session, 0 , "");
	}
	
	//DB onresponse
	if( dbReturn && p_env->p_fn_onresponse_dbpool )
	{
		nret = p_env->p_fn_onresponse_dbpool( p_accepted_session );
		if( nret )
		{
			ERRORLOGSG( "db onrequest failed[%d]" , nret );
			SetSessionResponse( p_accepted_session, -1 , "db onresponse failed");
		}
	}
	
	/*总时间超过默认配置30分钟，增加Connection:Close*/
	connection_value = HTTP_OPTIONS_FILL_CONNECTION_NONE;
	if( CheckHttpKeepAlive( p_accepted_session->http ) )
	{
		if( (  p_accepted_session->perfms.tv_publish_end.tv_sec - p_accepted_session->accept_begin_time ) > p_env->httpserver_conf.httpserver.server.keepAliveMaxTime )
			connection_value = HTTP_OPTIONS_FILL_CONNECTION_CLOSE;
		else 
			connection_value = HTTP_OPTIONS_FILL_CONNECTION_KEEPALIVE;
	}

	nret = FormatHttpResponseStartLine2( status_code , p_accepted_session->http, connection_value, 0,"Content-Type: %s;%s"
				HTTP_RETURN_NEWLINE "Content-length: %d" 
				HTTP_RETURN_NEWLINE HTTP_RETURN_NEWLINE "%s" ,
				HTTP_HEADER_CONTENT_TYPE_JSON , 
				p_accepted_session->charset,
				strlen( p_accepted_session->http_rsp_body ) ,
				p_accepted_session->http_rsp_body );
	if( nret )
	{
		ERRORLOGSG( "StrcatfHttpBuffer failed[%d]" , nret );
	} 
	
	if( p_accepted_session->needFree )
	{
		/*已经fd已经从epoll中删除，所以不会存在并发问题*/
		FreeSession( p_accepted_session );
	}
	else
	{
		nret = AddEpollSendEvent( p_env, p_accepted_session );
	}
	
	return 0;
}

int ThreadWorker( void *arg, int threadno )
{
	long 			cost_time;
	struct timeval		*ptv_start = NULL; 
	struct timeval		*ptv_end = NULL;
	
	struct AcceptedSession *p_accepted_session = (struct AcceptedSession*)arg;
	HttpserverEnv 	*p_env = UHPGetEnv();  //(HttpserverEnv*)p_accepted_session->p_env;
	int 			nret ;
	
	/*清空返回响应体，防止长连接请求，遗留上次响应内容*/
	memset( p_accepted_session->http_rsp_body, 0, p_accepted_session->body_len );
	ptv_start = &( p_accepted_session->perfms.tv_publish_begin );
	gettimeofday( ptv_start, NULL );
	
	//interceptors onrequest
	vecPluginInfo::iterator it = p_env->p_vec_interceptors->begin();
	while( it != p_env->p_vec_interceptors->end() )
	{
		nret = it->p_fn_onrequest( p_accepted_session );
		if( nret )
		{
			ERRORLOGSG( "db onrequest failed[%d]" , nret );
			SetSessionResponse( p_accepted_session, nret , "db onrequest failed");
			GenerateHttpResponse( p_env, p_accepted_session, TRUE, TRUE );
			return -1;
		}
		it++;
	}

	//process doworker
	nret = p_accepted_session->p_plugin->p_fn_doworker( p_accepted_session );
	if( nret )
	{
		ERRORLOGSG( "doworker failed path[%s] nret[%d]" , p_accepted_session->p_plugin->path.c_str(), nret );
		SetSessionResponse( p_accepted_session, nret , "doworker failed path[%s]" , p_accepted_session->p_plugin->path.c_str() );
		GenerateHttpResponse( p_env, p_accepted_session, TRUE, TRUE );
		return -1;
	}
	
	INFOLOGSG( "path[%s]插件Doworker调用成功", p_accepted_session->p_plugin->path.c_str() );
	
	//interceptors onresponse
	while( it != p_env->p_vec_interceptors->end() )
	{
		nret = it->p_fn_onresponse( p_accepted_session );
		if( nret )
		{
			ERRORLOGSG( "db onrequest failed[%d]" , nret );
		}
		it++;
	}
	
	ptv_end = &( p_accepted_session->perfms.tv_publish_end );
	gettimeofday( ptv_end, NULL );
	cost_time =( ptv_end->tv_sec - ptv_start->tv_sec ) * 1000*1000 +( ptv_end->tv_usec - ptv_start->tv_usec) ;
	INFOLOGSG("ThreadWorker threadno[%d] cost_time[%ld]us p_accepted_session[%p]",  threadno, cost_time, p_accepted_session );
	
	GenerateHttpResponse( p_env, p_accepted_session, FALSE, TRUE );
	
	return 0;
}

/*	
static int CheckHeadValid( HttpserverEnv *p_env , struct AcceptedSession *p_accepted_session, struct httpserver_httphead  *p_reqhead )
{	 
	return 0;
}
*/

static int OnProcessAddTask( HttpserverEnv *p_env , struct AcceptedSession *p_accepted_session )
{
	taskinfo_t	task ;
	int		nret = 0;
	
	/*添加任务前设置状态，表示线程还未被调度*/
	gettimeofday( &( p_accepted_session->perfms.tv_receive_end ), NULL ) ;
	p_accepted_session->status = SESSION_STATUS_PUBLISH;
	INFOLOGSG( "p_accepted_session[%p] fd[%d] receive end set SESSION_STATUS_PUBLISH" , p_accepted_session, p_accepted_session->netaddr.sock );
	
	memset( &task, 0, sizeof(taskinfo_t) );
	task.fn_callback = ThreadWorker;
	task.arg = p_accepted_session;
	task.timeout = p_accepted_session->p_plugin->timeout;
	nret = threadpool_addTask( p_env->p_threadpool, &task );
	if( nret < 0 )
	{
		ERRORLOGSG( "threadpool_addTask failed nret[%d]", nret);
		return -1;
	}
	INFOLOGSG( "threadpool_addTask ok nret[%d]", nret);
		
	return HTTP_OK;
}

static int GetCharset( struct AcceptedSession *p_accepted_session )
{
	char 	*p_begin = NULL;
	char 	*p_end = NULL;
	char	*p_content_type = NULL;
	int	value_len;
	size_t	 len;

	p_content_type = QueryHttpHeaderPtr( p_accepted_session->http , HTTP_HEADER_CONTENT_TYPE , & value_len );
	if( !p_content_type || value_len <= 0 )
        {
                ERRORLOGSG( "content_type[%.*s] error" , value_len , p_content_type );
		return -1;
        }
        INFOLOGSG("content_type[%.*s]" , value_len , p_content_type );
        
	p_begin = strchr( p_content_type, '=' );
	if( !p_begin )
	{
		//没有指定编码默认设置UTF-8
		strncpy( p_accepted_session->charset, "utf-8", sizeof(p_accepted_session->charset)-1 );
		return 0;
	}
		
	/*兼容windows是的换行符*/
	p_end = strstr( p_content_type, "\r\n" );
	if( !p_end )
	{
		p_end = strchr( p_content_type, '\n' );
		if( !p_end )
			return -1;
	}
	
	len = p_end - p_begin -1;
	if( len > ( sizeof(p_accepted_session->charset) -1 ) )
	{
		return -1;	
	}
	else
	{
		strncpy( p_accepted_session->charset, p_begin+1, len );
	}
	
	return 0;
}
static int CheckContentType( HttpserverEnv *p_env, struct AcceptedSession *p_accepted_session, char *type , int type_size)
{
	char		*p_content = NULL;
	int		value_len;

	p_content = QueryHttpHeaderPtr( p_accepted_session->http , HTTP_HEADER_CONTENT_TYPE , & value_len );
	if( ! MEMCMP( p_content , == , type , type_size -1 )  )
	{
		ERRORLOGSG( "content_type[%.*s] Is InCorrect" , value_len , p_content );
		SetSessionResponse( p_accepted_session, HTTP_BAD_REQUEST, "content_type[%.*s] Is InCorrect" , value_len , p_content );
		return HTTP_BAD_REQUEST;
	}
	
	return  HTTP_OK;
}
int OnProcess( HttpserverEnv *p_env , struct AcceptedSession *p_accepted_session )
{
	char			*p_uri = NULL;
	char			uri[65];
	int			uri_len;
	char			*method = NULL;
	int			method_len;

	int			nret = 0;
	
	/* 得到method 和URI */
	method = GetHttpHeaderPtr_METHOD( p_accepted_session->http , & method_len );
	p_uri = GetHttpHeaderPtr_URI( p_accepted_session->http , & uri_len );
	memset( uri, 0, sizeof(uri) );
	strncpy( uri, p_uri, uri_len );
	INFOLOGSG( "method[%.*s] uri[%.*s]" ,method_len , method ,  uri_len , uri );
	
	if( method_len == sizeof(HTTP_METHOD_POST)-1 && MEMCMP( method , == , HTTP_METHOD_POST , method_len ) )
	{
		char	*p_body = NULL;
		
		p_body = GetHttpBodyPtr( p_accepted_session->http, NULL );
		if( !p_body )
		{
			ERRORLOGSG( "p_body is null" );
			SetSessionResponse( p_accepted_session, HTTP_BAD_REQUEST, "http body is null" );
			return HTTP_BAD_REQUEST;
		}
		
		nret = GetCharset( p_accepted_session );
		if( nret )
		{
			ERRORLOGSG( "GetCharset error" );
			SetSessionResponse( p_accepted_session, HTTP_BAD_REQUEST, "GetCharset error" );
			return HTTP_BAD_REQUEST;
		}
		INFOLOGSG( "GetCharset[%s] uri[%s]", p_accepted_session->charset, uri );
		
		mapPluginInfo::iterator it = p_env->p_map_plugin_output->find( uri );
		if( it != p_env->p_map_plugin_output->end() )
		{	
			char 	*p_content = NULL;
			int	value_len = 0;
		
			p_accepted_session->p_plugin = &( it->second ) ;
			p_content = QueryHttpHeaderPtr( p_accepted_session->http , HTTP_HEADER_CONTENT_TYPE , & value_len );
			if( strncasecmp( p_content, p_accepted_session->p_plugin->content_type.c_str(), p_accepted_session->p_plugin->content_type.length() ) != 0 )
			{
				ERRORLOGSG( "content_type[%.*s] config_type[%s] is not match", value_len, p_content, p_accepted_session->p_plugin->content_type.c_str() );
				SetSessionResponse( p_accepted_session, HTTP_BAD_REQUEST, "content_type[%.*s] config_type[%s] is not match", value_len, p_content, p_accepted_session->p_plugin->content_type.c_str() );
				return HTTP_BAD_REQUEST;
			}
				
		}
		else
		{
			ERRORLOGSG( "URI[%s] not exist" , uri );
			SetSessionResponse( p_accepted_session, HTTP_BAD_REQUEST, "URI[%s] not exist" , uri );
			return HTTP_INTERNAL_SERVER_ERROR;
		}
			
		nret = OnProcessAddTask( p_env, p_accepted_session );
		if( nret != HTTP_OK )
		{
			ERRORLOGSG( "OnProcessAddTadk failed[%d]" , nret );
			return HTTP_INTERNAL_SERVER_ERROR;
		}
		INFOLOGSG( "OnProcessAddTadk ok" );
	}
	else if( method_len == sizeof(HTTP_METHOD_GET)-1 && MEMCMP( method , == , HTTP_METHOD_GET , method_len ) )
	{
		/* 显示所有已连接客户端会话 */
		if(  uri_len == sizeof(URI_SESSIONS)-1 && MEMCMP( uri , == , URI_SESSIONS , uri_len ) )
		{
			nret = CheckContentType( p_env, p_accepted_session, HTTP_HEADER_CONTENT_TYPE_TEXT, sizeof(HTTP_HEADER_CONTENT_TYPE_TEXT) );
			if( nret != HTTP_OK )
			{
				ERRORLOGSG( "CheckContentType failed[%d]" , nret );
				return nret;
			}
	
			nret = OnProcessShowSessions( p_env , p_accepted_session );
			if( nret != HTTP_OK )
			{
				ERRORLOGSG( "OnProcessShowSessions failed[%d]" , nret );
				return HTTP_INTERNAL_SERVER_ERROR;
			}
			else
			{
				INFOLOGSG( "OnProcessShowSessions ok" );
			}
		}
		/* 显示服务端线程状态信息 */
		else if(  uri_len == sizeof(URI_THREAD_STATUS)-1 && MEMCMP( uri , == , URI_THREAD_STATUS , uri_len ) )
		{
			nret = CheckContentType( p_env, p_accepted_session, HTTP_HEADER_CONTENT_TYPE_TEXT, sizeof(HTTP_HEADER_CONTENT_TYPE_TEXT) );
			if( nret != HTTP_OK )
			{
				ERRORLOGSG( "CheckContentType failed[%d]" , nret );
				return nret;
			}
	
			nret = OnProcessShowThreadStatus( p_env , p_accepted_session );
			if( nret != HTTP_OK )
			{
				ERRORLOGSG( "OnProcessShowThreadStatus failed[%d]" , nret );
				return HTTP_INTERNAL_SERVER_ERROR;
			}
			else
			{
				INFOLOGSG( "OnProcessShowThreadStatus ok" );
			}
		}
		/* 显示httpserver.conf配置信息 */
		else if( uri_len == sizeof(URI_CONFIG)-1 && MEMCMP( uri , == , URI_CONFIG , uri_len ) )
		{
			nret = CheckContentType( p_env, p_accepted_session, HTTP_HEADER_CONTENT_TYPE_TEXT, sizeof(HTTP_HEADER_CONTENT_TYPE_TEXT) );
			if( nret != HTTP_OK )
			{
				ERRORLOGSG( "CheckContentType failed[%d]" , nret );
				return nret;
			}
	
			nret = OnProcessShowConfig( p_env , p_accepted_session );
			if( nret != HTTP_OK )
			{
				ERRORLOGSG( "OnProcessShowConfig failed[%d]" , nret );
				return HTTP_INTERNAL_SERVER_ERROR;
			}
			else
			{
				INFOLOGSG( "OnProcessShowConfig ok" );
			}
		}
		else
		{	
			nret = OnProcessAddTask( p_env, p_accepted_session );
			if( nret != HTTP_OK )
			{
				ERRORLOGSG( "OnProcessAddTadk failed[%d]" , nret );
				return HTTP_INTERNAL_SERVER_ERROR;
			}
			INFOLOGSG( "OnProcessAddTadk ok" );
		}
	
	}
	else
	{
		ERRORLOGSG( "method unknown method[%.*s] " ,method_len , method  );  
		SetSessionResponse( p_accepted_session, HTTP_BAD_REQUEST, "method unknown method[%.*s] " ,method_len , method  );
	}
	
	return HTTP_OK;
}


static int AddResponseData( HttpserverEnv *p_env, struct AcceptedSession *p_accepted_session, struct HttpBuffer *buf, char *content_type )
{
	struct HttpBuffer	*rsp_buf = NULL;
	
	int			nret = 0;
	
	rsp_buf = GetHttpResponseBuffer( p_accepted_session->http );
	nret = StrcatfHttpBuffer( rsp_buf , "Content-Type: %s" HTTP_RETURN_NEWLINE 
			"Content-length: %d"
			HTTP_RETURN_NEWLINE HTTP_RETURN_NEWLINE "%.*s" , 
			content_type , 
			GetHttpBufferLength(buf) , 
			GetHttpBufferLength(buf) , GetHttpBufferBase(buf,NULL) );
	if( nret )
	{
		ERRORLOGSG( "StrcatfHttpBuffer failed[%d] , errno[%d]" , nret , errno );
		return HTTP_INTERNAL_SERVER_ERROR;
	}
	
	INFOLOGSG( "SEND RESPONSE HTTP [%.*s]" , GetHttpBufferLength(rsp_buf) , GetHttpBufferBase(rsp_buf,NULL) );

	nret = AddEpollSendEvent( p_env, p_accepted_session );
	if( nret )
		return nret;
	
	return 0;
}
int OnProcessShowSessions( HttpserverEnv *p_env , struct AcceptedSession *p_accepted_session )
{
	struct HttpBuffer	*buf = NULL;
	struct AcceptedSession	*p_session = NULL;
	
	int			nret = 0;
		
	buf = AllocHttpBuffer( 4096 );
	if( buf == NULL )
	{
		ERRORLOGSG( "AllocHttpBuffer failed[%d] , errno[%d]" , nret , errno );
		return HTTP_INTERNAL_SERVER_ERROR;
	}
	
	for( int i = 0; i < p_env->httpserver_conf.httpserver.server.epollThread; i++ )
	{
		setSession::iterator it = p_env->thread_epoll[i].p_set_session->begin();
		while( it != p_env->thread_epoll[i].p_set_session->end() )
		{
			p_session = *it;
			nret = StrcatfHttpBuffer( buf , "%s %d \n" , p_session->netaddr.remote_ip , p_session->netaddr.remote_port );
			if( nret )
			{
				ERRORLOGSG( "StrcatfHttpBuffer failed[%d] , errno[%d]" , nret , errno );
				FreeHttpBuffer( buf );
				return HTTP_INTERNAL_SERVER_ERROR;
			}
			it++;
		}
	}

	nret = AddResponseData( p_env, p_accepted_session, buf, HTTP_HEADER_CONTENT_TYPE_TEXT );
	FreeHttpBuffer( buf );
	if( nret )
	{
		return HTTP_INTERNAL_SERVER_ERROR;
	}

	return HTTP_OK;
}

int OnProcessShowThreadStatus( HttpserverEnv *p_env , struct AcceptedSession *p_accepted_session )
{
	struct HttpBuffer	*buf = NULL;
	struct ProcStatus 	*p_status = NULL;
	int			i;
	struct tm		tm_restarted;
	char			last_restarted_time[20+1];
	
	int			nret = 0;
	
	buf = AllocHttpBuffer( 4096 );
	if( buf == NULL )
	{
		ERRORLOGSG( "AllocHttpBuffer failed[%d] , errno[%d]" , nret , errno );
		return HTTP_INTERNAL_SERVER_ERROR;
	}

	p_status = (struct ProcStatus*)(p_env->p_shmPtr);
	for( i = 0; i < p_env->httpserver_conf.httpserver.server.processCount; i++, p_status++ )
	{
		localtime_r( &( p_status->last_restarted_time ), &tm_restarted );
		memset( last_restarted_time, 0, sizeof(last_restarted_time) );
		strftime( last_restarted_time, sizeof(last_restarted_time), "%Y-%m-%d %H:%M:%S", &tm_restarted );
		
		nret = StrcatfHttpBuffer( buf , "process[%d] "
						"startTime[%s] "
						"task[%d] "
						"working[%d] "
						"idle[%d] "
						"thread[%d] "
						"timeout[%d] "
						"min[%d] "
						"max[%d] "
						"percent[%d] "
						"session[%d]\n"
						 , i+1
						 , last_restarted_time
						 , p_status->task_count
						 , p_status->working_count
						 , p_status->Idle_count 
						 , p_status->thread_count	
						 , p_status->timeout_count
						 , p_status->min_threads
						 , p_status->max_threads
						 , p_status->taskTimeoutPercent
						 , p_status->session_count );
		if( nret )
		{
			ERRORLOGSG( "StrcatfHttpBuffer failed[%d] , errno[%d]" , nret , errno );
			FreeHttpBuffer( buf );
			return HTTP_INTERNAL_SERVER_ERROR;
		}
	}
	
	nret = AddResponseData( p_env, p_accepted_session, buf, HTTP_HEADER_CONTENT_TYPE_TEXT );
	FreeHttpBuffer( buf );
	if( nret )
	{
		return HTTP_INTERNAL_SERVER_ERROR;
	}
	
	return HTTP_OK;
}

int OnProcessShowConfig( HttpserverEnv *p_env , struct AcceptedSession *p_accepted_session )
{
	struct HttpBuffer	*buf = NULL;
	char			*file_content = NULL;
	int			file_len = 0;
	
	int			nret = 0;
	
	buf = AllocHttpBuffer( 4096 );
	if( buf == NULL )
	{
		ERRORLOGSG( "AllocHttpBuffer failed[%d] , errno[%d]" , nret , errno );
		return HTTP_INTERNAL_SERVER_ERROR;
	}
	
	/* 读取httpserver.conf主配置文件 */
	file_content = StrdupEntireFile( p_env->server_conf_pathfilename , & file_len ) ;
	if( file_content == NULL )
	{
		ERRORLOGSG( "StrdupEntireFile[%s] failed , nret[%d]\n" , p_env->server_conf_pathfilename , errno );
		FreeHttpBuffer( buf );
		return HTTP_INTERNAL_SERVER_ERROR;
	}

	nret = StrcatfHttpBuffer( buf , "%s" , file_content );
	if( nret )
	{
		ERRORLOGSG( "StrcatfHttpBuffer failed[%d] , errno[%d]" , nret , errno );
		FreeHttpBuffer( buf );
		return HTTP_INTERNAL_SERVER_ERROR;
	}

	free(file_content);
	
	nret = AddResponseData( p_env, p_accepted_session, buf, HTTP_HEADER_CONTENT_TYPE_TEXT );
	FreeHttpBuffer( buf );
	if( nret )
	{
		return HTTP_INTERNAL_SERVER_ERROR;
	}

	return HTTP_OK;
}

int OnProcessPipeEvent( HttpserverEnv *p_env , struct PipeSession *p_pipe_session )
{
	char		ch ;
	httpserver_conf	httpserver_conf ;
	char 		module_name[50];
	
	int		nret = 0 ;

	nret = (int)read( p_pipe_session->pipe_fds[0] , & ch , 1 );
	if( nret == -1 )
	{
		ERRORLOGSG( "read pipe failed[%d] , errno[%d]" , nret , errno );
		return -2;
	}
	INFOLOGSG( "read pipe ok , ch[%c]" , ch );
	
	memset( & httpserver_conf , 0x00 , sizeof(httpserver_conf) );
	nret = LoadConfig( p_env , &httpserver_conf );
	if( nret )
	{
		ERRORLOGSG( "LoadConfig error" );
		return -1;
	}
	
	memcpy( &p_env->httpserver_conf , &httpserver_conf , sizeof(httpserver_conf) );
	
	/* 设置日志环境 */
	memset( module_name, 0, sizeof(module_name) );
	snprintf( module_name, sizeof(module_name)-1, "worker_%d", g_process_index+1 );
	nret = InitLogEnv( p_env, module_name, SERVER_AFTER_LOADCONFIG );
	if( nret )
	{
		// printf( "InitLogEnv faild[%d]\n" , nret );
		return -1;
	}
		
	if( p_env->p_threadpool )
	{
		nret = threadpool_setThreads( p_env->p_threadpool, p_env->httpserver_conf.httpserver.threadpool.minThreads, p_env->httpserver_conf.httpserver.threadpool.maxThreads );
		if( nret == THREADPOOL_FORBIT_DIMINISH )
		{
			ERRORLOGSG("最大线程内存正在使用中,不允许减少,请人工重启服务使配置生效!");
			return -1;
		}	
		threadpool_setTaskTimeout( p_env->p_threadpool, p_env->httpserver_conf.httpserver.threadpool.taskTimeout );
		threadpool_setThreadSeed( p_env->p_threadpool, p_env->httpserver_conf.httpserver.threadpool.threadSeed );
	}
	
	INFOLOGSG("reload_config ok!");
	
	/*设置子线程重载日志命令，由个线程自行进行重载*/
	p_env->thread_accept.cmd = 'L';
	for( int i = 0; i < p_env->httpserver_conf.httpserver.server.epollThread; i++ )
	{
		pthread_join( p_env->thread_epoll[i].thread_id, NULL );
		p_env->thread_epoll[i].cmd = 'L';
	}
	
	threadpool_setThreadsCommand( p_env->p_threadpool, 'L');
	INFOLOGSG( "threadpool_setThreads_command ok" );
	
	return 0 ;
}

/*导出性能日志*/
int ExportPerfmsFile( HttpserverEnv *p_env , struct AcceptedSession *p_accepted_session )
{
	FILE			*fp = NULL;
	char			path_file[256];
	size_t			size;
	
	if( p_env->httpserver_conf.httpserver.server.perfmsEnable > 0 )
	{
		memset( path_file, 0, sizeof(path_file) );
		snprintf( path_file, sizeof(path_file)-1, "%s/log/httpserver_perfms.log", secure_getenv("HOME") );
		fp = fopen( path_file, "ab" );
		if( !fp )
		{
			ERRORLOGSG( "fopen failed path_file[%s]", path_file );
			return -1;
		}
		
		size = fwrite( &( p_accepted_session->perfms ), sizeof(struct Performamce), 1, fp );
		if( size != 1 )
		{
			ERRORLOGSG( "fwrite error nret[%u]", size );
			fclose( fp );
			return -1;
		}
		
		fclose( fp );
	}
	
	return 0;
}

void FreeSession( struct AcceptedSession *p_accepted_session )
{
	INFOLOGSG( "close fd[%d] [%s:%d]->[%s:%d] p_accepted_session[%p] status[%d]" , p_accepted_session->netaddr.sock , p_accepted_session->netaddr.remote_ip , p_accepted_session->netaddr.remote_port , p_accepted_session->netaddr.local_ip , p_accepted_session->netaddr.local_port , p_accepted_session,p_accepted_session->status );
	DestroyHttpEnv( p_accepted_session->http );
	close( p_accepted_session->netaddr.sock );
	
	if( p_accepted_session->http_rsp_body )
		free( p_accepted_session->http_rsp_body );
	free( p_accepted_session );
	
	return;
}

int AddEpollSendEvent(struct HttpserverEnv *p_env , struct AcceptedSession *p_accepted_session )
{
	struct epoll_event	event ;
	int 			nret ;
	
	/*******注意必须先删除然后再加入后续队列的顺序关系，否则会造成事件的并发处理问题*********/
	nret = epoll_ctl( p_accepted_session->epoll_fd , EPOLL_CTL_DEL , p_accepted_session->netaddr.sock , NULL );
	if( nret == -1 )
	{
		ERRORLOGSG( "epoll_ctl_recv[%d] del accepted_session[%d] EPOLLIN failed , errno[%d]" , p_accepted_session->epoll_fd , p_accepted_session->netaddr.sock , errno );
		return 1;
	}
	DEBUGLOGSG( "epoll_ctl_recv[%d] del accepted_session[%d] EPOLLIN ok" , p_accepted_session->epoll_fd , p_accepted_session->netaddr.sock );
	
	/* 加入到发送epoll事件 */
	memset( & event , 0x00 , sizeof(struct epoll_event) );
	event.events = EPOLLOUT | EPOLLERR ;
	event.data.ptr = p_accepted_session ;
	nret = epoll_ctl( p_accepted_session->epoll_fd_send , EPOLL_CTL_ADD , p_accepted_session->netaddr.sock , & event ) ;
	if( nret == -1 )
	{
		ERRORLOGSG( "epoll_ctl_send[%d] add accepted_session[%d] EPOLLIN failed , errno[%d]" , p_accepted_session->epoll_fd_send , p_accepted_session->netaddr.sock , errno );
		return 1;
	}
	DEBUGLOGSG( "epoll_ctl_send[%d] add accepted_session[%d] EPOLLIN ok" , p_accepted_session->epoll_fd_send , p_accepted_session->netaddr.sock );
	
	return 0;
}

int AddEpollRecvEvent( struct HttpserverEnv *p_env , struct AcceptedSession *p_accepted_session )
{
	struct epoll_event	event ;
	int 			nret ;
	
	/*******注意必须先删除然后再加入后续队列的顺序关系，否则会造成事件的并发处理问题*********/
	nret = epoll_ctl( p_accepted_session->epoll_fd_send , EPOLL_CTL_DEL , p_accepted_session->netaddr.sock , NULL );
	if( nret == -1 )
	{
		ERRORLOGSG( "epoll_ctl_send[%d] del accepted_session[%d] EPOLLIN failed , errno[%d]" , p_accepted_session->epoll_fd_send , p_accepted_session->netaddr.sock , errno );
		return 1;
	}	
	DEBUGLOGSG( "epoll_ctl_send[%d] del accepted_session[%d] EPOLLIN ok" , p_accepted_session->epoll_fd_send , p_accepted_session->netaddr.sock );
	
	/* 加入到发送epoll事件 */
	memset( & event , 0x00 , sizeof(struct epoll_event) );
	event.events = EPOLLIN | EPOLLERR ;
	event.data.ptr = p_accepted_session ;
	nret = epoll_ctl( p_accepted_session->epoll_fd , EPOLL_CTL_ADD , p_accepted_session->netaddr.sock , & event ) ;
	if( nret == -1 )
	{
		ERRORLOGSG( "epoll_ctl_recv[%d] add accepted_session[%d] EPOLLIN failed , errno[%d]" , p_accepted_session->epoll_fd , p_accepted_session->netaddr.sock , errno );
		return 1;
	}
	DEBUGLOGSG( "epoll_ctl_recv[%d] add accepted_session[%d] EPOLLIN ok" , p_accepted_session->epoll_fd , p_accepted_session->netaddr.sock );
	
	return 0;
}
