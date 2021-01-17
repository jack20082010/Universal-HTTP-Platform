/*
 * 
 * author	: jack
 * email	: xulongzhong2004@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "seq_sdk_in.h"
#include "seq_sdk.h"

Cplugin* Cplugin::_this = NULL;

int ThreadExit( void *arg, int threadno );
int ThreadBegin( void *arg, int threadno );
int ThreadRunning( void *arg, int threadno );

#define TCP_NONBLOCK_CONNECT_TIMEOUT	4
#define MAX_HTTP_REQUEST_TIMEOUT	60
#define	FULL_FUSING_TIME		10	/*所有机器全熔断时间*/
#define FUSING_TIME			60	/*当台机器熔断时间*/

Cplugin::Cplugin()
{
	int nret;
	
	InitLogEnv();
        m_threadpool = threadpool_create( 1, 1 );
	if( !m_threadpool )
	{
		ERRORLOGSG("threadpool_create error ");
		return;
	}
	
	threadpool_setReserver1( m_threadpool, this );
	threadpool_setBeginCallback( m_threadpool, &ThreadBegin );
	threadpool_setRunningCallback( m_threadpool, &ThreadRunning );
	threadpool_setExitCallback( m_threadpool, &ThreadExit );
	
	nret = threadpool_start( m_threadpool );
        if( nret )
        {
                ERRORLOGSG("threadpool_start error ");
                return ;
        }	
	
	m_connect_timeout = TCP_NONBLOCK_CONNECT_TIMEOUT;
	m_timeout = MAX_HTTP_REQUEST_TIMEOUT;
	m_fusing_time = FUSING_TIME;
	m_full_fusing_time = FULL_FUSING_TIME;
}

Cplugin::~Cplugin()
{
	if( m_threadpool )
		threadpool_destroy( m_threadpool );
	
	CleanLogEnv();
}

int ThreadBegin( void *arg, int threadno )
{
	char			module_name[50];
	
	memset( module_name, 0, sizeof(module_name) );
	snprintf( module_name, 15, "PluginThread_%d", threadno ); 
	prctl( PR_SET_NAME, module_name );
	
	/* 设置日志环境 */
	Cplugin::Instance()->InitLogEnv();
	INFOLOGSG(" InitPluginLogEnv module_name[%s] threadno[%d] ok", module_name, threadno );
	
	return 0;
}

int ThreadRunning( void *arg, int threadno )
{
	threadinfo_t		*p_threadinfo = (threadinfo_t*)arg;

	if( p_threadinfo->cmd == 'L' ) /*线程内应用初始化*/
	{
		char	module_name[50];

		memset( module_name, 0, sizeof(module_name) );
		snprintf( module_name, 15, "uhp_sdk_%d", threadno ); 
		prctl( PR_SET_NAME, module_name );
		
		/* 设置日志环境 */
		Cplugin::Instance()->InitLogEnv();
		INFOLOGSG(" Plugin reload config module_name[%s] threadno[%d] ok", module_name, threadno );
		
		p_threadinfo->cmd = 0;
	}
	
	/*空闲时候输出日志*/
	if( p_threadinfo->status == THREADPOOL_THREAD_WAITED )
	{
		struct timeval	now_time;
		
		gettimeofday( &now_time, NULL );
		//if( ( now_time.tv_sec - p_env->last_show_status_timestamp ) >= p_env->httpserver_conf.httpserver.server.showStatusInterval )
		{
			INFOLOGSG(" Plugin Thread Is Idling  threadno[%d] ", threadno );
		}
	}

	return 0;
}

int ThreadExit( void *arg, int threadno )
{
	threadinfo_t	*p_threadinfo = (threadinfo_t*)arg;
	
	Cplugin::Instance()->CleanLogEnv();
	p_threadinfo->cmd = 0;
	INFOLOGSG("Plugin Thread Exit threadno[%d] ", threadno );

	return 0;
}

Cplugin* Cplugin::Instance()
{
	if (_this != NULL)
	{
		return _this;
	}

	_this = new Cplugin;

	return _this;
}

