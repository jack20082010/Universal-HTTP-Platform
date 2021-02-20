#include "plugin.h"
#include "dbsqlca.h"
#include "LOGS.h"

#define HTTP_HEADER_CONTENT_TYPE_TEXT           "application/text;charset=gb18030"
#define	HTTP_HEADER_CONTENT_TYPE_JSON		"application/json"

int Load()
{

	INFOLOGSG( "plugin load" );
	return 0;
}

int Unload()
{
	INFOLOGSG( "plugin unload clean" );
	return 0;
}

int SetSessionResponse( AcceptedSession *p_session , int errcode, char *format , ...  ) 
{
	va_list			valist ;
	char			errmsg[ 1024 + 1 ];
	
	va_start( valist , format );
	memset( errmsg , 0x00 , sizeof(errmsg) );
	vsnprintf( errmsg , sizeof(errmsg)-1 , format , valist );
	va_end( valist );
	
	if( errcode == HTTP_OK )
		errcode = 0;
	
	return OnException( p_session, errcode, errmsg );
}

int NotifyOtherSessions( AcceptedSession *p_current, AcceptedSession *p_other, void *cb_arg )
{
	struct HttpEnv*		p_http_env = NULL;
	char			http_rsp_body[80] = "Notify ok";
	int			nret;
	
	p_http_env = UHPGetSessionHttpEnv( p_other );
	nret = FormatHttpResponseStartLine( HTTP_OK , p_http_env , 0 , HTTP_HEADER_CONTENT_TYPE": %s;%s" HTTP_RETURN_NEWLINE "Content-length: %d" 
		HTTP_RETURN_NEWLINE HTTP_RETURN_NEWLINE "%s" , HTTP_HEADER_CONTENT_TYPE_JSON, UHPGetSessionCharset(p_other) , strlen(http_rsp_body) , http_rsp_body );
	if( nret )
	{
		ERRORLOGSG( "FormatHttpResponseStartLine failed[%d]" , nret );
		return 1;
	}
	
	INFOLOGSG( "NotifyOtherSessions" );
	
	return 0;
}

int Doworker( AcceptedSession *p_session )
{
	struct HttpEnv*		p_http_env = NULL;
	char			*p_uri = NULL;
	int			uri_len;
	int			nret;
	
	p_http_env = UHPGetSessionHttpEnv( p_session );
	p_uri = GetHttpHeaderPtr_URI( p_http_env , & uri_len );
	//INFOLOGSG( "method[%.*s] uri[%.*s]" ,method_len , method ,  uri_len , p_uri );
	if( uri_len == sizeof(URI_MESSAGE)-1 && STRNICMP( p_uri , == , URI_MESSAGE , uri_len ) )
	{
		INFOLOGSG( "URI_MESSAGE[%s]", URI_MESSAGE );
		nret = FormatHttpResponseStartLine( HTTP_NOT_MODIFIED , p_http_env , 0 , HTTP_HEADER_CONTENT_TYPE": %s;%s" HTTP_RETURN_NEWLINE "Content-length: %d" 
			HTTP_RETURN_NEWLINE HTTP_RETURN_NEWLINE "%s" , HTTP_HEADER_CONTENT_TYPE_JSON, UHPGetSessionCharset(p_session) , strlen(HTTP_NOT_MODIFIED_T) , HTTP_NOT_MODIFIED_T );
		if( nret )
		{
			ERRORLOGSG( "FormatHttpResponseStartLine failed[%d]" , nret );
			return 1;
		}
	}
	else if( uri_len == sizeof(URI_NOTIFY)-1 && STRNICMP( p_uri , == , URI_NOTIFY , uri_len ) )
	{
		INFOLOGSG( "URI_NOTIFY[%s]", URI_NOTIFY );
		nret = UHPSetNotifyOtherSessions( p_session, NotifyOtherSessions, NULL );
		if( nret )
		{
			ERRORLOGSG( "UHPSetNotifyOtherSessions failed[%d]" , nret );
			return 1;
		}
		SetSessionResponse( p_session, 0, "" );
	}
	else
	{
		SetSessionResponse( p_session, -1, "uri unknown uri[%.*s]" , uri_len, p_uri );
		return -1;
	}
	
	return nret;
}

int OnException( AcceptedSession *p_session, int errcode, char *errmsg )
{
	char	*p_response = NULL;
	int	len;
	
	len = UHPGetSessionResponseSize( p_session );
	p_response = UHPGetSessionResponse( p_session );
	snprintf( p_response, len - 1, "{ \"errorCode\": \"%d\", \"errorMessage\": \"%s\" }", errcode, errmsg );
	
	return 0;
}

