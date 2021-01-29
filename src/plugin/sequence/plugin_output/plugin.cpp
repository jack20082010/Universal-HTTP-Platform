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
#include "IDL_SeqRequest.dsc.h"

int ThreadExit( void *arg, int threadno );
int ThreadBegin( void *arg, int threadno );
int ThreadRunning( void *arg, int threadno );
int ThreadWorker( void *arg, int threadno );

Cplugin* Cplugin::_this = NULL;
Cplugin::Cplugin()
{
	m_pdbpool = (CDbSqlcaPool*)UHPGetDBPool();
	m_threadpool = NULL;
}

Cplugin::~Cplugin()
{
	
	if( m_threadpool )
		threadpool_destroy( m_threadpool );
}

int Cplugin::Doworker( AcceptedSession *p_session )
{
	struct HttpEnv*		p_http_env = NULL;
	char			*p_body = NULL;
	int			len = 0;
	char			*p_response = NULL;
	SeqRequest		seq_request;
	unsigned long long 	seq_val = 0;
	int			step = 0;
	int			client_cache = 0;
	int			client_alert_diff = 0;
	int			nret;
	
	/*
	struct HttpBuffer    *p_HttpBuffer = NULL; 
	p_HttpBuffer = GetHttpRequestBuffer( p_http_env );
	p_base = GetHttpBufferBase( p_HttpBuffer, NULL );
	p_response = GetSessionResponse( p_session );
	INFOLOGSG("base[%d] body[%s]", p_base, p_body );
	*/
	
	p_http_env = GetSessionHttpEnv( p_session );
	p_response = GetSessionResponse( p_session );
	p_body = GetHttpBodyPtr( p_http_env, &len );
	if( !p_body || len <= 0 )
	{
		ERRORLOGSG(" http body is null");
		OnException( p_session, -1, "http body is null" );
		return -1;
	}

	memset( &seq_request, 0, sizeof(seq_request) );
	nret = DSCDESERIALIZE_JSON_SeqRequest( "GB18030", p_body, &len, &seq_request );
	if( nret )
	{
		ERRORLOGSG("DSCDESERIALIZE_JSON_SeqRequest failed");
		OnException( p_session, -1, "DSCDESERIALIZE_JSON_SeqRequest failed");
		return -1;
	}
	
	if( seq_request.name[0] == 0 )
	{
		ERRORLOGSG("seq_name is null");
		OnException( p_session, -1, "seq_name is null" );
		return -1;
	}

	if( seq_request.count > 1 )
	{
		seq_val = GetBatSequence( seq_request.name, &seq_request.count, &step, &client_cache, &client_alert_diff );
		if( seq_val <= 0 )
		{
			ERRORLOGSG("GetBatSequence failed");
			OnException( p_session, -1, "GetBatSequence failed" );
			return -1;
		}
	}
	else
	{
		seq_val = GetSequence( seq_request.name, &step, &client_cache, &client_alert_diff );
		if( seq_val <= 0 )
		{
			ERRORLOGSG("GetSequence failed");
			OnException( p_session, -1, "GetSequence failed" );
			return -1;
		}
	}
	
	len = GetSessionResponseSize( p_session );
	snprintf( p_response, len - 1, "{ \"errorCode\":\"0\", \"value\":%llu, \"count\":%d, \"step\":%d, \"client_cache\":%d, \"client_alert_diff\":%d }", \
			seq_val, seq_request.count, step, client_cache, client_alert_diff );
	INFOLOGSG("seq_val[%llu], count[%d] setp[%d] client_cache[%d] client_alert_diff[%d]", seq_val, seq_request.count, step, client_cache, client_alert_diff );
	
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
	int	nret;
	
	m_threadpool = threadpool_create( UHPGetReserveInt1(), UHPGetReserveInt2() );
	if( !m_threadpool )
	{
		ERRORLOGSG("threadpool_create error ");
		return -1;
	}
	
	threadpool_setReserver1( m_threadpool, this );
	threadpool_setBeginCallback( m_threadpool, &ThreadBegin );
	threadpool_setRunningCallback( m_threadpool, &ThreadRunning );
	threadpool_setExitCallback( m_threadpool, &ThreadExit );
	
	nret = threadpool_start( m_threadpool );
        if( nret )
        {
                ERRORLOGSG("threadpool_start error ");
                return -1;
        }		
	
	return 0;
}