int Cplugin::Uninstance()
{
	if (_this != NULL)
	{
		delete _this;
		_this = NULL;
	}
	
	return 0;

}

int Cplugin::ConnectNonBlock( struct NetAddress *p_netaddr, int connect_timeout )
{
	int			nret = 0 ;
	
	p_netaddr->sock = socket( AF_INET , SOCK_STREAM , IPPROTO_TCP ) ;
	if( p_netaddr->sock == -1 )
	{
		ERRORLOGSG( "socket failed , errno[%d]" , errno );
		return -1;
	}
	
	SetHttpNodelay( p_netaddr->sock , 1 );
	SetHttpNonblock( p_netaddr->sock );
	SETNETADDRESS( (*p_netaddr) )
	
	nret = connect( p_netaddr->sock , (struct sockaddr *)&( p_netaddr->addr ) , sizeof(struct sockaddr) ) ;
	if( nret == -1 )
	{
		int		addr_len ;
		int		error ;
		int		code ;
			
#ifdef _WIN32 

		if( errcode == WSAEWOULDBLOCK || errcode == WSAEALREADY)
		{
			fd_set		read_fds ;
			fd_set		write_fds ;
			struct timeval	timeout ;
			
			FD_ZERO( & read_fds );
			FD_ZERO( & write_fds );
			FD_SET( p_netaddr->sock , & read_fds );
			FD_SET( p_netaddr->sock , & write_fds );
			
			timeout.tv_sec = connect_timeout ;
			timeout.tv_usec = 0 ;
			INFOLOGSG( "set timeout[%d] seconds" , timeout.tv_sec );
			
			nret = select( p_netaddr->sock + 1 , & read_fds , & write_fds , NULL , & timeout ) ;
			if( nret == -1 )
			{
				/* 关闭客户端socket */
				closesocket( p_netaddr->sock );
				p_netaddr->sock = -1;
				ERRORLOGSG( "socket  select failed , errno[%d]" , errno );
				return -1;
			}
			else if( nret == 0 )
			{
				closesocket( p_netaddr->sock );
				p_netaddr->sock = -1;
				INFOLOGSG( "select[%s:%d] timeout" , p_netaddr->ip , p_netaddr->port );
				return -2;
			}
		
			if( FD_ISSET( p_netaddr->sock , &read_fds ) || FD_ISSET( p_netaddr->sock , &write_fds ) )
			{
				addr_len = sizeof(int) ;
				code = getsockopt( p_netaddr->sock , SOL_SOCKET , SO_ERROR , & error , & addr_len ) ;
				if( code < 0 || ( code == 0 && error ))
				{
					closesocket( p_netaddr->sock );
					p_netaddr->sock = -1;
					ERRORLOGSG( "connect[%s:%d] failed , errno[%d]" , _netaddr->ip , _netaddr->port , errno );
					return -2;
				}		
			}
		}
#else
		if( ERRNO == EINPROGRESS )
		{
			struct pollfd 		poll_fd ;
			int			poll_timeout ;
			
			poll_timeout = connect_timeout * 1000 ;			
			poll_fd.fd = p_netaddr->sock ; 
			poll_fd.events = POLLOUT ;
			
			nret = poll( &poll_fd , 1 , poll_timeout );
			if( nret == -1 )
			{
				/* 关闭客户端socket */
				close( p_netaddr->sock );
				p_netaddr->sock = -1;
				ERRORLOGSG( "poll  select failed , errno[%d]" , errno );
				return -1;
			}
			else if( nret == 0 )
			{
				close( p_netaddr->sock );
				p_netaddr->sock = -1;
				INFOLOGSG( "poll[%s:%d] timeout" , p_netaddr->ip , p_netaddr->port );
				return -2;
			}
		
			if(  poll_fd.revents & ( POLLOUT | POLLERR ) )
			{
				addr_len = sizeof(int) ;
				code = getsockopt( p_netaddr->sock , SOL_SOCKET , SO_ERROR , & error , (socklen_t*)&addr_len ) ;
				if( code < 0 || ( code == 0 && error ))
				{
					close( p_netaddr->sock );
					p_netaddr->sock = -1;
					ERRORLOGSG( "connect[%s:%d] failed , errno[%d]" , p_netaddr->ip , p_netaddr->port , errno );
					return -2;
				}		
			}
		}
#endif
		else
		{
			close( p_netaddr->sock );
			p_netaddr->sock = -1;
			ERRORLOGSG( "connect[%s:%d] failed , errno[%d]" , p_netaddr->ip , p_netaddr->port , errno );
			return -2;
		}
	}
	
	SetHttpBlock( p_netaddr->sock );

	return 0;
}

