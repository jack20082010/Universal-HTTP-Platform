#include "plugin.h"
#include "dbsqlca.h"
#include "LOGS.h"

int load()
{

	INFOLOGSG( "plugin load" );
	return 0;
}

int unload()
{
	INFOLOGSG( "plugin unload clean" );
	return 0;
}


int doworker( struct AcceptedSession *p_session )
{
	INFOLOGSG( "plugin doworker" );
	return 0;
}