int Cplugin::ThreadWorkerImp( SeqAtrr *p_atr_seq, int threadno )
{
	int 		nret;
	SeqAtrr		seqAtrr;
	
	seqAtrr.name = p_atr_seq->name;
	nret = PullOneSeqAtrr( &seqAtrr );
	if( nret )
	{
		ERRORLOGSG("PullOneSeqAtrr failed nret[%d]\n", nret );
		CAutoLock lock( p_atr_seq->lock );
		p_atr_seq->is_pulling = 0; 	/*失败重新推送*/
	}
	else
	{
		CAutoLock lock( m_map_seq_next_lock );
		m_map_seq_next[p_atr_seq->name]= seqAtrr;
		INFOLOGSG("Tid[%ld] threadno[%d] cur_val[%llu] max_cur[%llu] 异步拉去数据成功\n",pthread_self() ,threadno,p_atr_seq->cur_val, p_atr_seq->max_val);
		
	}
	
	return 0;
	
}
	
int Cplugin::PullOneSeqAtrr( SeqAtrr *p_seq, int batCount )
{
	int 			nret;
	char			sql[200];
	char			update_sql[100];
	unsigned long long 	cur_val = 0;
	unsigned long long 	max_val = 0;
	unsigned long long 	min_val = 0;
	unsigned long long 	alert_val = 0;
	unsigned long long 	update_val = 0;
	int			alert_diff = 0; 
	int			client_alert_diff = 0;
	int			cache = 0;
	int			batch_cache = 0;
	int			batch_fetch = 0;
	int			client_cache = 0;
	int			step = 0;
	int 			cycle = 0;
	int			status = 0;
	CDbSqlca 		*pDbSqlca = NULL ;
	
	if( !p_seq )
	{
		ERRORLOGSG("参数非法\n");
		return -1;
	}

	pDbSqlca = m_pdbpool->GetIdleDbSqlca( );
	if( !pDbSqlca )
	{
		ERRORLOGSG("获取数据库连接失败：errCode[%d] errMsg[%s]\n", m_pdbpool->GetLastErrCode(), m_pdbpool->GetLastErrMsg() );
		return -1;
	}
        
	memset( sql, 0, sizeof(sql) );
	strncpy( sql, "select status, cycle, cur_val, max_val, min_val, step, cache, alert_diff, batch_cache, batch_fetch, client_cache, client_alert_diff, alert_val from bs_seq_rule where seq_name = ? for update ", sizeof(sql)-1 );
	pDbSqlca->BindIn( 1, p_seq->name.c_str() );
	pDbSqlca->BindOut( 1, "%d", &status );
	pDbSqlca->BindOut( 2, "%d", &cycle );
	pDbSqlca->BindOut( 3, "%llu", &cur_val );
	pDbSqlca->BindOut( 4, "%llu", &max_val );
	pDbSqlca->BindOut( 5, "%llu", &min_val );
	pDbSqlca->BindOut( 6, "%d", &step );
	pDbSqlca->BindOut( 7, "%d", &cache );
	pDbSqlca->BindOut( 8, "%d", &alert_diff );
	pDbSqlca->BindOut( 9, "%d", &batch_cache );
	pDbSqlca->BindOut( 10, "%d", &batch_fetch );
	pDbSqlca->BindOut( 11, "%d", &client_cache );
	pDbSqlca->BindOut( 12, "%d", &client_alert_diff );
	pDbSqlca->BindOut( 13, "%llu", &alert_val );       
	nret = pDbSqlca->ExecSql( sql );
	if( nret < 0 )
	{
		ERRORLOGSG("Sql[%s] failed errCode[%d] errMsg[%s]\n", sql,  pDbSqlca->GetDbErrorCode(),pDbSqlca->GetDbErrorMsg() );
		pDbSqlca->Rollback();
		m_pdbpool->ReleaseDbsqlca( pDbSqlca );
		return -1;
	}
	else if( nret == 0 )
	{
		ERRORLOGSG("找不到对应记录 sql[%s] seq_name[%s]\n", sql, p_seq->name.c_str() );
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
			ERRORLOGSG("超过最大值限制：name[%s] update_val[%llu] max_val[%llu] cur_val[%llu] step[%d] cache[%d]\n", p_seq->name.c_str(), update_val, max_val, cur_val, step, cache );
			pDbSqlca->Rollback();
			m_pdbpool->ReleaseDbsqlca( pDbSqlca );
			return -1;
		}
		else
		{
			ERRORLOGSG("超过最大值被设置为最小值循环使用：name[%s] update_val[%llu] max_val[%llu] cur_val[%llu] step[%d] cache[%d]\n", p_seq->name.c_str(), update_val, max_val, cur_val, step, cache );
			if( batCount )
				update_val = min_val + step*cache;
		}
	}
	else if( update_val >= alert_val && alert_val > 0 )
	{
		WARNLOGSG("超过告警值：name[%s] update_val[%llu] max_val[%llu] cur_val[%llu] alert_val[%llu] step[%d] cache[%d]\n", p_seq->name.c_str(), update_val, max_val, cur_val, alert_val, step, cache );
	}
	
	/*持久化到数据库*/
	memset( update_sql, 0, sizeof(update_sql) );
	strncpy( update_sql, "update bs_seq_rule set cur_val = ? where seq_name = ? ", sizeof(update_sql)-1 );
	pDbSqlca->BindIn( 1,  update_val );
	pDbSqlca->BindIn( 2,  p_seq->name.c_str() );
	nret = pDbSqlca->ExecSql( update_sql );
	if( nret < 0 )
	{
		ERRORLOGSG("sql[%s] seq_name[%s] failed errCode[%d] errMsg[%s]\n", update_sql,  p_seq->name.c_str(), pDbSqlca->GetDbErrorCode(),pDbSqlca->GetDbErrorMsg() );
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
	p_seq->batch_fetch = batch_fetch;
	p_seq->client_cache = client_cache; 
	p_seq->client_alert_diff = client_alert_diff;
	
	pDbSqlca->Commit();
	m_pdbpool->ReleaseDbsqlca( pDbSqlca );

	//printf("Tid[%ld] PullOneSeqAtrr seq_name[%s] cur_val[%llu] update_val[%llu], max_val[%llu] min_val[%llu] \n", pthread_self(), p_seq->name.c_str(), cur_val, update_val, max_val, min_val );
	
	return 0;
}