int Cplugin::RequestAndResponseV( struct NetAddress *p_netaddr , struct HttpEnv *http , int timeout , char *format , va_list valist )
{
	struct HttpBuffer	*b = NULL ;
	int			nret = 0;

	ResetHttpEnv( http );
	
	b = GetHttpRequestBuffer( http ) ;
	nret = StrcatvHttpBuffer( b , format , valist ) ;
	if( nret )
	{
		ERRORLOGSG( "StrcatvHttpBuffer failed[%d]" , nret );
		return nret;
	}
	
	DEBUGHEXLOGSG( GetHttpBufferBase(b,NULL) , GetHttpBufferLength(b) , "RequestBuffer [%d]bytes" , GetHttpBufferLength(b));
	
	SetHttpTimeout( http , timeout );
	
	INFOLOGSG( "begin SendHttpRequest" );
	nret = SendHttpRequest( p_netaddr->sock , NULL , http ) ;
	if( nret )
	{
		ERRORLOGSG( "SendHttpRequest failed[%d]" , nret );
		return nret;
	}
	
	nret = ReceiveHttpResponse( p_netaddr->sock , NULL , http ) ;
	if( nret )
	{
		ERRORLOGSG( "ReceiveHttpResponse failed[%d]" , nret );
		return nret;
	}
	
	b = GetHttpResponseBuffer( http ) ;
	DEBUGHEXLOGSG( GetHttpBufferBase(b,NULL) , GetHttpBufferLength(b) , "ResponseBuffer [%d]bytes" , GetHttpBufferLength(b));
	
	return 0;
}

struct HostInfo* Cplugin:: SelectValidHost( )
{
	struct timeval 	tv;
	int		rand_mod = -1;
	size_t		i;
	vector<HostInfo*> vector_host;
	
	/*设立随机因子*/
	gettimeofday( &tv , NULL );
	srand( tv.tv_sec * tv.tv_usec );
	
	for( i = 0; i < m_vector_host.size() ; i++ )
	{
		//printf(" ip[%s] port[%d]\n", m_vector_host[i].ip.c_str(), m_vector_host[i].port);
		if( m_vector_host[i].port > 0 && m_vector_host[i].activeTimestamp <= tv.tv_sec )
		{
			HostInfo    *p_host = NULL;
			p_host = &(m_vector_host[i]);
			vector_host.push_back( p_host );
		}
	}
	
	if( vector_host.size() == 0 )
	{
		/*全熔断时重置时间为熔断配置时间*/
		for( i = 0; i < m_vector_host.size() ; i++ )
		{
			if( m_vector_host[i].port > 0 && tv.tv_sec - m_vector_host[i].activeTimestamp > m_full_fusing_time  )
			{
				m_vector_host[i].activeTimestamp = tv.tv_sec + m_full_fusing_time  ;
			}
		}
		ERRORLOGSG( "选择主机失败, 当前没有可用的主机" );
		return NULL;
	}
	
	rand_mod = rand()% vector_host.size() ;	
	HostInfo *p_host = vector_host[rand_mod];
	INFOLOGSG( "select validHosts[%d] IP[%s]:[%d] activeTimestamp[%ld]" , vector_host.size(), p_host->ip.c_str(), p_host->port, p_host->activeTimestamp );
	
	return p_host;
}

