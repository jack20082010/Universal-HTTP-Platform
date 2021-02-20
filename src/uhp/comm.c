/* 
 * author	: jack
 * email	: xulongzhong2004@163.com
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "uhp_in.h"

/* 处理接受新连接事件 */
int OnAcceptingSocket( HttpserverEnv *p_env , struct ListenSession *p_listen_session )
{
	struct AcceptedSession	*p_accepted_session = NULL ;
	SOCKLEN_T		accept_addr_len ;
	struct epoll_event	event ;
	int			nret = 0 ;
	int 			task_count; 
	int			index;
	static	int		accepte_num = 0;
	
	/* 申请内存以存放客户端连接会话结构 */
	p_accepted_session = (struct AcceptedSession *)malloc( sizeof(struct AcceptedSession) ) ;
	if( p_accepted_session == NULL )
	{
		ERRORLOGSG( "malloc p_accepted_session failed , errno[%d]" , errno );
		return 1;
	}
	memset( p_accepted_session , 0x00 , sizeof(struct AcceptedSession) );
	
	p_accepted_session->body_len = p_env->httpserver_conf.httpserver.server.maxHttpResponse;
	p_accepted_session->http_rsp_body = (char*)malloc( p_accepted_session->body_len );
	if( p_accepted_session->http_rsp_body == NULL )
	{
		ERRORLOGSG( "malloc http_rsp_body failed , errno[%d]" , errno );
		return 1;
	}
	memset( p_accepted_session->http_rsp_body , 0x00 , p_accepted_session->body_len );
	
	
	/* 接受新连接 */
	accept_addr_len = sizeof(struct sockaddr) ;
	p_accepted_session->netaddr.sock = accept( p_listen_session->netaddr.sock , (struct sockaddr *) & (p_accepted_session->netaddr.addr) , & accept_addr_len ) ;
	if( p_accepted_session->netaddr.sock == -1 )
	{
		if ( !( errno == EAGAIN || errno == EWOULDBLOCK) )
		{
			ERRORLOGSG( "accept failed , errno[%d]" , errno );
		}
		free( p_accepted_session );
		return 1;
	}
	
	/*增加流控逻辑*/
	task_count = threadpool_getTaskCount( p_env->p_threadpool );
	if( task_count > p_env->httpserver_conf.httpserver.server.maxTaskcount )
	{
		
		ERRORLOGSG( "到达服务器任务堆积上限[%d],强制断开连接" , p_env->httpserver_conf.httpserver.server.maxTaskcount );
		close( p_accepted_session->netaddr.sock );
		free( p_accepted_session );
		return 1;
	}
	
	gettimeofday( &( p_accepted_session->perfms.tv_accepted ), NULL );
	p_accepted_session->accept_begin_time = p_accepted_session->perfms.tv_accepted.tv_sec;
	SetHttpNonblock( p_accepted_session->netaddr.sock );
	SetHttpNodelay( p_accepted_session->netaddr.sock , 1 );
	
	GETNETADDRESS( p_accepted_session->netaddr )
	GETNETADDRESS_LOCAL( p_accepted_session->netaddr )
	GETNETADDRESS_REMOTE( p_accepted_session->netaddr )
	DEBUGLOGSG( "accept ok , socket[%d] [%s:%d]->[%s:%d] session[%p]" , p_accepted_session->netaddr.sock ,
		p_accepted_session->netaddr.remote_ip , p_accepted_session->netaddr.remote_port , 
		p_accepted_session->netaddr.local_ip ,p_accepted_session->netaddr.local_port ,
		p_accepted_session );
	
	/* 创建HTTP环境 */
	p_accepted_session->http = CreateHttpEnv() ;
	if( p_accepted_session->http == NULL )
	{
		ERRORLOGSG( "CreateHttpEnv failed , errno[%d]" , errno );
		close( p_accepted_session->netaddr.sock );
		free( p_accepted_session );
		return 1;
	}

	p_accepted_session->request_begin_time = p_accepted_session->perfms.tv_accepted.tv_sec;
	p_accepted_session->status = SESSION_STATUS_RECEIVE;
	INFOLOGSG( "session fd[%d] set SESSION_STATUS_RECEIVE" , p_accepted_session->netaddr.sock );
	
	/* 加入新套接字可读事件到epoll */
	memset( & event , 0x00 , sizeof(struct epoll_event) );
	event.events = EPOLLIN | EPOLLERR ;
	event.data.ptr = p_accepted_session ;
	
	if( accepte_num >= 100* p_env->httpserver_conf.httpserver.server.epollThread )
		accepte_num = 0;
	index = accepte_num % p_env->httpserver_conf.httpserver.server.epollThread;
	accepte_num++;
	
	p_accepted_session->index = index;
	p_accepted_session->epoll_fd = p_env->thread_epoll[index].epoll_fd ;
	p_accepted_session->epoll_fd_send = p_env->thread_epoll_send[index].epoll_fd ;
	
	pthread_mutex_lock( &p_env->thread_epoll[p_accepted_session->index].session_lock );
	p_env->thread_epoll[index].p_set_session->insert( p_accepted_session );
	pthread_mutex_unlock( &p_env->thread_epoll[p_accepted_session->index].session_lock );
	
	INFOLOGSG( "accept epoll_index[%d] epoll_fd[%d] fd[%d] p_accepted_session[%p]", index, p_accepted_session->epoll_fd, p_accepted_session->netaddr.sock, p_accepted_session );
	
	nret = epoll_ctl( p_accepted_session->epoll_fd , EPOLL_CTL_ADD , p_accepted_session->netaddr.sock , & event ) ;
	if( nret == -1 )
	{
		ERRORLOGSG( "epoll_ctl epoll_fd[%d] add accepted_session[%d] EPOLLIN failed , errno[%d]" , p_accepted_session->epoll_fd , p_accepted_session->netaddr.sock , errno );
		DestroyHttpEnv( p_accepted_session->http );
		close( p_accepted_session->netaddr.sock );
		
		pthread_mutex_lock( &p_env->thread_epoll[p_accepted_session->index].session_lock );
		p_env->thread_epoll[index].p_set_session->erase( p_accepted_session );
		pthread_mutex_unlock( &p_env->thread_epoll[p_accepted_session->index].session_lock );

		free( p_accepted_session );
		
		return 1;
	}
	DEBUGLOGSG( "epoll_ctl[%d] add accepted_session[%d] EPOLLIN ok" , p_accepted_session->epoll_fd , p_accepted_session->netaddr.sock );
		
	return 0;
}