int Cplugin::RoundUp( int num, int factor ) 
{
    if( num < 0 || factor <= 0 )
    	return -1;
    else if( num == 0 )
    	return factor;
    
    return ( ( num + factor - 1 ) / factor ) * factor;

}

unsigned long long  Cplugin::PullBatSeqAtrr( char *name, int count, BatSeqAtrr *p_batseq, int *p_step , int *p_client_cache, int *p_client_alert_diff )
{
	unsigned long long 		cur_val = 0;
	SeqAtrr				seq_atrr;
	int				nret;
	StrSeqSortMap::iterator 	it;
	int				batch_fetch = 3;	/*本次提取数量 2个保留到内存中*/
	
	if( p_batseq )
	{
		if( p_batseq->batch_map.size() > (size_t)p_batseq->batch_cache )
			batch_fetch = 1;
		else
			batch_fetch = p_batseq->batch_fetch;
		
	}
	INFOLOGSG( "batch_fetch[%d]\n", batch_fetch );
	
	//序列被禁用情况，数据库加载时不判断，由加载到map内存判断，提高性能
	seq_atrr.name = name;
	nret = PullOneSeqAtrr( &seq_atrr, batch_fetch*count );
	if( nret )
	{
		ERRORLOGSG("PullOneSeqAtrr 失败 nert[%d]\n", nret );
		return 0;
	}

	if( p_step )
		*p_step = seq_atrr.step;
	if( p_client_cache )
		*p_client_cache = seq_atrr.client_cache;
	if( p_client_alert_diff )
		*p_client_alert_diff = seq_atrr.client_alert_diff;
	
	cur_val = seq_atrr.cur_val;
	seq_atrr.cur_val += count*seq_atrr.step;
	
	//printf("have no cache data 数据库获取 cur_val[%llu], cur_val2[%llu] max_val[%llu]\n", cur_val, seq_atrr.cur_val, seq_atrr.max_val );
	
	if( p_batseq == NULL ) //map not find m_map_batseq_lock
	{
		BatSeqAtrr 	batseq_atrr;
		
		batseq_atrr.name = seq_atrr.name ;
		batseq_atrr.status = seq_atrr.status;
		batseq_atrr.batch_cache = seq_atrr.batch_cache;
		batseq_atrr.batch_fetch = seq_atrr.batch_fetch;
		batseq_atrr.client_cache = seq_atrr.client_cache;
		batseq_atrr.client_alert_diff = seq_atrr.client_alert_diff;
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
			p_batseq->batch_fetch = seq_atrr.batch_fetch;
			p_batseq->client_cache = seq_atrr.client_cache;
			p_batseq->client_alert_diff = seq_atrr.client_alert_diff;

			if( p_batseq->batch_map.size() < (size_t)seq_atrr.batch_cache )
			{
				//INFOLOGSG("batch_map_size[%d], batch_cache[%d]\n", p_batseq->batch_map.size(), seq_atrr.batch_cache );
				p_batseq->batch_map[count] = seq_atrr;
			}
		}
		
	}
	
	return cur_val;
}