int Cplugin:: HttpRequestFormat( struct HttpEnv	*p_http_env, char *format , ... )
{
	va_list		valist ;
	int		nret = 0 ;
	HostInfo	*p_host = NULL; 
	struct timeval	  tv_now;
	struct NetAddress netaddr;

RETRY_SELECT_HOST:
	
	gettimeofday( &tv_now, NULL );
	p_host = SelectValidHost( );
	if( !p_host  )
	{
		return -1;
	}
	
	strncpy( netaddr.ip, p_host->ip.c_str(), sizeof(netaddr.ip) -1 );
	netaddr.port = p_host->port;
	
	nret = ConnectNonBlock( &netaddr , m_connect_timeout );
	if( nret == -2 )
	{
		INFOLOGSG( "当前主机连接失败,进行重新转连" );
		p_host->activeTimestamp = tv_now.tv_sec + m_fusing_time;
		goto RETRY_SELECT_HOST;
	}
	else if( nret )
	{
		return nret;
	}
	INFOLOGSG( "AF_INET connect ok host IP[%s]:[%d]" , p_host->ip.c_str(), p_host->port, p_host->activeTimestamp );
	
	va_start( valist , format );
	nret = RequestAndResponseV( &netaddr , p_http_env , m_timeout , format , valist ) ;
	va_end( valist );
	Disconnect( &netaddr );
	
	if( nret == FASTERHTTP_ERROR_TCP_SELECT_SEND_TIMEOUT
		|| nret == FASTERHTTP_ERROR_TCP_SELECT_RECEIVE_TIMEOUT 
		|| nret == FASTERHTTP_ERROR_TCP_CLOSE )
	{
		if ( nret == FASTERHTTP_ERROR_TCP_SELECT_SEND_TIMEOUT )
		{
			WARNLOGSG( "发送超时，进行重新转连" );
		}
		if ( nret == FASTERHTTP_ERROR_TCP_SELECT_RECEIVE_TIMEOUT )
		{
			WARNLOGSG( "通讯接收超时，进行重新转连" );
		}
		else if( nret == FASTERHTTP_ERROR_TCP_CLOSE )
		{
			WARNLOGSG( "通讯连接异常断开，进行重新转连" );
		}
		
		p_host->activeTimestamp = tv_now.tv_sec + m_fusing_time;
		goto RETRY_SELECT_HOST;
	}
		
	return nret;
}

int Cplugin::HttpRequest( char* seq_name, int count, UhpResponse *p_response )
{		
	char			*statuscode = NULL ;
	int			statuscode_len ;
	char			*body = NULL ;
	int			body_len = 0;
	struct HttpEnv	*p_http_env = NULL;
	
	struct timeval		tm_time;
	UhpResponse		response;
	struct timeval		tm_end;
	float			cost_time;
	char			req_json[100];
	int			nret = 0 ;

	/* 创建HTTP环境 */
	p_http_env = CreateHttpEnv() ;
	if( p_http_env == NULL )
	{
		ERRORLOGSG( "CreateHttpEnv failed , errno[%d]" , errno );
		return -1;
	}
	
	memset( req_json, 0, sizeof(req_json) );
	snprintf( req_json, sizeof(req_json)-1, "{ \"name\": \"%s\", \"count\": %d }", seq_name, count );
	
	gettimeofday( &tm_time , NULL );
	nret = HttpRequestFormat( p_http_env, "POST %s  HTTP/1.0" HTTP_RETURN_NEWLINE
					"%s: %s" HTTP_RETURN_NEWLINE
					HTTP_HEADER_CONTENT_LENGTH": %d" HTTP_RETURN_NEWLINE
					HTTP_RETURN_NEWLINE
					"%s"
					,URI_PRODUCER
					,HTTP_HEADER_CONTENT_TYPE , HTTP_HEADER_CONTENT_TYPE_JSON
					,strlen(req_json)
					,req_json );
	
	if( nret )
	{
		ERRORLOGSG( "HttpRequestFormat failed, nret[%d]" , nret );
		DestroyHttpEnv( p_http_env );
		return nret;
	}
	
	statuscode = GetHttpHeaderPtr_STATUSCODE( p_http_env , & statuscode_len ) ;
	if( !statuscode || statuscode_len <= 0 )
	{
		ERRORLOGSG( "GetHttpHeaderPtr_STATUSCODE failed statuscode is null" );
		DestroyHttpEnv( p_http_env );
		return -1;	
		
	}
	
	body = GetHttpBodyPtr( p_http_env , & body_len );
	if( !body || body_len <= 0 )
	{
		ERRORLOGSG( "GetHttpBodyPtr failed body is null" );
		DestroyHttpEnv( p_http_env );
		return -1;	
		
	}

	nret = StringNToInt( statuscode, statuscode_len ) ;
	if( nret != HTTP_OK )
	{
		ERRORLOGSG( "statuscode[%.*s] body[%.*s]" , statuscode_len , statuscode, body_len, body );
		DestroyHttpEnv( p_http_env );
		return nret;
	}
	
	nret = DSCDESERIALIZE_JSON_UhpResponse( "GB18030" , body, &body_len , p_response );
	if( nret )
	{
		ERRORLOGSG( "JSON response unpack failed body[%.*s]" , body_len, body );
		DestroyHttpEnv( p_http_env );
		return -1;
	}
	
	gettimeofday( &tm_end , NULL );
	cost_time = (tm_end.tv_sec - tm_time.tv_sec)*1.0 + (tm_end.tv_usec - tm_time.tv_usec)/1000000.0;
	
	nret = atoi( response.errorCode );
	if( nret == HTTP_OK)
		nret = 0;
	if( nret )
	{
		ERRORLOGSG( "发送失败 costTime[%.3f]s body[%.*s]" , cost_time, body_len, body );
	}
	else
	{
		INFOLOGSG( "发送成功 costTime [%.3f]s, nret[%d]", cost_time, nret );
	}
	DestroyHttpEnv( p_http_env );
	
	return nret;
}

