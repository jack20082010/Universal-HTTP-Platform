#include "dbsqlca.h"
#include "dbutil.h"
#include "threadpool.h"

unsigned long long GetSequence( char *name );
int ThreadWorker( void *arg, int threadno );

class SeqAtrr
{
public:
	char 			name[65];
	CriSection   		lock;	
	unsigned long long	cur_val;
	unsigned long long	max_val;
	int			step;
	int			alert_diff;
	int		  	status;
	int			is_pulling;
	int			batch_cache;  //内存cache个数,用于批量获取
	void			*arg;
};

typedef map< string, SeqAtrr >  StrSeqAtrrMap;
typedef map< int, SeqAtrr, greater<int> >  IntSeqAtrrMap;

class BatSeqAtrr
{
public:
	char 			name[65];
	CriSection   		lock;
	int		  	status;
	int			batch_cache;  //内存cache个数,用于批量获取
	IntSeqAtrrMap		batch_map;
};

typedef map< string, BatSeqAtrr > StrSeqSortMap;

CriSection 	m_map_seq_lock;  
StrSeqAtrrMap 	m_map_seq;
CriSection	m_map_seq_next_lock;  
StrSeqAtrrMap	m_map_seq_next;


CriSection 	m_map_batseq_lock;  
StrSeqSortMap 	m_map_batseq;
	


int			g_exit_flag;
CDbSqlcaPool 		*m_pdbpool = NULL;
threadpool_t		*p_threadpool;

#define 		DEFAULT_PULL_COUNT 	1000

typedef struct _STSequence
{
	long long id;
	char	seq_name[65];
	char	seq_desc[81];
	long	seq_type;
	long long  min_val;
	long long  max_val;
	long long cur_val;
	int step;
	int cycle;
	int status;
	char create_time[21];
	char update_time[21];
	char remark[101];
	
}STSequence;