/* 主动关闭套接字 */
void OnClosingSocket( HttpserverEnv *p_env , struct AcceptedSession *p_accepted_session, BOOL with_lock )
{
	if( p_accepted_session )
	{
		epoll_ctl( p_accepted_session->epoll_fd_send , EPOLL_CTL_DEL , p_accepted_session->netaddr.sock , NULL );
		epoll_ctl( p_accepted_session->epoll_fd , EPOLL_CTL_DEL , p_accepted_session->netaddr.sock , NULL );
		DEBUGLOGSG( "epoll_ctl[%d] epoll_del fd[%d] p_accepted_session[%p] status[%d]" , p_accepted_session->epoll_fd , p_accepted_session->netaddr.sock, p_accepted_session, p_accepted_session->status );
		
		
		if( with_lock )
                {
                        pthread_mutex_lock( &p_env->thread_epoll[p_accepted_session->index].session_lock );
                        p_env->thread_epoll[p_accepted_session->index].p_set_session->erase( p_accepted_session );
                        pthread_mutex_unlock( &p_env->thread_epoll[p_accepted_session->index].session_lock );
                }
                else
                {
                       p_env->thread_epoll[p_accepted_session->index].p_set_session->erase( p_accepted_session );
                }
                
		if( p_accepted_session->status == SESSION_STATUS_PUBLISH )
		{
			/*等待线程池工作完成后，延迟释放*/
			p_accepted_session->needFree = 1;
			INFOLOGSG( "needFree fd[%d] [%s:%d]->[%s:%d] p_accepted_session[%p] status[%d]" , p_accepted_session->netaddr.sock , p_accepted_session->netaddr.remote_ip , p_accepted_session->netaddr.remote_port , p_accepted_session->netaddr.local_ip , p_accepted_session->netaddr.local_port , p_accepted_session,p_accepted_session->status );
		}
		else
		{
			/*已经fd已经从epoll中删除，所以不会存在并发问题*/
			FreeSession( p_accepted_session );
		}
	}

	return;
}