int Cplugin::SetConnectTimeout( int timeout )
{
	if( timeout <= 0 )
		return -1;
	
	if( m_connect_timeout == timeout );
	{
		INFOLOGSG("SetConnectTimeout same value");
		return 0;
	}
	
	m_connect_timeout = timeout;
	
	return 0;
}

int Cplugin::PushHostInfo( string host_url )
{
	size_t		index = 0;
    	HostInfo	host;

	INFOLOGSG("host_url[%s]", host_url.c_str() );	  	
    	index = host_url.find_first_of( ":" ) ;
    	if( index == host_url.npos )
    	{
    		ERRORLOGSG( " servers[%s] ip端口格式错误" , host_url.c_str() );
    		return -1;
    	}
    	
    	host.ip = host_url.substr( 0, index );
    	host.port = atoi( host_url.substr( index + 1 ).c_str() );
    	if( host.ip.empty() || host.port <= 0 )
    	{
    		ERRORLOGSG( " host_url[%s] ip端口格式错误" , host_url.c_str() );
    		return -1;
    	}
        m_vector_host.push_back( host );
  
	return 0;
}

int Cplugin::SetServerUrl( string server_url )
{
	size_t		start = 0;
	size_t		index = 0;
	string	 	tmp;
	int		nret;
	
	if( server_url.empty() )
		return -1;
			
	if( m_server_url == server_url )
	{
		INFOLOGSG("SetServerUrl same value");
		return 0;
	}
	
	m_vector_host.clear();
        index = server_url.find_first_of( ";" );
        while( index != server_url.npos )
        {
            if( start != index )
            {  	
            	tmp = server_url.substr( start, index - start );
            	nret = PushHostInfo( tmp );
            	if( nret )
            	{
            		ERRORLOGSG( "PushHostInfo failed" );
            		return -1;
            	}
            	
            }
            
            start = index + 1;
            index = server_url.find_first_of( ";", start );
        }

        tmp = server_url.substr( start );
        if( !tmp.empty() )
        {
            	nret = PushHostInfo( tmp );
            	if( nret )
            	{
            		ERRORLOGSG( "PushHostInfo failed" );
            		return -1;
            	}
        }
        
	m_server_url = server_url;

	return 0;
}

int Cplugin:: Disconnect( struct NetAddress *p_netaddr )
{
	if( p_netaddr )
	{
		if( p_netaddr->sock != -1 )
		{
			INFOLOGSG( "diconnect[%s:%d] ok" , p_netaddr->ip , p_netaddr->port );
			close( p_netaddr->sock );
			p_netaddr->sock = -1 ;
		}
	}

	return 0;
}