int test_select()
{
	int 		nret;
	char		sql[100];
	STSequence      STSeq = {0};
	
        CDbSqlca * pDbSqlca = new CDbSqlca ;
        
        nret = pDbSqlca->Connect( "mysqldb", "mysqldb", "192.168.56.129", "mysqldb", 3306 );
        if( nret )
        {
        	printf("获取数据库连接失败：errCode[%d] errMsg[%s]\n", pDbSqlca->GetDbErrorCode(), pDbSqlca->GetDbErrorMsg() );
		return -1;
        }
        
        memset( sql, 0, sizeof(sql) );
	strncpy( sql, "select * from bs_seq_rule where seq_name = ? ", sizeof(sql)-1 );
        
        nret = pDbSqlca->BindIn( 1, "seq_test" );
        //nret = pDbSqlca->BindIn( 2, 1 );
        
	nret = pDbSqlca->BindOut( 1, "%lld", &STSeq.id, sizeof(STSeq.id) );
	nret = pDbSqlca->BindOut( 2, "%s", STSeq.seq_name, sizeof(STSeq.seq_name) );
	nret = pDbSqlca->BindOut( 3, "%s", STSeq.seq_desc, sizeof(STSeq.seq_desc) );
	nret = pDbSqlca->BindOut( 4, "%d", &STSeq.seq_type, sizeof(STSeq.seq_type) );
	nret = pDbSqlca->BindOut( 5, "%lld", &STSeq.min_val, sizeof(STSeq.min_val) );
	nret = pDbSqlca->BindOut( 6, "%lld", &STSeq.max_val, sizeof(STSeq.max_val) );
	nret = pDbSqlca->BindOut( 7, "%lld", &STSeq.cur_val, sizeof(STSeq.cur_val) );
	nret = pDbSqlca->BindOut( 8, "%ld", &STSeq.step, sizeof(STSeq.step) );
	nret = pDbSqlca->BindOut( 9, "%u", &STSeq.cycle, sizeof(STSeq.cycle) );
	nret = pDbSqlca->BindOut( 10, "%lu", &STSeq.status, sizeof(STSeq.status) );
	nret = pDbSqlca->BindOut( 11, "%s", STSeq.create_time, sizeof(STSeq.create_time) );
	nret = pDbSqlca->BindOut( 12, "%s", STSeq.update_time, sizeof(STSeq.update_time) );
	nret = pDbSqlca->BindOut( 13, "%s", STSeq.remark, sizeof(STSeq.remark) );
	
	
	//nret = pDbSqlca->BindOut( 1, "%lld", &STSeq.id );
	//nret = pDbSqlca->BindOut( 2, "%s", STSeq.seq_name );
	//nret = pDbSqlca->BindOut( 3, "%s", STSeq.seq_desc );
	//nret = pDbSqlca->BindOut( 4, "%ld", &STSeq.seq_type );
	//nret = pDbSqlca->BindOut( 5, "%lld", &STSeq.min_val );
	//nret = pDbSqlca->BindOut( 6, "%lld", &STSeq.max_val );
	//nret = pDbSqlca->BindOut( 7, "%lld", &STSeq.cur_val );
	//nret = pDbSqlca->BindOut( 8, "%d", &STSeq.step );
	//nret = pDbSqlca->BindOut( 9, "%d", &STSeq.cycle );
	//nret = pDbSqlca->BindOut( 10, "%d", &STSeq.status );
	//nret = pDbSqlca->BindOut( 11, "%s", STSeq.create_time );
	//nret = pDbSqlca->BindOut( 12, "%s", STSeq.update_time );
	//nret = pDbSqlca->BindOut( 13, "%s", STSeq.remark );
	
	//nret = pDbSqlca->ExecSql( "select id, seq_name, seq_desc, seq_type, min_val, max_val, cur_val, step, cycle, \
				status, create_time, update_time, remark   from bs_seq_rule where  id = ? " );
	nret = pDbSqlca->ExecSql( sql );
	printf("id[%lld]\n", STSeq.id);
	printf("seq_name[%s]\n", STSeq.seq_name);
	printf("seq_desc[%s]\n", STSeq.seq_desc);
	printf("seq_type[%d]\n", STSeq.seq_type);
	printf("min_val[%lld]\n", STSeq.min_val);
	printf("max_val[%lld]\n", STSeq.max_val);
	printf("cur_val[%lld]\n", STSeq.cur_val);
	printf("step[%d]\n", STSeq.step);
	printf("cycle[%d]\n", STSeq.cycle);
	printf("status[%d]\n", STSeq.status);
	printf("create_time[%s]\n", STSeq.create_time);
	printf("update_time[%s]\n", STSeq.update_time);
	printf("remark[%s]\n", STSeq.remark);
	
	if( nret < 0 )
	{
		printf("ExecSql[%s] failed errCode[%d] errMsg[%s]\n", sql,  pDbSqlca->GetDbErrorCode(),pDbSqlca->GetDbErrorMsg() );
		return -1;
	}
	
	delete pDbSqlca;
	
	return 0;
}

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

