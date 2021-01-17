#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include "dbsqlca.h"
#include "dbutil.h"
#include "seq_sdk.h"

CDbSqlcaPool 		*m_pdbpool = NULL;

int test_insert( char* seq_name, unsigned long long seq_val )
{
	char			sql[200];
	CDbSqlca 		*pDbSqlca = NULL ;
	int 			nret;
	
	pDbSqlca = m_pdbpool->GetIdleDbSqlca( );
	if( !pDbSqlca )
	{
		printf("获取数据库连接失败：errCode[%d] errMsg[%s]", m_pdbpool->GetLastErrCode(), m_pdbpool->GetLastErrMsg() );
		return -1;
	}
	
	strncpy( sql, "insert into bs_seq_test( id, seq_name ) values( ?,? ) ", sizeof(sql)-1 );
	pDbSqlca->BindIn( 1,  seq_val );
	pDbSqlca->BindIn( 2, seq_name );
	nret = pDbSqlca->ExecSql( sql );
	if( nret < 0 )
	{
		printf("Sql[%s] seq_name[%s] seq_val[%llu] failed errCode[%d] errMsg[%s]\n", sql,  seq_name, seq_val, pDbSqlca->GetDbErrorCode(),pDbSqlca->GetDbErrorMsg() );
		pDbSqlca->Rollback();
		m_pdbpool->ReleaseDbsqlca( pDbSqlca );
		return -1;
	}
	
	pDbSqlca->Commit();
	m_pdbpool->ReleaseDbsqlca( pDbSqlca );
	
	return 0;
}

void*  Thread_working( void *arg )
{
	int 	i ;
	int	nret;
	int	thread_no=0;                            
	unsigned long long seq_val = 0;
	char	seq_name[65];
	
	for( i = 0; i < 1000000; i++ )
	{
	        thread_no = i%10;
	   
		memset( seq_name, 0, sizeof(seq_name) );
		sprintf( seq_name, "seq_test%d", thread_no+1 );
		
		seq_val = GetSequence( seq_name );
		if( seq_val <= 0 )
		{
			printf("GetSequence failed GetSequence[%llu]\n", seq_val);
			return NULL;
		}
	
		nret = test_insert( seq_name, seq_val );
		if( nret  )
		{
			printf("GetBatSequence failed GetSequence[%llu]\n", seq_val);
			return NULL;
		}			

		printf("Tid[%ld] start_val[%llu]\n", pthread_self(), seq_val );
	}

     return 0;
}

int main()
{
//	unsigned long long seq_val = 0;
	int nret;
	int i;
	pthread_t ntid;
	CDbSqlcaPool 	*p_pool = NULL;
	
	nret = SetServerUrl("192.168.56.129:10608");
	if( nret )
	{
		printf("SetServerUrl failed nret[%d]\n", nret );
		return -1;
	}
	SetConnectTimeout( 10 );
	
	p_pool = new CDbSqlcaPool( "mysqldb", "mysqldb", "192.168.56.129", "mysqldb", 3306 ); 
	if( !p_pool )
	{
		printf( "new CDbSqlcaPool failed" );
	}
	
	p_pool->SetMinConnections( 1 );
	p_pool->SetMaxConnections( 100 );
	nret = p_pool->ConnectDB( 1 );
	if( nret )
	{
		printf( "ConnectDB failed errCode[%d] errMsg[%s]\n", p_pool->GetLastErrCode(), p_pool->GetLastErrMsg() );
		delete p_pool;
		return -1;
	}
	m_pdbpool = p_pool;
	
	for( i = 0; i < 32; i++ )
	{
		pthread_create( &ntid, NULL, Thread_working, &i );
	}  

	printf("end\n");
	      
	getchar();
		
	return 0;
}