unsigned long long Cplugin::GetBatSequence( char *name, int *p_count, int *p_step, int *p_client_cache, int *p_client_alert_diff )
{
	SeqAtrr				*p_seq = NULL;
	BatSeqAtrr			*p_batseq = NULL;
	unsigned long long 		cur_val = 0;
	StrSeqSortMap::iterator 	it;
	IntSeqAtrrMap::iterator 	it2;
	
	const int 			factor = 10;		/*向上取整因子*/
	int				count = 0;
	
	if( !name || !p_count || *p_count <= 0 )
	{
		ERRORLOGSG("name 参数错误");
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
			ERRORLOGSG("序列禁用状态\n");
			return 0;	
		}
		lock0.Unlock();

		CAutoLock lock( p_batseq->lock );
		it2 = p_batseq->batch_map.find( count);
		if( it2 != p_batseq->batch_map.end() )
		{
			p_seq = &( it2->second );
			if( ( p_seq->max_val - p_seq->cur_val ) >= (size_t)count*p_seq->step )
			{
				if( p_step )
					*p_step = p_seq->step;
				if( p_client_cache )
					*p_client_cache = p_seq->client_cache;
				if( p_client_alert_diff )
					*p_client_alert_diff = p_seq->client_alert_diff;
				
				cur_val = p_seq->cur_val;
				p_seq->cur_val += count*p_seq->step;
				
				INFOLOGSG("have cache data 缓存获取 cur_val[%llu] cur_val2[%llu]\n", cur_val, p_seq->cur_val);
			}
			else
			{
				cur_val = PullBatSeqAtrr( name, count, p_batseq, p_step, p_client_cache, p_client_alert_diff );
			}
		}
		else
		{
			cur_val = PullBatSeqAtrr( name, count, p_batseq, p_step, p_client_cache, p_client_alert_diff );
		}
	}
	else
	{
		cur_val = PullBatSeqAtrr( name, count, NULL, p_step, p_client_cache, p_client_alert_diff );
	}
		
	return cur_val;

}

unsigned long long Cplugin::GetSequence( char *name , int *p_step, int *p_client_cache, int *p_client_alert_diff )
{
	SeqAtrr				*p_seq = NULL;
	int				nret;
	unsigned long long 		cur_val = 0;
	StrSeqAtrrMap::iterator		it;
	StrSeqAtrrMap::iterator		it2;
	
	if( !name )
	{
		ERRORLOGSG("name 参数不能为空");
		return 0;	
	}
	
	CAutoLock lock0( m_map_seq_lock );
	it = m_map_seq.find( name );
	if( it != m_map_seq.end() )
	{
		p_seq = &( it->second );		
		if( p_seq->status != 0 )
		{
			ERRORLOGSG("序列禁用状态\n");
			return 0;	
		}
		if( p_step )
			*p_step = p_seq->step;
		if( p_client_cache )
			*p_client_cache = p_seq->client_cache;
		if( p_client_alert_diff )
			*p_client_alert_diff = p_seq->client_alert_diff;

		lock0.Unlock();   
		
		CAutoLock lock( p_seq->lock );
		if( p_seq->max_val == p_seq->cur_val  ) //异步获取时间差还没完成，直接同步调用
		{
			//printf("==== cur_val[%llu] max_val[%llu] is_pulling[%d]\n", p_seq->cur_val, p_seq->max_val, p_seq->is_pulling);
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
				p_seq->client_cache = p_seq_next->client_cache;
				p_seq->client_alert_diff = p_seq_next->client_alert_diff;
				m_map_seq_next.erase( it2 );
				p_seq->is_pulling = 0;
				INFOLOGSG("cache have data 获取到缓存数据 cur_val[%llu]  max_val[%llu]\n", p_seq->cur_val, p_seq->max_val );
				
			}
			else
			{
				lock2.Unlock();
				INFOLOGSG("have no cache data is_pulling[%d] 直接从数据库获取数据\n", p_seq->is_pulling);
				
				nret = PullOneSeqAtrr( p_seq );
				if( nret )
				{
					ERRORLOGSG("PullOneSeqAtrr 失败 nert[%d]\n", nret );
					return 0;
				}
			}
			

		}
		else if( p_seq->max_val - p_seq->cur_val <= (size_t)p_seq->alert_diff  && p_seq->is_pulling == 0 )
		{
			taskinfo_t	task ;
			
			p_seq->is_pulling = 1; //设置正在拉去标志
			p_seq->arg = this;

			memset( &task, 0, sizeof(taskinfo_t) );
			task.fn_callback = ThreadWorker;
			task.arg = p_seq;
			nret = threadpool_addTask( m_threadpool, &task );
			if( nret < 0 )
			{
				ERRORLOGSG( "threadpool_addTask failed nret[%d]\n", nret);
				return -1;
			}
			INFOLOGSG( "threadpool_addTask ok cur_val[%llu] max_val[%llu] nret[%d]\n", p_seq->cur_val, p_seq->max_val, nret);
			
		}
		
		cur_val = p_seq->cur_val;
		p_seq->cur_val += p_seq->step;
		
	}
	else
	{      
		SeqAtrr		seq_atrr;
		
		seq_atrr.name = name ;
		nret = PullOneSeqAtrr( &seq_atrr );
		if( nret )
		{
			ERRORLOGSG("PullOneSeqAtrr 失败 nert[%d]\n", nret );
			return 0;
		}
		
		if( p_step )
			*p_step = seq_atrr.step;
		if( p_client_cache )
			*p_client_cache = seq_atrr.client_cache;
		if( p_client_alert_diff )
			*p_client_alert_diff = seq_atrr.client_alert_diff;
		
		cur_val = seq_atrr.cur_val;
		seq_atrr.cur_val += seq_atrr.step;
		seq_atrr.is_pulling = 0; //初始化
		m_map_seq[name] = seq_atrr;
		INFOLOGSG("load seq_name  cur_val[%llu] max_val[%llu]\n", cur_val, seq_atrr.max_val);
	}
	
	//printf("cur_val[%llu] max_val[%llu] alert_diff[%d]\n",  p_seq->cur_val,  p_seq->max_val,  p_seq->alert_diff );
	return cur_val;

}