/* 接收客户端套接字数据 */
int OnReceivingSocket( HttpserverEnv *p_env , struct AcceptedSession *p_accepted_session )
{
	struct HttpBuffer	*rsp_buf = NULL ;
	struct HttpBuffer	*req_buf = NULL ;
	struct timeval 		now_time;
	
	int			nret = 0 ;
	
	if( g_exit_flag && threadpool_isShutdown( p_env->p_threadpool ) )
	{
		/*接收退出命令后直接关闭socket，sdk发送转连*/
		ERRORLOGSG( "threadpool is destroy session fd[%d]", p_accepted_session->netaddr.sock );
		return 1;
	}

	if( p_accepted_session->perfms.tv_receive_begin.tv_sec <= 0 )
	{	
		gettimeofday( &( p_accepted_session->perfms.tv_receive_begin ), NULL ) ;
		now_time.tv_sec = p_accepted_session->perfms.tv_receive_begin.tv_sec;
		now_time.tv_usec = 0;
		p_accepted_session->hangTimeoutFlag = 0;

		if( CheckHttpKeepAlive( p_accepted_session->http ) )
		{
			//重新赋值后不需先删除后添加否则不会排序
			p_accepted_session->request_begin_time = p_accepted_session->perfms.tv_receive_begin.tv_sec;
			//pthread_mutex_lock( &p_env->thread_epoll[p_accepted_session->index].session_lock );
			//p_env->thread_epoll[p_accepted_session->index].p_set_session->erase( p_accepted_session );
			//p_env->thread_epoll[p_accepted_session->index].p_set_session->insert( p_accepted_session );
			//pthread_mutex_unlock( &p_env->thread_epoll[p_accepted_session->index].session_lock );
		}
	}
	else
	{
		gettimeofday( &now_time, NULL ) ;
	}
	
	/*判断交易是否超时*/
	if( ( now_time.tv_sec - p_accepted_session->request_begin_time ) > p_env->httpserver_conf.httpserver.server.totalTimeout )
	{
		ERRORLOGSG( "session fd[%d], now_time[%ld] request_begin_time[%ld] totalTimeout[%d] ", 
		p_accepted_session->netaddr.sock, now_time.tv_sec,
		p_accepted_session->request_begin_time,
		p_env->httpserver_conf.httpserver.server.totalTimeout );
		ERRORLOGSG( "OnReceivingSocket timeout close session fd[%d]", p_accepted_session->netaddr.sock );
		return 1;
	}
	
	req_buf = GetHttpRequestBuffer( p_accepted_session->http ) ;
	/* 接收请求数据 */
	nret = ReceiveHttpRequestNonblock( p_accepted_session->netaddr.sock , NULL , p_accepted_session->http ) ;
	if( nret == FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER )
	{
		DEBUGLOGSG( "ReceiveHttpRequestNonblock[%d] return FASTERHTTP_INFO_NEED_MORE_HTTP_BUFFER" , p_accepted_session->netaddr.sock );
		return 0;
	}
	else if( nret == FASTERHTTP_INFO_TCP_CLOSE )
	{
		INFOLOGSG( "ReceiveHttpRequestNonblock[%d] return[%d]" , p_accepted_session->netaddr.sock , nret );
		return 1;
	}
	else if( nret )
	{
		ERRORLOGSG( "ReceiveHttpRequestNonblock[%d] return ERROR[%d] p_accepted_session[%p] status[%d]" , p_accepted_session->netaddr.sock , nret, p_accepted_session, p_accepted_session->status );
		DEBUGHEXLOGSG( GetHttpBufferBase(req_buf,NULL) , GetHttpBufferLength(req_buf) , "RECV REQUEST HTTP [%d]BYTES" , GetHttpBufferLength(req_buf) );
		return 1;
	}
	else
	{
		char 			*p_base = NULL;
		
		p_base = GetHttpBufferBase( req_buf, NULL );
		/* 接收完整了 */
		DEBUGLOGSG( "ReceiveHttpRequestNonblock[%d] return DONE" , p_accepted_session->netaddr.sock );
		INFOLOGSG( "RECV REQUEST HTTP [%.*s]" , GetHttpBufferLength( req_buf ) , p_base );

		/*清空返回响应体，防止长连接请求，遗留上次响应内容*/
		memset( p_accepted_session->http_rsp_body, 0, p_accepted_session->body_len );
		nret = FormatHttpResponseStartLine( HTTP_OK , p_accepted_session->http , 0 , NULL ) ;
		if( nret )
		{
			ERRORLOGSG( "FormatHttpResponseStartLine failed[%d]" , nret );
			return 1;
		}
		
		/*发送心跳数据,直接回复pong*/
		if( p_base && strncasecmp( p_base, "ping", 4 ) == 0 )
		{
			nret = FormatHttpResponseStartLine( HTTP_OK , p_accepted_session->http , 0 , HTTP_HEADER_CONTENT_TYPE": %s;%s" HTTP_RETURN_NEWLINE "Content-length: %d" 
				HTTP_RETURN_NEWLINE HTTP_RETURN_NEWLINE "%s" , HTTP_HEADER_CONTENT_TYPE_TEXT, p_accepted_session->charset , 4 , "pong" );
			if( nret )
			{
				ERRORLOGSG( "FormatHttpResponseStartLine failed[%d]" , nret );
				return 1;
			}
			
			rsp_buf = GetHttpResponseBuffer( p_accepted_session->http ) ;
			INFOLOGSG( "RESPONSE HTTP [%.*s]" , GetHttpBufferLength(rsp_buf) , GetHttpBufferBase(rsp_buf,NULL) );
			
			nret = AddEpollSendEvent( p_env, p_accepted_session );
			if( nret )
				return nret;
			
			return 0;
		}
		
		/* 调用应用层 */
		nret = OnProcess( p_env , p_accepted_session ) ;
		if( nret != HTTP_OK )
		{
			char			body_convert[MAX_RESPONSE_BODY_LEN];
			int 			status_code ;
			
			status_code = nret;
			memset( body_convert, 0, sizeof(body_convert) );
			nret = ConvertReponseBody( p_accepted_session, body_convert, sizeof(body_convert) );
			if( nret )
				status_code = HTTP_NOT_ACCEPTABLE;
			
			nret = FormatHttpResponseStartLine( status_code , p_accepted_session->http , 0 , HTTP_HEADER_CONTENT_TYPE": %s;%s" HTTP_RETURN_NEWLINE "Content-length: %d" 
				HTTP_RETURN_NEWLINE HTTP_RETURN_NEWLINE "%s" , HTTP_HEADER_CONTENT_TYPE_JSON, p_accepted_session->charset , strlen(body_convert) , body_convert );
			if( nret )
			{
				ERRORLOGSG( "FormatHttpResponseStartLine failed[%d]" , nret );
				return 1;
			}
			
			rsp_buf = GetHttpResponseBuffer( p_accepted_session->http ) ;
			INFOLOGSG( "RESPONSE HTTP [%.*s]" , GetHttpBufferLength(rsp_buf) , GetHttpBufferBase(rsp_buf,NULL) );
			
			nret = AddEpollSendEvent( p_env, p_accepted_session );
			if( nret )
				return nret;
		}
	}
	
	return 0;
}