int test_cursor( char *name, StrSeqAtrrMap *p_seqMap )
{
	int 			nret;
	char			sql[200];
	char			update_sql[100];
	unsigned long long 	cur_val = 0;
	unsigned long long 	max_val = 0;
	unsigned long long 	min_val = 0;
	unsigned long long 	update_val = 0;
	int			alert_diff = 0;
	int			cache = 0;
	int			step = 0;
	int 			cycle = 0;
	SeqAtrr			atr_seq;
	int			errCode;
	CDbSqlca 		*pDbSqlca = NULL ;
	
	pDbSqlca = m_pdbpool->GetIdleDbSqlca( );
	if( !pDbSqlca )
	{
		printf("获取数据库连接失败：errCode[%d] errMsg[%s]", m_pdbpool->GetLastErrCode(), m_pdbpool->GetLastErrMsg() );
		return -1;
	}
        
        memset( sql, 0, sizeof(sql) );
	strncpy( sql, "select seq_name, status, cycle, cur_val, max_val, min_val, step, cache, alert_diff from bs_seq_rule for update ", sizeof(sql)-1 );
		
	pDbSqlca->BindOut( 1, "%s", atr_seq.name );
	pDbSqlca->BindOut( 2, "%d", &atr_seq.status );
	pDbSqlca->BindOut( 3, "%d", &cycle );
	pDbSqlca->BindOut( 4, "%llu", &cur_val );
	pDbSqlca->BindOut( 5, "%llu", &max_val );
	pDbSqlca->BindOut( 6, "%llu", &min_val );
	pDbSqlca->BindOut( 7, "%d", &step );
	pDbSqlca->BindOut( 8, "%d", &cache );
	pDbSqlca->BindOut( 9, "%d", &alert_diff );
	
	void *pStmthandle = pDbSqlca->OpenCursor( sql );
	if( pStmthandle == NULL )
	{
		printf("Sql[%s] OpenCursor failed errCode[%d] errMsg[%s]\n", sql,  pDbSqlca->GetDbErrorCode(),pDbSqlca->GetDbErrorMsg() );
		pDbSqlca->Rollback();
		m_pdbpool->ReleaseDbsqlca( pDbSqlca );
		return -1;
	}
	while( 1 )
	{
		int nAffectRows = pDbSqlca->FetchData( 1, pStmthandle );
		if( nAffectRows < 0 )
		{
			printf("Sql[%s] FetchData failed errCode[%d] errMsg[%s]\n", sql,  pDbSqlca->GetDbErrorCode(),pDbSqlca->GetDbErrorMsg() );
			pDbSqlca->Rollback();
			pDbSqlca->CloseCursor( pStmthandle );
			m_pdbpool->ReleaseDbsqlca( pDbSqlca );
			return -1;
		}
		
		errCode = pDbSqlca->GetDbErrorCode();
		if( errCode == SQLNOTFOUND )
		{
			printf("break sql[%s]\n", sql);
			break;
		}
	
		//todo
		
	}
		
	pDbSqlca->CloseCursor( pStmthandle );
	pDbSqlca->Commit();
	m_pdbpool->ReleaseDbsqlca( pDbSqlca );
	
	return 0;
}


int test_updte( char *name, StrSeqAtrrMap *p_seqMap )
{
	int 			nret;
	char			update_sql[100];
	unsigned long long 	update_val = 0;
	
	int			errCode;
	CDbSqlca 		*pDbSqlca = NULL ;
	
	pDbSqlca = m_pdbpool->GetIdleDbSqlca( );
	if( !pDbSqlca )
	{
		printf("获取数据库连接失败：errCode[%d] errMsg[%s]", m_pdbpool->GetLastErrCode(), m_pdbpool->GetLastErrMsg() );
		return -1;
	}
        
        /*持久化到数据库*/
	memset( update_sql, 0, sizeof(update_sql) );
	strncpy( update_sql, "update bs_seq_rule set cur_val = ? where seq_name = ? ", sizeof(update_sql)-1 );
	pDbSqlca->BindIn( 1,  update_val );
	pDbSqlca->BindIn( 2,  name );
	nret = pDbSqlca->ExecSql( update_sql );
	if( nret < 0 )
	{
		printf("sql[%s] seq_name[%s] failed errCode[%d] errMsg[%s]\n", update_sql,  name, pDbSqlca->GetDbErrorCode(),pDbSqlca->GetDbErrorMsg() );
		pDbSqlca->Rollback();
		m_pdbpool->ReleaseDbsqlca( pDbSqlca );
		return -1;
	}
		
	pDbSqlca->Commit();
	m_pdbpool->ReleaseDbsqlca( pDbSqlca );
	
	return 0;
}

