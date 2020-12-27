/*
 * author	: jack
 * email	: xulongzhong2004@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "uhp_in.h"

/* 快速、单次的发起HTTP请求 */
static int HttpRequestV( struct NetAddress *p_netaddr , struct HttpEnv *http , int timeout , char *format , va_list valist )
{
	struct HttpBuffer	*b = NULL ;
	
	int			nret = 0 ;
	
	p_netaddr->sock = socket( AF_INET , SOCK_STREAM , IPPROTO_TCP ) ;
	if( p_netaddr->sock == -1 )
	{
		ERRORLOGSG( "socket failed , errno[%d]" , errno );
		return -1;
	}
	
	SetHttpNodelay( p_netaddr->sock , 1 );
	
	SETNETADDRESS( (*p_netaddr) )
	nret = connect( p_netaddr->sock , (struct sockaddr *) & (p_netaddr->addr) , sizeof(struct sockaddr) ) ;
	if( nret == -1 )
	{
		ERRORLOGSG( "connect[%s:%d] failed , errno[%d]" , p_netaddr->ip , p_netaddr->port , errno );
		close( p_netaddr->sock ); p_netaddr->sock = -1 ;
		return -2;
	}
	
	ResetHttpEnv( http );
	
	b = GetHttpRequestBuffer( http ) ;
	nret = StrcatvHttpBuffer( b , format , valist ) ;
	if( nret )
	{
		ERRORLOGSG( "StrcatvHttpBuffer failed[%d]" , nret );
		close( p_netaddr->sock ); p_netaddr->sock = -1 ;
		return nret;
	}
	
	DEBUGHEXLOGSG( GetHttpBufferBase(b,NULL) , GetHttpBufferLength(b) , "RequestBuffer [%d]bytes" , GetHttpBufferLength(b));
	
	SetHttpTimeout( http , timeout );
	
	nret = SendHttpRequest( p_netaddr->sock , NULL , http ) ;
	if( nret )
	{
		ERRORLOGSG( "SendHttpRequest failed[%d]" , nret );
		close( p_netaddr->sock ); p_netaddr->sock = -1 ;
		return nret;
	}
	
	nret = ReceiveHttpResponse( p_netaddr->sock , NULL , http ) ;
	if( nret )
	{
		ERRORLOGSG( "ReceiveHttpResponse failed[%d]" , nret );
		close( p_netaddr->sock ); p_netaddr->sock = -1 ;
		return nret;
	}
	
	b = GetHttpResponseBuffer( http ) ;
	DEBUGHEXLOGSG( GetHttpBufferBase(b,NULL) , GetHttpBufferLength(b) , "ResponseBuffer [%d]bytes" , GetHttpBufferLength(b));
	
	close( p_netaddr->sock ); p_netaddr->sock = -1 ;
	
	return 0;
}

static int HttpClientV( struct HttpserverEnv *p_env , char *format , va_list valist )
{
	struct NetAddress	netaddr ;
	struct HttpEnv		*http = NULL ;
	
	char			*statuscode = NULL ; 
	int			statuscode_len ;
	/* char			*reasonphrase = NULL ;  */
	int			reasonphrase_len ;
	char			*body = NULL ;
	int			body_len ;
	
	int			nret = 0 ;
	
	memset( & netaddr , 0x00 , sizeof(struct NetAddress) );
	nret = QueryNetIpByHostName( p_env->httpserver_conf.httpserver.server.ip , netaddr.ip , sizeof(netaddr.ip) );
	if( nret )
	{
		ERRORLOGSG( "QueryNetIpByHostName ip[%s] failed" , p_env->httpserver_conf.httpserver.server.ip );
		return nret;
	}

	nret = QueryNetPortByServiceName( p_env->httpserver_conf.httpserver.server.port , "tcp" , & (netaddr.port) );
	if( nret )
	{
		ERRORLOGSG( "QueryNetPortByServiceName port[%s] failed" , p_env->httpserver_conf.httpserver.server.port );
		return nret;
	}

	if( netaddr.port == 0 )
	{
		printf( "*** ERROR : port[%s][%d] invalid\n" , p_env->httpserver_conf.httpserver.server.port , netaddr.port );
		return -1;
	}
	
	http = CreateHttpEnv() ;
	if( http == NULL )
	{
		printf( "*** ERROR : CreateHttpEnv failed , errno[%d]\n" , errno );
		return -1;
	}
	
	nret = HttpRequestV( & netaddr , http , 60 , format , valist ) ;
	if( nret )
	{
		printf( "*** ERROR : HttpRequest failed[%d] , errno[%d]\n" , nret , errno );
		DestroyHttpEnv( http );
		return nret;
	}
	
	statuscode = GetHttpHeaderPtr_STATUSCODE( http , & statuscode_len ) ;
	GetHttpHeaderPtr_REASONPHRASE( http , & reasonphrase_len ) ;
	body = GetHttpBodyPtr( http , & body_len );
	if( body )
	{
		if( body[body_len-1] != '\n' )
			printf( "%.*s\n" , body_len , body );
		else
			printf( "%.*s" , body_len , body );
	}
	nret = StringNToInt(statuscode,statuscode_len) ;
	
	DestroyHttpEnv( http );
	
	return nret;
}

