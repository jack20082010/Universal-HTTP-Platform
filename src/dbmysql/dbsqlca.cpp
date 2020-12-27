/*
 * author	: jack
 * email	: xulongzhong2004@163.com
 *
 * Licensed under Apache Licence 2.0, see the file LICENSE in base directory.
 */

#include "dbsqlca.h"

CDbSqlca::CDbSqlca()
{
}

CDbSqlca::~CDbSqlca()
{
	
	m_ClassSqlca.Disconnect();
}

int CDbSqlca::Connect( char* sUser, char* sPassWd, char* sSvrName, const char *dbname, int port )
{
	int nRtn = m_ClassSqlca.Connect( sUser, sPassWd, sSvrName, dbname, port );
	if( nRtn != DB_SUCCESS )
	{
		return nRtn;
	}

	return nRtn;
}

int CDbSqlca::Disconnect()
{
	Rollback();
	m_ClassSqlca.Disconnect();

	return 0;
}
//DBExecSql 可变参数必须传递地址
int CDbSqlca::ExecSql( char* szSQL ) 
{
	int nRtn = DB_SUCCESS;

	nRtn = m_ClassSqlca.ExecuteSql( szSQL );
	if( nRtn < 0 )
	{
		return nRtn;
	}

	return nRtn;
}


int CDbSqlca::ExecuteArraySql( char* szSQL, int nCount, int pvskip )
{
	int nRtn = DB_SUCCESS;

	nRtn = m_ClassSqlca.ExecuteArraySql( szSQL, nCount, pvskip );
	if( nRtn < 0 )
	{
		return nRtn;
	}

	return nRtn;

}

int  CDbSqlca::Commit( bool bDestroyStmthp )
{
	m_ClassSqlca.SqlCommit();

	return DB_SUCCESS;
}

int CDbSqlca::Rollback( bool bDestroyStmthp )
{
	m_ClassSqlca.SqlRollback();

	return DB_SUCCESS;
}

void* CDbSqlca::OpenCursor( char *szSQL,int pvskip )
{
	int nRtn = m_ClassSqlca.ExecuteSql( szSQL, true, pvskip );
	if( nRtn < 0 )
	{
		
		return NULL;
	}
	
	return m_ClassSqlca.GetCurStmtHandle();
}

int  CDbSqlca::FetchData( int nFetchRow, void *pStmthandle )
{
	int nRtn = 0;

	nRtn = m_ClassSqlca.FetchData( nFetchRow, 0, 0, pStmthandle );
	if( nRtn < 0 )
	{
		//统一在外边释放这里不再做关闭操作
		m_ClassSqlca.SetCurStmtHandle( pStmthandle );
		return DB_FAILURE;
	}

	return nRtn;
}

int CDbSqlca::CloseCursor( void *pStmthandle )
{
	m_ClassSqlca.SetCurStmtHandle(pStmthandle);
	
	return 0;
}



STBindParamVec CDbSqlca::GetCurBindInVec()
{
	STBindParamVec BindInVec;

	STBindParamMap::iterator itBIn = m_ClassSqlca.m_mapBindInParam.find( m_ClassSqlca.m_stmthp );
	if( itBIn != m_ClassSqlca.m_mapBindInParam.end() )
	{
		BindInVec = itBIn->second;
	}

	return BindInVec;
}

STBindParamVec CDbSqlca::GetCurBindOutVec()
{
	STBindParamVec BindInVec;

	STBindParamMap::iterator itBout = m_ClassSqlca.m_mapBindOutParam.find( m_ClassSqlca.m_stmthp );
	if( itBout != m_ClassSqlca.m_mapBindOutParam.end() )
	{
		BindInVec = itBout->second;
	}

	return BindInVec;
}


int CDbSqlca::BindIn( STBindParamVec BinVec )
{
	STBindParamMap::iterator itBIn = m_ClassSqlca.m_mapBindInParam.find( NULL );
	if( itBIn != m_ClassSqlca.m_mapBindInParam.end() )
	{
		m_ClassSqlca.m_mapBindInParam.erase( itBIn );
	}

	m_ClassSqlca.m_mapBindInParam[NULL] = BinVec;

	return DB_SUCCESS;
}

int CDbSqlca::BindOut( STBindParamVec BinOutVec )
{
	STBindParamMap::iterator itBInOut = m_ClassSqlca.m_mapBindOutParam.find( NULL );
	if( itBInOut != m_ClassSqlca.m_mapBindOutParam.end() )
	{
		m_ClassSqlca.m_mapBindOutParam.erase( itBInOut );
	}

	m_ClassSqlca.m_mapBindOutParam[NULL] = BinOutVec;

	return DB_SUCCESS;
}

