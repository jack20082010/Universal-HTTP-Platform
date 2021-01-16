#include "plugin.h"
#include "dbsqlca.h"
#include "LOGS.h"

static int ConnectDB()
{
	CDbSqlcaPool 	*p_pool = NULL;
	int		nret;
	int		port = UHPGetDBPort();
	char		*p_user = UHPGetDBUserName();
	char		*p_pwd = UHPGetDBPassword();
	char		*p_host = UHPGetDBIp();
	char		*p_dbname = UHPGetDBName();
	int		minConnections = UHPGetMinConnections();
	int		maxConnections = UHPGetMaxConnections();
	
	if( minConnections <= 0 || maxConnections <= 0 )
	{
		ERRORLOGSG( "连接池个数配置错误：minConnections ConfigInt2[%d] minConnections ConfigInt3[%d]", minConnections, maxConnections ); 
		return -1;
	}
	
	if( port <= 0 )
		port = 3306;

	p_pool = new CDbSqlcaPool( p_user, p_pwd, p_host, p_dbname, port );
	if( !p_pool )
	{
		ERRORLOGSG( "new CDbSqlcaPool failed" );
	}
	
	p_pool->SetMinConnections( minConnections );
	p_pool->SetMaxConnections( maxConnections );
	nret = p_pool->ConnectDB( minConnections );
	if( nret )
	{
		ERRORLOGSG( "ConnectDB failed errCode[%d] errMsg[%s] user[%s] pwd[%s] host[%s] dbname[%s] port[%d]",
			 p_pool->GetLastErrCode(), p_pool->GetLastErrMsg(), p_user, p_pwd, p_host, p_dbname, port );
		delete p_pool;
		return -1;
	}
	
	UHPSetDBPool( p_pool );

	return 0;
}


int Load()
{
	int nret;
	
	nret = ConnectDB();
	INFOLOGSG( "plugin load nret[%d]", nret );
	
	return nret;
}

int Unload()
{
	CDbSqlcaPool 	*p_pool = (CDbSqlcaPool*)UHPGetDBPool();
	if( p_pool )
		delete p_pool;
	
	INFOLOGSG( "plugin unload clean" );
	
	return 0;
}


int Doworker( AcceptedSession *p_session )
{
	CDbSqlcaPool 	*p_pool = (CDbSqlcaPool*)UHPGetDBPool();
	if( p_pool )
		p_pool->HeartBeat();
		
	INFOLOGSG( "plugin doworker" );
	
	return 0;
}

int OnRequest( AcceptedSession *p_session )
{
	INFOLOGSG( "plugin OnRequest" );
	
	return 0;
}


int OnResponse( AcceptedSession *p_session )
{
	INFOLOGSG( "plugin OnResponse" );
	
	return 0;
}