int Load()
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

int Unload()
{
	Cplugin::Uninstance();   
	INFOLOGSG( "plugin unload clean" );
	
	return 0;
}

int Doworker( AcceptedSession *p_session )
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

int OnException( AcceptedSession *p_session, int errcode, char *errmsg )
{
	char	*p_response = NULL;
	int	len;
	
	len = GetSessionResponseSize( p_session );
	p_response = GetSessionResponse( p_session );
	snprintf( p_response, len - 1, "{ \"errorCode\": \"%d\", \"errorMessage\": \"%s\" }", errcode, errmsg );
	
	return 0;
}

int ThreadBegin( void *arg, int threadno )
{
	char			module_name[50];
	
	memset( module_name, 0, sizeof(module_name) );
	snprintf( module_name, 15, "Plugin_%d", threadno ); 
	prctl( PR_SET_NAME, module_name );
	
	/* 设置日志环境 */
	UHPInitLogEnv();
	INFOLOGSG(" InitPluginLogEnv module_name[%s] threadno[%d] ok", module_name, threadno );
	
	return 0;
}

int ThreadRunning( void *arg, int threadno )
{
	threadinfo_t		*p_threadinfo = (threadinfo_t*)arg;

	if( p_threadinfo->cmd == 'L' ) /*线程内应用初始化*/
	{
		char	module_name[50];

		memset( module_name, 0, sizeof(module_name) );
		snprintf( module_name, 15, "PluginThread_%d", threadno ); 
		prctl( PR_SET_NAME, module_name );
		
		/* 设置日志环境 */
		UHPInitLogEnv();
		INFOLOGSG(" Plugin reload config module_name[%s] threadno[%d] ok", module_name, threadno );
		
		p_threadinfo->cmd = 0;
	}
	
	/*空闲时候输出日志*/
	if( p_threadinfo->status == THREADPOOL_THREAD_WAITED )
	{
		DEBUGLOGSG(" Plugin Thread Is Idling  threadno[%d] ", threadno );
	}

	return 0;
}

int ThreadExit( void *arg, int threadno )
{
	threadinfo_t	*p_threadinfo = (threadinfo_t*)arg;
	
	UHPCleanLogEnv();
	p_threadinfo->cmd = 0;
	INFOLOGSG("Plugin Thread Exit threadno[%d] ", threadno );

	return 0;
}

int ThreadWorker( void *arg, int threadno )
{
	SeqAtrr	 	*p_atr_seq = (SeqAtrr*)arg ;
	Cplugin	 	*p_Cplugin = (Cplugin*)p_atr_seq->arg ;
	int 		nret;

	nret = p_Cplugin->ThreadWorkerImp( p_atr_seq, threadno );
	if( nret )
	{
		ERRORLOGSG("ThreadWorkerImp failed nret[%d]\n", nret );
	}
	
	return 0;
}