//设置列的属性
int CDbSqlca::SetColAttr( const char* strSql )
{
	int nRtn = 0;
	
	if( NULL == m_ClassSqlca.m_stmthp )
	{
		nRtn = m_ClassSqlca.PrepareSql( strSql );
		if ( nRtn != DB_SUCCESS )
		{
			Destory_stmthp();
			return DB_FAILURE;
		}

		nRtn = m_ClassSqlca.Execute( 0, 0) ;
		if( nRtn != DB_SUCCESS )
		{
			Destory_stmthp();
			return DB_FAILURE;
		}

	}

	nRtn = m_ClassSqlca.SetColPropty();
	if( nRtn != DB_SUCCESS )
	{
		Destory_stmthp();
		return DB_FAILURE;
	}
	//不清空属性队列
	m_ClassSqlca.Destory_stmthp( false );

	return DB_SUCCESS;
}



int CDbSqlca::GetColType( int nColIndex )
{
	if( nColIndex < 0 || 0 == m_ClassSqlca.m_stSqlPropty.vecColPropty.size() ) return DB_FAILURE;

	return m_ClassSqlca.m_stSqlPropty.vecColPropty[nColIndex-1].nColType;
}

char* CDbSqlca::GetColName(int nColIndex)
{
	if( nColIndex < 0 || 0 == m_ClassSqlca.m_stSqlPropty.vecColPropty.size() ) return "";

	return m_ClassSqlca.m_stSqlPropty.vecColPropty[nColIndex-1].sColName;
}

int CDbSqlca::GetColSize( int nColIndex )
{
	if( nColIndex < 0 || 0 == m_ClassSqlca.m_stSqlPropty.vecColPropty.size() ) return DB_FAILURE;

	return m_ClassSqlca.m_stSqlPropty.vecColPropty[nColIndex-1].nColLen;
}
int CDbSqlca::GetColScale(int nColIndex)
{
	if( nColIndex < 0 || 0 == m_ClassSqlca.m_stSqlPropty.vecColPropty.size() ) return DB_FAILURE;

	return m_ClassSqlca.m_stSqlPropty.vecColPropty[nColIndex-1].nColScale;
}


//---------------------------------dbsqlca连接池--------------------------------------

CDbSqlcaPool::CDbSqlcaPool( const char* username, const char* password, const char* dblink, const char* dbname, int port )
{
	m_nMinConnections = 1;
	m_nMaxConnections = 10;
	m_port = port;
	strcpy( m_sLinkstr, dblink );
	strcpy( m_sUserName, username );
	strcpy( m_sPassword, password );
	if( dbname != NULL )
		strcpy( m_sDbname, dbname );
	m_errCode = 0;
	memset( m_errMsg, 0, sizeof(m_errMsg) );
	m_nTimeout = 3000;
	m_idleSleep = 10;

}

CDbSqlcaPool::~CDbSqlcaPool()
{
	ClearAllDbSqlca();
}

CDbSqlca* CDbSqlcaPool::ConnectDB()
{
	CDbSqlca *pDbObject = new CDbSqlca;
	
	if( NULL == pDbObject)
	{
		m_errCode = -1;
		strncpy( m_errMsg, "new CociSqlca 内存分配错误", sizeof(m_errMsg) );
		return NULL;
	}

	if( DB_SUCCESS != pDbObject->Connect( m_sUserName, m_sPassword, m_sLinkstr, m_sDbname, m_port ) )
	{
		m_errCode = pDbObject->GetDbErrorCode();
		strncpy( m_errMsg, pDbObject->GetDbErrorMsg(), sizeof(m_errMsg) );
		delete pDbObject;

		return NULL;
	}

	return pDbObject;
}

void CDbSqlcaPool::SetConTimeout( int millisecond )
{
	m_nTimeout = millisecond;

}

void CDbSqlcaPool::SetIdleSleep( int millisecond )
{
	m_idleSleep = millisecond;
}

int CDbSqlcaPool::GetLastErrCode( )
{
	return m_errCode ;

}

char*  CDbSqlcaPool::GetLastErrMsg( )
{
	return m_errMsg ;
}

int CDbSqlcaPool::ConnectDB( int nNum )
{
	CDbSqlca*  pDbSqlca = NULL;
	for( int i = 0; i < nNum; i++ )
	{
		pDbSqlca = ConnectDB();
		if( NULL == pDbSqlca )
		{
			return DB_FAILURE;
		}

		CAutoLock lock( m_deqDbSqlcaLock );
		m_deqDbSqlca.push_back( pDbSqlca );
	}


	m_nCurConnections = nNum;
	m_nMinConnections = nNum;
	if( m_nMaxConnections < m_nMinConnections )
		m_nMaxConnections = m_nMinConnections;

	return DB_SUCCESS;
}

