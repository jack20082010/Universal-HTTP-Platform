/*
 * 
 * author	: jack
 * email	: xulongzhong2004@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#ifndef _H_PLUGIN_
#define _H_PLUGIN_

#include "dbsqlca.h"
#include "LOGS.h"
#include "uhp_api.h"
#include "fasterhttp.h"
#include "fasterjson.h"
#include "threadpool.h"

class SeqAtrr
{
public:
	string 			name;
	CriSection   		lock;	
	unsigned long long	cur_val;
	unsigned long long	max_val;
	int			step;
	int			alert_diff;		//服务端当前值到达告警值时触发异步db获取
	int		  	status;         	
	int			is_pulling;     	
	int			batch_cache;    	//内存cache个数,批量获取使用
	int			batch_fetch;    	//每次提取个数方便cache,批量获取使用
	int			client_cache;		//sdk内存缓存个数,调用批量获取附带配置参数
	int			client_alert_diff;	//sdk当前值到达告警值时触发异步db获取
	void			*arg;

	SeqAtrr()
	{
		cur_val = 0;
		max_val = 0;
		step = 0;
		alert_diff = 0;
		status = 0;
		is_pulling = 0;
		batch_cache = 0;
		batch_fetch = 0;
		client_cache = 0;
		client_alert_diff = 0;
		arg = NULL;
	}
};

typedef map< string, SeqAtrr >  StrSeqAtrrMap;
typedef map< int, SeqAtrr, greater<int> >  IntSeqAtrrMap;

class BatSeqAtrr
{
public:
	string 			name;
	CriSection   		lock;
	int		  	status;
	int			batch_cache;
	int			batch_fetch;
	int			client_cache;
	int			client_alert_diff;
	IntSeqAtrrMap		batch_map;

	BatSeqAtrr()
	{
		status = 0;
		batch_cache = 0;
		batch_fetch = 0;
		client_cache = 0;
		client_alert_diff = 0;
	}
};

typedef map< string, BatSeqAtrr > StrSeqSortMap;


class Cplugin
{
public:
        static Cplugin* Instance();
        static int      Uninstance();
        
 	unsigned long long GetSequence( char *name, int *p_step, int *p_client_cache, int *p_client_alert_diff );
	unsigned long long GetBatSequence( char *name, int *p_count, int *p_step, int *p_client_cache, int *p_client_alert_diff );
	
        int		Load();
        int 		Doworker( struct AcceptedSession *p_session );
        int 		ThreadWorkerImp( SeqAtrr *p_atr_seq, int threadno );
        
     
private:
	static  Cplugin*      	_this;
        Cplugin();
        ~Cplugin();
        int ConnectDB();
       
        unsigned long long  PullBatSeqAtrr( char *name, int count, BatSeqAtrr *p_batseq, int *p_step, int *p_client_cache,int *p_clent_alert_diff );
        int RoundUp( int num, int factor );
        int PullOneSeqAtrr( SeqAtrr *p_seq, int batCount = -1 );

        CDbSqlcaPool 		*m_pdbpool;
        threadpool_t		*m_threadpool;
        
        CriSection 	m_map_seq_lock;  
	StrSeqAtrrMap 	m_map_seq;
	CriSection	m_map_seq_next_lock;  
	StrSeqAtrrMap	m_map_seq_next;
	
	
	CriSection 	m_map_batseq_lock;  
	StrSeqSortMap 	m_map_batseq;
	
        
};


#ifdef __cplusplus
extern "C" {
#endif

int ThreadExit( void *arg, int threadno );
int ThreadBegin( void *arg, int threadno );
int ThreadRunning( void *arg, int threadno );

int Load();
int Unload();
int Doworker( AcceptedSession *p_session );
int OnException( AcceptedSession *p_session, int errcode, char *errmsg );

#ifdef __cplusplus
}
#endif

#endif
