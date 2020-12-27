/*
 * 
 * author	: jack
 * email	: xulongzhong2004@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include <sys/prctl.h>
#include "plugin.h"
#include "LOG.h"
#include "LOGS.h"

#define SET_ERROR_RESPONSE( _response, _errcode, _errmsg ) \
	if( _errcode == 0 || _errcode == HTTP_OK ) \
	{ \
		sprintf( _response, "{\"errorCode\":\"%d\",", 0 ); \
	} \
	else \
	{ \
		sprintf( _response, "{\"errorCode\":\"%d\",", _errcode ); \
	} \
	if( _errmsg != NULL ) \
	{ \
		strcat( _response , "\"errorMessage\":\"" ); \
		strcat( _response ,_errmsg ); \
		strcat( _response ,"\"" ); \
	}\
	else \
	{ \
		strcat( _response , "\"errorMessage\": null" ); \
	} \
	strcat( _response, "}" );

Cplugin* Cplugin::_this = NULL;

Cplugin::Cplugin()
{

}

Cplugin::~Cplugin()
{

}

int Cplugin::Doworker( struct AcceptedSession *p_session )

{	char		*p_content = NULL;
	int		value_len;
	char		nret;
	char		*p_uri = NULL;
	int		uri_len;
	struct HttpEnv  *p_httpEnv = NULL;
	char		*p_response = NULL;
	char		err_msg[256];
	
	memset( err_msg, 0, sizeof(err_msg) );
	p_httpEnv = GetSessionHttpEnv( p_session );
	p_response = GetSessionResponse( p_session );
	p_uri = GetHttpHeaderPtr_URI( p_httpEnv , & uri_len );
	INFOLOGSG( "uri[%.*s]" , uri_len , p_uri );
	
	p_content = QueryHttpHeaderPtr( p_httpEnv , HTTP_HEADER_CONTENT_TYPE , & value_len );
	if( !STRNICMP( p_content , == , HTTP_HEADER_CONTENT_TYPE_JSON , sizeof(HTTP_HEADER_CONTENT_TYPE_JSON) -1 ) )
	{
		snprintf( err_msg, sizeof(err_msg)-1, "content_type[%.*s] Is InCorrect" , value_len , p_content );
		ERRORLOGSG( err_msg );
		SET_ERROR_RESPONSE( p_response, -1, err_msg );
		return -1;
	}

	if( uri_len == sizeof(URI_SEQUENCE)-1 && STRNICMP( p_uri , == , URI_SEQUENCE , uri_len ) )
	{
		//正常处理
	}
	else
	{
		snprintf( err_msg, sizeof(err_msg)-1, "uri unknown uri[%.*s]" , uri_len , p_uri );
		ERRORLOGSG( err_msg  ); 
		SET_ERROR_RESPONSE( p_response, -1, err_msg );
		return -1;
	}
	
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

int Cplugin::Load( )
{	
	int nret = Cplugin::Instance()->Load();
	if( nret )
	{
		ERRORLOGSG( "Load falied nret[%d]", nret );
		return -1;
	}
	
	INFOLOGSG( "Load ok" );
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

int Doworker( struct AcceptedSession *p_session )
{
	int nret = Cplugin::Instance()->Doworker( p_session );
	if( nret )
	{
		ERRORLOGSG( "Cplugin::Instance()->doworker() falied nret[%d]", nret );
		return -1;
	}
	
	INFOLOGSG( "Cplugin::Instance()->doworker ok " );
	
	return 0;
}