int ThreadWorker( void *arg, int threadno )
{
	BatSeqAtrr	*p_batch = (BatSeqAtrr*)arg ;
	Cplugin	 	*p_Cplugin = (Cplugin*)p_batch->arg ;
	int 		nret;

	nret = p_Cplugin->ThreadWorkerImp( p_batch, threadno );
	if( nret )
	{
		ERRORLOGSG("ThreadWorkerImp failed nret[%d]\n", nret );
	}
	
	return 0;
}

int Cplugin::ThreadWorkerImp( BatSeqAtrr *p_batch, int threadno )
{
	int 		nret;
	UhpResponse	response;
	
	memset( &response, 0, sizeof(UhpResponse) );
	nret = HttpRequest( (char*)p_batch->name.c_str(), p_batch->client_cache, &response );
	if( nret )
	{
		ERRORLOGSG( "HttpRequest failed nret[%d]", nret );
		CAutoLock lock( p_batch->lock );
		p_batch->is_pulling = 0; 	/*失败重新推送*/
	}
	else
	{
		CAutoLock lock( p_batch->lock );
		p_batch->client_cache = response.client_cache;
		p_batch->client_alert_diff = response.client_alert_diff;
		p_batch->total_count += response.count;
		p_batch->is_pulling = 0;
		p_batch->deque_seq.push_back( response );
		
		INFOLOGSG("Tid[%ld] threadno[%d] cur_val[%llu] count[%d] 异步拉去数据成功\n",pthread_self() ,threadno, response.value, response.count );
		
	}
	
	return 0;
	
}

unsigned long long Cplugin::GetSequence( char* name )
{
	unsigned long long cur_val = 0;
	UhpResponse	response;
	int		nret;
	int		client_cache = 1;
	
	memset( &response, 0, sizeof(UhpResponse) );
	CAutoLock lock( m_map_batseq_lock );
	StrSeqSortMap::iterator it = m_map_batseq.find( name );
	if( it != m_map_batseq.end() )
	{
		BatSeqAtrr	 *p_batch = NULL;
		UhpResponse	 *p_response = NULL;
	
		p_batch = &( it->second );
		lock.Unlock();
		
		CAutoLock  lock2( p_batch->lock );
		if( !p_batch->deque_seq.empty() ) 
		{
			p_response = &( p_batch->deque_seq.front() );
			if( p_response->count <= 0 )
			{
				p_batch->deque_seq.pop_front(); //弹出消耗完的序列
			}
			if( !p_batch->deque_seq.empty() ) 
				p_response = &( p_batch->deque_seq.front() );
			else
				p_response = NULL;
			
		}
		
		if( p_response == NULL )
		{	
			//printf("client_cache[%d]\n", p_batch->client_cache );
			if( p_batch->client_cache > 0 )
				client_cache = p_batch->client_cache;
			
			nret = HttpRequest( name, client_cache, &response );
			if( nret )
			{
				ERRORLOGSG( "HttpRequest failed nret[%d]", nret );
				return 0;
			}
					
			if( response.value == 0 )
			{
				ERRORLOGSG( "GetLocalSequece failed");
				return 0;
			}
			cur_val = response.value;
			p_batch->client_cache = response.client_cache;
			p_batch->client_alert_diff = response.client_alert_diff;
			
			if( response.count > 1 )
			{
				response.value++;
				response.count--;
				p_batch->total_count = response.count;
				p_batch->deque_seq.push_back( response );
			}
		}
		else
		{
			cur_val = p_response->value;
			p_response->value++;
			p_response->count--;
			p_batch->total_count--;
			
			//保留个数过小触发异步获取
			if( p_batch->total_count <= p_batch->client_alert_diff && p_batch->is_pulling == 0 
				&& p_batch->client_cache > 0 )
			{
				taskinfo_t	task ;
				
				p_batch->is_pulling = 1; //设置正在拉去标志
				p_batch->arg = this;
				memset( &task, 0, sizeof(taskinfo_t) );
				task.fn_callback = ThreadWorker;
				task.arg = p_batch;
				nret = threadpool_addTask( m_threadpool, &task );
				if( nret < 0 )
				{
					ERRORLOGSG( "threadpool_addTask failed nret[%d]\n", nret);
					return 0;
				}
				INFOLOGSG( "threadpool_addTask ok cur_val[%llu] total_count[%d] nret[%d]\n", cur_val, p_batch->total_count, nret);
				
			}
			
		}
		
	}
	else
	{
		BatSeqAtrr	batSeq;
		
		nret = HttpRequest( name, client_cache, &response );
		if( nret )
		{
			ERRORLOGSG( "HttpRequest failed nret[%d]", nret );
			return 0;
		}
		if( response.value == 0 )
		{
			ERRORLOGSG( "GetLocalSequece failed");
			return 0;
		}

		//printf("response.count[%d] client_cache[%d], response.value[%llu]\n",  response.count, response.client_cache,response.value );
		batSeq.name = name;
		cur_val = response.value;
		batSeq.client_cache = response.client_cache;
		batSeq.client_alert_diff = response.client_alert_diff;
		if( response.count > 1 )
		{
			response.value++;
			response.count--;
			batSeq.total_count = response.count;
			batSeq.deque_seq.push_back( response );
		}

		m_map_batseq[name] = batSeq;
	}
	
	return cur_val;
}

