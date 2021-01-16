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


int Doworker( struct AcceptedSession *p_session )
{
	INFOLOGSG( "plugin doworker" );
	return 0;
}