int PullOneSeqAtrr( SeqAtrr *p_seq, int batCount = -1 )
{
	int 			nret;
	char			sql[200];
	char			update_sql[100];
	unsigned long long 	cur_val = 0;
	unsigned long long 	max_val = 0;
	unsigned long long 	min_val = 0;
	unsigned long long 	update_val = 0;
	int			alert_diff = 0;
	int			cache = 0;
	int			batch_cache = 0;
	int			step = 0;
	int 			cycle = 0;
	int			status = 0;
	int			errCode;
	CDbSqlca 		*pDbSqlca = NULL ;
	
	if( !p_seq )
	{
		printf("参数非法\n");
		return -1;
	}

	pDbSqlca = m_pdbpool->GetIdleDbSqlca( );
	if( !pDbSqlca )
	{
		printf("获取数据库连接失败：errCode[%d] errMsg[%s]\n", m_pdbpool->GetLastErrCode(), m_pdbpool->GetLastErrMsg() );
		return -1;
	}
        
	memset( sql, 0, sizeof(sql) );
	strncpy( sql, "select status, cycle, cur_val, max_val, min_val, step, cache, alert_diff, batch_cache from bs_seq_rule where seq_name = ? for update ", sizeof(sql)-1 );
	pDbSqlca->BindIn( 1, p_seq->name );
	pDbSqlca->BindOut( 1, "%d", &status );
	pDbSqlca->BindOut( 2, "%d", &cycle );
	pDbSqlca->BindOut( 3, "%llu", &cur_val );
	pDbSqlca->BindOut( 4, "%llu", &max_val );
	pDbSqlca->BindOut( 5, "%llu", &min_val );
	pDbSqlca->BindOut( 6, "%d", &step );
	pDbSqlca->BindOut( 7, "%d", &cache );
	pDbSqlca->BindOut( 8, "%d", &alert_diff );
	pDbSqlca->BindOut( 9, "%d", &batch_cache );
	nret = pDbSqlca->ExecSql( sql );
	if( nret < 0 )
	{
		printf("Sql[%s] failed errCode[%d] errMsg[%s]\n", sql,  pDbSqlca->GetDbErrorCode(),pDbSqlca->GetDbErrorMsg() );
		pDbSqlca->Rollback();
		m_pdbpool->ReleaseDbsqlca( pDbSqlca );
		return -1;
	}
	else if( nret == 0 )
	{
		printf("找不到对应记录 sql[%s] seq_name[%s]\n", sql, p_seq->name );
		pDbSqlca->Rollback();
		m_pdbpool->ReleaseDbsqlca( pDbSqlca );
		return -1;
	}
	
	if( batCount > 0 )
		cache = batCount;
	
	update_val = cur_val + step*cache;	
	if(  update_val > max_val )
	{
		if( cycle == 0 )
		{
			printf("超过最大值限制：name[%s] update_val[%llu] max_val[%llu] cur_val[%llu] step[%d] cache[%d]\n", p_seq->name, update_val, max_val, cur_val, step, cache );
			pDbSqlca->Rollback();
			m_pdbpool->ReleaseDbsqlca( pDbSqlca );
			return -1;
		}
		else
		{
			printf("超过最大值被设置为最小值循环使用：name[%s] update_val[%llu] max_val[%llu] cur_val[%llu] step[%d] cache[%d]\n", p_seq->name, update_val, max_val, cur_val, step, cache );
			if( batCount )
			update_val = min_val + step*cache;
		}
	}

	/*持久化到数据库*/
	memset( update_sql, 0, sizeof(update_sql) );
	strncpy( update_sql, "update bs_seq_rule set cur_val = ? where seq_name = ? ", sizeof(update_sql)-1 );
	pDbSqlca->BindIn( 1,  update_val );
	pDbSqlca->BindIn( 2,  p_seq->name );
	nret = pDbSqlca->ExecSql( update_sql );
	if( nret < 0 )
	{
		printf("sql[%s] seq_name[%s] failed errCode[%d] errMsg[%s]\n", update_sql,  p_seq->name, pDbSqlca->GetDbErrorCode(),pDbSqlca->GetDbErrorMsg() );
		pDbSqlca->Rollback();
		m_pdbpool->ReleaseDbsqlca( pDbSqlca );
		return -1;
	}
	
	p_seq->cur_val = cur_val;
	p_seq->max_val = update_val;
	p_seq->alert_diff = alert_diff;
	p_seq->status = status;
	p_seq->step = step;
	p_seq->batch_cache = batch_cache;
	
	pDbSqlca->Commit();
	m_pdbpool->ReleaseDbsqlca( pDbSqlca );

	//printf("Tid[%ld] PullOneSeqAtrr seq_name[%s] cur_val[%llu] update_val[%llu], max_val[%llu] min_val[%llu] \n", pthread_self(), p_seq->name, cur_val, update_val, max_val, min_val );
	
	return 0;
}

int RoundUp( int num, int factor ) 
{
    if( num < 0 || factor <= 0 )
    	return -1;
    else if( num == 0 )
    	return factor;
    
    return ( ( num + factor - 1 ) / factor ) * factor;

}

