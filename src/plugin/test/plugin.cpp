#include "plugin.h"
#include "dbsqlca.h"
#include "LOGS.h"

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


int Doworker( AcceptedSession *p_session )
{
	INFOLOGSG( "plugin doworker" );
	return 0;
}

int OnException( AcceptedSession *p_session, int errcode, char *errmsg )
{
	char	*p_response = NULL;
	int	len;
	
	len = GetSessionResponseSize( p_session );
	p_response = GetSessionResponse( p_session );
	snprintf( p_response, len - 1, "{ \"errorCode\": \"%d\", \"errorMessage\": \"%s\" }", errcode, errmsg );
	
	return 0;
}