int Cplugin::InitLogEnvV( char *service_name , char *module_name , char *event_output , int process_loglevel , char *process_output_format , va_list valist )
{
	char			pathfilename[ MAXLEN_FILENAME + 1 ] ;
	char			process_output[ MAXLEN_FILENAME + 1 ] ;
	LOG			*g = NULL ;
	char 			*host_name = NULL ;
	char 			*user_name = NULL ;
	
	int			nret = 0 ;
	
	host_name = GetHostnamePtr();
	user_name = getenv( "USER" );
	
	/* 创建日志句柄集合 */
	CreateLogsHandleG();
	
	if( event_output && event_output[0] )
	{
		/* 创建event日志句柄 */
		g = CreateLogHandle() ;
		if( g == NULL )
			return -11;
		
		SetLogCustLabel( g , 1 , host_name );
		SetLogCustLabel( g , 2 , user_name );
		SetLogCustLabel( g , 3 , service_name );
		SetLogCustLabel( g , 4 , module_name );
		
		
		memset( pathfilename , 0x00 , sizeof(pathfilename) );
		snprintf( pathfilename , sizeof(pathfilename)-1 , "%s/%s" , getenv("HOME") , event_output );
		/*printf("event pathfilename:%s\n", pathfilename);*/
		SetLogStyles( g , LOG_STYLE_DATETIME|LOG_STYLE_LOGLEVEL|LOG_STYLE_CUSTLABEL1|LOG_STYLE_CUSTLABEL2|LOG_STYLE_CUSTLABEL3|LOG_STYLE_CUSTLABEL4|LOG_STYLE_FORMAT|LOG_STYLE_NEWLINE , NULL );
		nret = SetLogOutput( g , LOG_OUTPUT_FILE , pathfilename , LOG_NO_OUTPUTFUNC ) ;
		
		if( nret )
			return -12;
		
		SetLogLevel( g , LOG_LEVEL_NOTICE );

		nret = AddLogToLogsG( "event" , g ) ;
		if( nret )
			return -13;
	}
	
	if( process_output_format && process_output_format[0] )
	{
		/* 创建process日志句柄 */
		g = CreateLogHandle() ;
		if( g == NULL )
			return -21;
		
		SetLogCustLabel( g , 1 , host_name );
		SetLogCustLabel( g , 2 , user_name );
		SetLogCustLabel( g , 3 , service_name );
		SetLogCustLabel( g , 4 , module_name );
		
		memset( process_output , 0x00 , sizeof(process_output) );
		vsnprintf( process_output , sizeof(process_output)-1 , process_output_format , valist );

		memset( pathfilename , 0x00 , sizeof(pathfilename) );
		snprintf( pathfilename , sizeof(pathfilename)-1 , "%s/%s" , getenv("HOME") , process_output );
		/*printf("process pathfilename:%s\n", pathfilename);*/
		SetLogStyles( g , LOG_STYLE_DATETIMEMS|LOG_STYLE_LOGLEVEL|LOG_STYLE_CUSTLABEL1|LOG_STYLE_CUSTLABEL2|LOG_STYLE_CUSTLABEL3|LOG_STYLE_CUSTLABEL4|LOG_STYLE_PID|LOG_STYLE_TID|LOG_STYLE_SOURCE|LOG_STYLE_FORMAT|LOG_STYLE_NEWLINE , NULL );
		nret = SetLogOutput( g , LOG_OUTPUT_FILE , pathfilename , LOG_NO_OUTPUTFUNC ) ;
		if( nret )
			return -22;
		
		SetLogLevel( g , process_loglevel );
		
		nret = AddLogToLogsG( "process" , g ) ;
		if( nret )
			return -23;
	}
	
	/* 创建ERROR日志句柄 */
	g = CreateLogHandle() ;
	if( g == NULL )
		return -51;
	
	SetLogCustLabel( g , 1 , host_name );
	SetLogCustLabel( g , 2 , user_name );
	SetLogCustLabel( g , 3 , service_name );
	SetLogCustLabel( g , 4 , module_name );
	
	memset( pathfilename , 0x00 , sizeof(pathfilename) );
	snprintf( pathfilename , sizeof(pathfilename)-1 , "%s/log/ERROR.log" , getenv("HOME") );
	SetLogStyles( g , LOG_STYLE_DATETIMEMS|LOG_STYLE_LOGLEVEL|LOG_STYLE_CUSTLABEL1|LOG_STYLE_CUSTLABEL2|LOG_STYLE_CUSTLABEL3|LOG_STYLE_CUSTLABEL4|LOG_STYLE_PID|LOG_STYLE_SOURCE|LOG_STYLE_FORMAT|LOG_STYLE_NEWLINE , NULL );
	nret = SetLogOutput( g , LOG_OUTPUT_FILE , pathfilename , LOG_NO_OUTPUTFUNC ) ;
	if( nret )
		return -52;
	
	SetLogLevel( g , LOG_LEVEL_ERROR );

	nret = AddLogToLogsG( "ERROR" , g ) ;
	if( nret )
		return -53;
		
	return nret;
}