unsigned long long  PullBatSeqAtrr( char *name, int count, BatSeqAtrr *p_batseq, int *p_step )
{
	unsigned long long 		cur_val = 0;
	SeqAtrr				seq_atrr;
	int				nret;
	StrSeqSortMap::iterator 	it;
	int				pull_count = 3;		/*本次提取数量 2个保留到内存中*/
	
	if( p_batseq )
	{
		if( p_batseq->batch_map.size() > p_batseq->batch_cache )
			pull_count = 1;
	}
	
	//序列被禁用情况，数据库加载时不判断，由加载到map内存判断，提高性能
	strncpy( seq_atrr.name, name, sizeof(seq_atrr.name)-1 );
	nret = PullOneSeqAtrr( &seq_atrr, pull_count*count );
	if( nret )
	{
		printf("PullOneSeqAtrr 失败 nert[%d]\n", nret );
		return 0;
	}

	cur_val = seq_atrr.cur_val;
	seq_atrr.cur_val += count*seq_atrr.step;
	if( p_step )
		*p_step = seq_atrr.step;

	printf("have no cache data 数据库获取 cur_val[%llu], cur_val2[%llu] max_val[%llu]\n", cur_val, seq_atrr.cur_val, seq_atrr.max_val );
	
	if( p_batseq == NULL ) //map not find m_map_batseq_lock
	{
		BatSeqAtrr 	batseq_atrr;
		strncpy( batseq_atrr.name, seq_atrr.name, sizeof(batseq_atrr.name)-1 );
		batseq_atrr.status = seq_atrr.status;
		batseq_atrr.batch_cache = seq_atrr.batch_cache;
		batseq_atrr.batch_map[count] = seq_atrr;
		m_map_batseq[name] = batseq_atrr;
	}
	else
	{
		//map find  p_batseq->lock 
		it = m_map_batseq.find( name );
		if( it != m_map_batseq.end() )
		{
			p_batseq = &( it->second );
			p_batseq->status = seq_atrr.status;
			p_batseq->batch_cache = seq_atrr.batch_cache;

			if( p_batseq->batch_map.size() < seq_atrr.batch_cache )
			{
				printf("batch_map_size[%d], batch_cache[%d]\n", p_batseq->batch_map.size(), seq_atrr.batch_cache );
				p_batseq->batch_map[count] = seq_atrr;
			}
		}
		
	}
	
	return cur_val;
}

unsigned long long GetBatSequence( char *name, int *p_count, int *p_step )
{
	SeqAtrr				*p_seq = NULL;
	BatSeqAtrr			*p_batseq = NULL;
	int				nret;
	unsigned long long 		cur_val = 0;
	bool				batch_pull = true;
	StrSeqSortMap::iterator 	it;
	IntSeqAtrrMap::iterator 	it2;
	
	const int 			factor = 10;		/*向上取整因子*/
	int				count = 0;
	
	if( !name || !p_count || *p_count <= 0 )
	{
		printf("name 参数错误");
		return 0;	
	}
	
	count = *p_count;
	count = RoundUp( count, factor ); //10倍数向上取整，主要是方便后续缓存
	*p_count = count;
		
	CAutoLock lock0( m_map_batseq_lock );
	it = m_map_batseq.find( name );
	if( it != m_map_batseq.end() )
	{
		p_batseq = &( it->second );
		if( p_batseq->status != 0 )
		{
			printf("序列禁用状态\n");
			return 0;	
		}
		lock0.Unlock();

		CAutoLock lock( p_batseq->lock );
		it2 = p_batseq->batch_map.find( count);
		if( it2 != p_batseq->batch_map.end() )
		{
			p_seq = &( it2->second );
			if( p_seq->max_val - p_seq->cur_val >= count*p_seq->step )
			{
				cur_val = p_seq->cur_val;
				if( p_step )
					*p_step = p_seq->step;
				p_seq->cur_val += count*p_seq->step;
				
				printf("have cache data 缓存获取 cur_val[%llu] cur_val2[%llu]\n", cur_val, p_seq->cur_val);
			}
			else
			{
				cur_val = PullBatSeqAtrr( name, count, p_batseq, p_step );
			}
		}
		else
		{
			cur_val = PullBatSeqAtrr( name, count, p_batseq, p_step );
		}
	}
	else
	{
		cur_val = PullBatSeqAtrr( name, count, NULL, p_step );
	}
		
	return cur_val;

}