int CDbSqlcaPool::ClearAllDbSqlca()
{
	CAutoLock lock( m_deqDbSqlcaLock );

	int nSize = m_deqDbSqlca.size();
	for(int i = 0; i < nSize; i++ )
	{
		CDbSqlca*  pDbSqlca = m_deqDbSqlca[i];
		delete pDbSqlca;
	}
	m_deqDbSqlca.clear();

	map< CDbSqlca*,CDbSqlca* >::iterator iter = m_mapDbSqlca_Resume.begin();
	while( iter != m_mapDbSqlca_Resume.end() )
	{
		CDbSqlca*  pDbSqlca =iter->first;
		delete pDbSqlca;
		iter++;
	}

	m_mapDbSqlca_Resume.clear();

	return DB_SUCCESS;
}

int CDbSqlcaPool::SetMaxConnections( int nNum )
{
	CAutoLock lock( m_deqDbSqlcaLock );
	m_nMaxConnections = nNum;

	return DB_SUCCESS;
}

int CDbSqlcaPool::SetMinConnections( int nNum )
{
	CAutoLock lock( m_deqDbSqlcaLock );
	m_nMinConnections = nNum;

	return DB_SUCCESS;
}

CDbSqlca* CDbSqlcaPool::GetIdleDbSqlca()
{
	CDbSqlca* 	pDbSqlca = NULL;
	int 		cost_time = 0;
	struct timeval	tv_start, tv_now ;
	
	gettimeofday( &tv_start, NULL );
	while(1)
	{
		CAutoLock lock( m_deqDbSqlcaLock );
		if( !m_deqDbSqlca.empty() )
		{
			pDbSqlca = m_deqDbSqlca.front();
			m_deqDbSqlca.pop_front();
			m_mapDbSqlca_Resume[pDbSqlca] = pDbSqlca;
		}
		else
		{
			if( m_nCurConnections < m_nMaxConnections )
			{
				pDbSqlca = ConnectDB();			
				if( NULL != pDbSqlca )
				{
					m_mapDbSqlca_Resume[pDbSqlca] = pDbSqlca;
					m_nCurConnections++;
				}
			}
		}
		lock.Unlock();

		if( NULL != pDbSqlca )
		{
			break;
		}
		
		gettimeofday( &tv_now, NULL );
		cost_time = ( tv_now.tv_sec - tv_start.tv_sec )*1000 + ( tv_now.tv_usec - tv_start.tv_usec )/1000;
		if( cost_time > m_nTimeout )
		{
			m_errCode = -1;
			snprintf( m_errMsg, sizeof(m_errMsg) -1, "获取连接超时: cost_time[%d]ms", cost_time );
			return NULL;
		}
		
		usleep( m_idleSleep );
		
	}

	return pDbSqlca;
}


int CDbSqlcaPool::ReleaseDbsqlca( CDbSqlca* pDbSqlca )
{
	if( pDbSqlca )
	{
		CAutoLock lock( m_deqDbSqlcaLock );
		map< CDbSqlca*,CDbSqlca* >::iterator iter = m_mapDbSqlca_Resume.find( pDbSqlca );
		if( iter != m_mapDbSqlca_Resume.end() )
		{
			CDbSqlca*  pDbSqlca = iter->first;
			m_deqDbSqlca.push_front( pDbSqlca );
			m_mapDbSqlca_Resume.erase( iter );

		}
	}

	return DB_SUCCESS;
}

int CDbSqlcaPool::GetConnections()
{
	CAutoLock lock( m_deqDbSqlcaLock );
	return m_nCurConnections;
}

int CDbSqlcaPool::GetUsedConnections()
{
	CAutoLock lock( m_deqDbSqlcaLock );
	int nUsed = m_mapDbSqlca_Resume.size();

	return nUsed;
}

int CDbSqlcaPool::HeartBeat()
{
	struct timeval tv_time;
	CDbSqlca* pDbSqlca = NULL;
	
	CAutoLock lock( m_deqDbSqlcaLock );
	int nNoUsed = m_deqDbSqlca.size();
	int nUsed = m_mapDbSqlca_Resume.size();
	int nSum = nUsed + nNoUsed;

	if( nSum <= m_nMinConnections || nNoUsed < m_nMinConnections )
	{
		return DB_SUCCESS;
	}

  	gettimeofday( &tv_time, NULL );
	pDbSqlca = m_deqDbSqlca.back(); //控制5分钟不使用删除
	if( tv_time.tv_sec * 1000 + tv_time.tv_usec / 1000 - pDbSqlca->m_ClassSqlca.GetLastUseTime() > 5*60*1000 )
	{
		//printf("delet num=%d\n",nSum);
		if( nSum == m_nMinConnections + 1 )
		{
			pDbSqlca->m_ClassSqlca.DestroyFreeMem();
		}
		m_deqDbSqlca.pop_back();
		delete pDbSqlca;

		m_nCurConnections--;
	}

	//printf("heartbeat num=%d\n",nSum);
	return DB_SUCCESS;
}