static int HttpClient( struct HttpserverEnv *p_env , char *format , ... )
{
	va_list		valist ;
	
	int		nret = 0 ;
	
	va_start( valist , format );
	nret = HttpClientV( p_env , format , valist ) ;
	va_end( valist );
	
	return nret;
}

int ShowSessions( struct HttpserverEnv *p_env )
{
	return HttpClient( p_env ,  "GET %s  HTTP/1.0" HTTP_RETURN_NEWLINE 
					"%s: %s" HTTP_RETURN_NEWLINE		
					HTTP_RETURN_NEWLINE 
					, URI_SESSIONS
					, HTTP_HEADER_CONTENT_TYPE , HTTP_HEADER_CONTENT_TYPE_TEXT
					);
}

int ShowThreadStatus( struct HttpserverEnv *p_env )
{
	return HttpClient( p_env ,  "GET %s  HTTP/1.0" HTTP_RETURN_NEWLINE 
					"%s: %s" HTTP_RETURN_NEWLINE		
					HTTP_RETURN_NEWLINE 
					, URI_THREAD_STATUS
					, HTTP_HEADER_CONTENT_TYPE , HTTP_HEADER_CONTENT_TYPE_TEXT
					);
}

int ShowConfig( struct HttpserverEnv *p_env )
{
	return HttpClient( p_env ,  "GET %s  HTTP/1.0" HTTP_RETURN_NEWLINE 
					"%s: %s" HTTP_RETURN_NEWLINE		
					HTTP_RETURN_NEWLINE 
					, URI_CONFIG
					, HTTP_HEADER_CONTENT_TYPE , HTTP_HEADER_CONTENT_TYPE_TEXT
					);
}

/*
static int Recvn( int fd, void *buf, int len, long *p_timeout )
{
	int 		nret;
	int 		n = len ;
	char 		*rev_buf = (char*)buf;
	struct pollfd 	poll_fd;
	struct timeval	tvBegin , tvEnd ;
	long 		timeout_usec = 0;
	long		poll_timeout = -1;
	
	if ( fd < 0 || !buf || len <= 0 )
		return -1;
		
	if( p_timeout )
	{
		poll_timeout = *p_timeout; 
		timeout_usec = *p_timeout*1000;
	}
		
	poll_fd.fd = fd;
	poll_fd.events = POLLIN;
	
	while( n > 0 )
	{
		gettimeofday( & tvBegin , NULL );
		nret = poll( &poll_fd , 1 , poll_timeout );
		if( nret < 0 )
			return NSPROXY_POLL_ERROR;

		gettimeofday( & tvEnd , NULL );
		
		if( p_timeout )
		{
			timeout_usec -= ( tvEnd.tv_sec *1000*1000 + tvEnd.tv_usec ) - ( tvBegin.tv_sec *1000*1000 + tvBegin.tv_usec );
			poll_timeout = timeout_usec/1000;
			if( poll_timeout <= 0 )
				break;
		}
		if( nret > 0 )
		{
			nret = recv( fd , rev_buf , n , 0 );
			if( nret <= 0 )
				return NSPROXY_SOCKET_CLOSED;
			
			n -= nret;
			rev_buf += nret;
		}
	}
	
	if( n > 0 )
		return NSPROXY_POLL_TIMEOUT;
	
	if( p_timeout )
		*p_timeout = poll_timeout;
	
	return 0;
}
*/