unsigned long long GetSequence( char *name )
{
	SeqAtrr				*p_seq = NULL;
	int				nret;
	unsigned long long 		cur_val = 0;
	StrSeqAtrrMap::iterator		it;
	StrSeqAtrrMap::iterator		it2;
	
	if( !name )
	{
		printf("name 参数不能为空");
		return 0;	
	}
	
	CAutoLock lock0( m_map_seq_lock );
	it = m_map_seq.find( name );
	if( it != m_map_seq.end() )
	{
		p_seq = &( it->second );
		if( p_seq->status != 0 )
		{
			printf("序列禁用状态\n");
			return 0;	
		}
		lock0.Unlock();   
		
		CAutoLock lock( p_seq->lock );
		if( p_seq->max_val == p_seq->cur_val  ) //异步获取时间差还没完成，直接同步调用
		{
			printf("==== cur_val[%llu] max_val[%llu] is_pulling[%d]\n", p_seq->cur_val, p_seq->max_val, p_seq->is_pulling);
			SeqAtrr		*p_seq_next = NULL;
			
			CAutoLock lock2( m_map_seq_next_lock );
			it2 = m_map_seq_next.find( name );
			if( it2 != m_map_seq_next.end() )
			{
				p_seq_next= &( it2->second);
				p_seq->cur_val = p_seq_next->cur_val;
				p_seq->max_val = p_seq_next->max_val;
				p_seq->alert_diff = p_seq_next->alert_diff;
				p_seq->status = p_seq_next->status;
				p_seq->step = p_seq_next->step;
				m_map_seq_next.erase( it2 );
				p_seq->is_pulling = 0;
				printf("cache have data 获取到缓存数据 cur_val[%llu]  max_val[%llu]\n", p_seq->cur_val, p_seq->max_val );
				
			}
			else
			{
				lock2.Unlock();
				printf("have no cache data is_pulling[%d] 直接从数据库获取数据\n", p_seq->is_pulling);
				
				nret = PullOneSeqAtrr( p_seq );
				if( nret )
				{
					printf("PullOneSeqAtrr 失败 nert[%d]\n", nret );
					return 0;
				}
			}
			

		}
		else if( p_seq->max_val - p_seq->cur_val <= p_seq->alert_diff  && p_seq->is_pulling == 0 )
		{
			taskinfo_t	task ;
			SeqAtrr	 	*p_atr_seq = new SeqAtrr ;
			
			p_seq->is_pulling = 1; //设置正在拉去标志
			p_atr_seq->arg = &( p_seq->is_pulling );
			
			strncpy( p_atr_seq->name, name, sizeof(p_atr_seq->name) -1 );
			memset( &task, 0, sizeof(taskinfo_t) );
			task.fn_callback = ThreadWorker;
			task.arg = p_atr_seq;
			nret = threadpool_addTask( p_threadpool, &task );
			if( nret < 0 )
			{
				printf( "threadpool_addTask failed nret[%d]\n", nret);
				return -1;
			}
			printf( "threadpool_addTask ok cur_val[%llu] max_val[%llu] nret[%d]\n", p_seq->cur_val, p_seq->max_val, nret);
			
		}
		
		cur_val = p_seq->cur_val;
		p_seq->cur_val++;
		
	}
	else
	{      
		SeqAtrr		seq_atrr;
		
		strncpy( seq_atrr.name, name, sizeof(seq_atrr.name)-1 );
		nret = PullOneSeqAtrr( &seq_atrr );
		if( nret )
		{
			printf("PullOneSeqAtrr 失败 nert[%d]\n", nret );
			return 0;
		}
		cur_val = seq_atrr.cur_val;
		seq_atrr.cur_val++;
		seq_atrr.is_pulling = 0; //初始化
		m_map_seq[name] = seq_atrr;
		printf("load seq_name  cur_val[%llu] max_val[%llu]\n", cur_val, seq_atrr.max_val);
	}
	
	//printf("cur_val[%llu] max_val[%llu] alert_diff[%d]\n",  p_seq->cur_val,  p_seq->max_val,  p_seq->alert_diff );
	return cur_val;

}