/* 发送客户端套接字数据 */
int OnSendingSocket( HttpserverEnv *p_env , struct AcceptedSession *p_accepted_session )
{
	struct timeval 		now_time;
	
	int			nret = 0 ;
	
	/* 发送响应数据 */
	
	/*线程池已经完成业务处理，安全释放session*/
	if( p_accepted_session->needFree )
	{
		/*已经fd已经从epoll中删除，所以不会存在并发问题*/
		FreeSession( p_accepted_session );
		return 0;
	}
	
	if( p_accepted_session->status < SESSION_STATUS_SEND )
	{
		INFOLOGSG( "session fd[%d] set SESSION_STATUS_SEND" , p_accepted_session->netaddr.sock );
		p_accepted_session->status = SESSION_STATUS_SEND; 
	}
		
	if( p_accepted_session->perfms.tv_send_begin.tv_sec <=0 )
	{
		gettimeofday( &( p_accepted_session->perfms.tv_send_begin ), NULL );
		now_time.tv_sec = p_accepted_session->perfms.tv_send_begin.tv_sec;
		now_time.tv_usec = 0;
	}
	else
	{
		gettimeofday( &now_time, NULL );
	}
	
	/*判断交易是否超时*/
	if( ( now_time.tv_sec - p_accepted_session->request_begin_time ) > p_env->httpserver_conf.httpserver.server.totalTimeout )
	{
		ERRORLOGSG( "session fd[%d], now_time[%ld] request_begin_time[%ld] totalTimeout[%d] ", 
		p_accepted_session->netaddr.sock, now_time.tv_sec,
		p_accepted_session->request_begin_time,
		p_env->httpserver_conf.httpserver.server.totalTimeout );
		ERRORLOGSG( "OnSendingSocket timeout close session fd[%d]", p_accepted_session->netaddr.sock );
		return 1;
	}
	
	nret = SendHttpResponseNonblock( p_accepted_session->netaddr.sock , NULL , p_accepted_session->http ) ;
	if( nret == FASTERHTTP_INFO_TCP_SEND_WOULDBLOCK )
	{
		DEBUGLOGSG( "SendHttpResponseNonblock[%d] return FASTERHTTP_INFO_TCP_SEND_WOULDBLOCK" , p_accepted_session->netaddr.sock );
		return 0;
	}
	else if( nret )
	{
		ERRORLOGSG( "SendHttpResponseNonblock[%d] return ERROR[%d]" , p_accepted_session->netaddr.sock , nret );
		return 1;
	}
	else
	{
		gettimeofday( &( p_accepted_session->perfms.tv_send_end ), NULL );
		DEBUGLOGSG( "SendHttpResponseNonblock[%d] return DONE" , p_accepted_session->netaddr.sock );
		
		{
			char			*URI = NULL ;
			int			URI_LEN ;
			char			*STATUS_CODE = NULL ;
			int			STATUS_CODE_LEN ;
			double			cost_time;
			struct HttpBuffer	*rsp_buf = NULL ;
			
			rsp_buf = GetHttpResponseBuffer( p_accepted_session->http ) ;
			INFOLOGSG( "RESPONSE HTTP [%.*s]" , GetHttpBufferLength(rsp_buf) , GetHttpBufferBase(rsp_buf,NULL) );
	
			if( CheckHttpKeepAlive( p_accepted_session->http ) )
			{
				cost_time = p_accepted_session->perfms.tv_send_end.tv_sec - p_accepted_session->perfms.tv_receive_begin.tv_sec
						+ ( p_accepted_session->perfms.tv_send_end.tv_usec - p_accepted_session->perfms.tv_receive_begin.tv_usec )/1000000.0 ;
			}
			else
			{
				cost_time = p_accepted_session->perfms.tv_send_end.tv_sec - p_accepted_session->perfms.tv_accepted.tv_sec
						+ ( p_accepted_session->perfms.tv_send_end.tv_usec - p_accepted_session->perfms.tv_accepted.tv_usec )/1000000.0 ;
			}
			
			URI = GetHttpHeaderPtr_URI( p_accepted_session->http , & URI_LEN );
			STATUS_CODE = GetHttpHeaderPtr_STATUSCODE( p_accepted_session->http , & STATUS_CODE_LEN );
			
			NOTICELOGSG( "[%s:%d][%.*s][%.*s][%.3f]"
					, p_accepted_session->netaddr.ip , p_accepted_session->netaddr.port
					, URI_LEN , URI
					, STATUS_CODE_LEN , STATUS_CODE, cost_time );
		}
		
		SetHttpTimeout( p_accepted_session->http , -1 );
		ResetHttpEnv( p_accepted_session->http ) ;
		
		/*导出性能日志*/
		ExportPerfmsFile( p_env, p_accepted_session );
		
		/*如果是长连接清空相关时间戳*/
		if( CheckHttpKeepAlive( p_accepted_session->http ) )
		{
			if( ( p_accepted_session->perfms.tv_send_end.tv_sec - p_accepted_session->accept_begin_time ) > p_env->httpserver_conf.httpserver.server.keepAliveMaxTime )
			{
				INFOLOGSG( "OnSendingSocket keepAliveMaxTime timeout[%d]ms close session fd[%d]",
					p_env->httpserver_conf.httpserver.server.keepAliveMaxTime,p_accepted_session->netaddr.sock );
				OnClosingSocket( p_env, p_accepted_session, TRUE );
				
				return 0;
			}
			else
			{
				memset( p_accepted_session->http_rsp_body, 0, p_accepted_session->body_len );
				p_accepted_session->request_begin_time = p_accepted_session->perfms.tv_send_end.tv_sec;
				p_accepted_session->status = SESSION_STATUS_RECEIVE;
				memset( &( p_accepted_session->perfms ), 0 , sizeof(struct Performamce) );
				INFOLOGSG( "session fd[%d] receive end set SESSION_STATUS_RECEIVE" , p_accepted_session->netaddr.sock );
			}
		}
		else
		{
			p_accepted_session->status = SESSION_STATUS_DIE;
			INFOLOGSG( "session fd[%d] send end set SESSION_STATUS_DIE" , p_accepted_session->netaddr.sock );
		}
		
		nret = AddEpollRecvEvent( p_env, p_accepted_session );
		if( nret )
			return nret;

	}
	
	return 0;
}

int OnReceivePipe( HttpserverEnv *p_env , struct PipeSession *p_pipe_session )
{
	int	nret = 0 ;
	
	nret = OnProcessPipeEvent( p_env, p_pipe_session );
	if( nret == -2 )
		return -1;
	else if( nret )
		return 1;
	
	return 0;
}