int Cplugin:: InitLogEnv_Format( char *service_name , char *module_name , char *event_output , int process_loglevel , char *process_output_format , ... )
{
	va_list			valist ;
	int			nret = 0 ;
	
	va_start( valist , process_output_format );
	nret = InitLogEnvV( service_name , module_name , event_output , process_loglevel , process_output_format , valist ) ;
	va_end( valist );
	
	return nret;
}

int Cplugin:: CleanLogEnv()
{
	if( GetLogsHandleG() )
	{
		DestroyLogsHandleG();
		SetLogsHandleG( NULL );
	}
	
	return 0;
}

int Cplugin:: InitLogEnv()
{
	int	nret;
	char	*OS_User = NULL;
	char	*p_loglevel = "INFO";
	char	*module_name = "sdk";
	char    *service_name = "uhp_sdk";

	OS_User = getenv( "USER" );
	CleanLogEnv();
	nret = InitLogEnv_Format( service_name , module_name , "log/event.log" , ConvertLogLevel( p_loglevel ) ,"log/uhp_%s_%s_%s.log" , module_name, GetHostnamePtr() , OS_User );
	if( nret )
		return -1;
		
	return 0;	
}

int Load()
{
	INFOLOGSG( "Load ok" );
	
	return 0;
}

int Unload()
{
	Cplugin::Uninstance();   
	INFOLOGSG( "plugin unload clean" );
	
	return 0;
}

unsigned long long GetSequence( char *name )
{
	return Cplugin::Instance()->GetSequence( name );
}

int SetConnectTimeout( int timeout )
{
	return Cplugin::Instance()->SetConnectTimeout( timeout );
}

int SetServerUrl( char* server_url )
{
	return Cplugin::Instance()->SetServerUrl( server_url );
}