int ThreadWorker( void *arg, int threadno )
{
	SeqAtrr	 	*p_atr_seq = (SeqAtrr*)arg ;
	int 		nret;
	
	nret = PullOneSeqAtrr( p_atr_seq );
	if( nret )
	{
		int 	*p_is_pulling;
		printf("PullOneSeqAtrr failed nret[%d]\n", nret );
		p_is_pulling =( int*)p_atr_seq->arg;
		p_is_pulling = 0; 	/*失败重新推送*/
		
	}
	else
	{
		CAutoLock lock( m_map_seq_next_lock );
		m_map_seq_next[p_atr_seq->name]= *p_atr_seq;
		printf("Tid[%ld] threadno[%d] cur_val[%llu] max_cur[%llu] 异步拉去数据成功\n",pthread_self() ,threadno,p_atr_seq->cur_val, p_atr_seq->max_val);
		lock.Unlock();
		
	}
	delete p_atr_seq;
	
	return 0;
}

void*  Thread_working( void *arg )
{
	int 	i , j ;
	int	nret;
	int	thread_no=0;
	int	step = 0;                               
	unsigned long long seq_val = 0;
	char	seq_name[65];
	unsigned long long  max_val = 0;
	int	count = 0;
	
	int 	rowcount=0;
	
	for( i = 0; i < 100000; i++ )
	{
	        //thread_no = i%10;
	   
		memset( seq_name, 0, sizeof(seq_name) );
		sprintf( seq_name, "seq_test%d", thread_no+1 );
		
#if 0	
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
#endif		
		//if( i < 10 )
		//	count =  i+11;
		//else
			count = 3 ;  

#if 1
		seq_val = GetBatSequence( seq_name, &count, &step);
		if( seq_val <= 0 )
		{
			printf("GetSequence failed GetSequence[%llu]\n", seq_val);
			return NULL;
		}
		//printf("Tid[%ld] index[%d] seq_val[%llu] step[%d] count[%d]\n",pthread_self(),i, seq_val, step , count );
		
		for( j = 0; j < count; j++ )
		{
			rowcount++;
			
			//printf("rowcount[%d] count[%d] seq_val[%llu] step[%d] max_val[%llu]\n", rowcount, count, seq_val, step , max_val );
			nret = test_insert( seq_name, seq_val );
			if( nret  )
			{
				printf("GetBatSequence failed GetSequence[%llu]\n", seq_val);
				return NULL;
			}
			seq_val += step;  
		}
#endif			
		
		//printf("start_val[%llu] step[%d] max_val[%llu]\n", seq_val, step , max_val );
	}

     return 0;
}

int main()
{
	int nret;
	int 	i;
	long long seq_val;
	pthread_t ntid;
	CDbSqlcaPool 	*p_pool = NULL;
	//test_select();
	
	
	
/*
	nret = PullDbSequence( NULL );
	if( nret )
	{
		printf("LoadServerCache failed\n");
		return -1;
	}
*/
	
	p_pool = new CDbSqlcaPool( "mysqldb", "mysqldb", "192.168.56.120", "mysqldb", 3306 ); 
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
	
	p_threadpool = threadpool_create( 1, 10 );
	if( !p_threadpool )
	{
		printf("threadpool_create error ");
		return -1;
	}
	//threadpool_setReserver1( p_threadpool, p_env );
	//threadpool_setBeginCallback( p_env->p_threadpool, &ThreadBegin );
	//threadpool_setRunningCallback( p_env->p_threadpool, &ThreadRunning );
	//threadpool_setExitCallback( p_env->p_threadpool, &ThreadExit );	
	
	nret = threadpool_start( p_threadpool );
	if( nret )
	{
		printf("threadpool_start error ");
		return -1;
	}
	
	for( i = 0; i < 32; i++ )
	{
		pthread_create( &ntid, NULL, Thread_working, &i );
	}  
	
	printf("end\n");
	      
	getchar();
	
	return 0;
}
