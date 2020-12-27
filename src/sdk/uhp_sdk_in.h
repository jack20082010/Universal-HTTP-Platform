
#ifndef _H_UHP_IN_
#define _H_UHP_IN_

#include <map>
#include <deque>
#include <vector>
#include <string>
#include <stdarg.h>
#include <sys/prctl.h>

using namespace std;

#include "threadpool.h"
#include "autolock.h"
#include "util.h"
#include "LOG.h"
#include "LOGS.h"
#include "fasterhttp.h"
#include "IDL_uhp_response.dsc.h"

#define URI_PRODUCER				"/sequence"
#define HTTP_HEADER_CONTENT_TYPE_TEXT           "application/text"
#define	HTTP_HEADER_CONTENT_TYPE_JSON		"application/json"

using namespace std;

class HostInfo
{

public:
	string	ip;
	int	port;
	long	activeTimestamp;
	HostInfo()
	{
		port = 0;
		activeTimestamp = 0;
	}
};

class BatSeqAtrr
{

public:
	string 			name;
	CriSection   		lock;
	int			total_count;
	int			client_cache;
	int			client_alert_diff;
	deque<UhpResponse>	deque_seq;
	int			is_pulling;
	void			*arg;
	
	BatSeqAtrr()
	{
		total_count = 0;
		client_cache = 0;
		client_alert_diff = 0;
		is_pulling = 0;
		arg = NULL;
	}
};

typedef map< string, BatSeqAtrr > StrSeqSortMap;

class Cplugin
{
public:
        static Cplugin* Instance();
        static int      Uninstance();
        unsigned long long	GetSequence( char *name );
        int ThreadWorkerImp( BatSeqAtrr *p_batch, int threadno );
        int InitLogEnv();
        int CleanLogEnv();
        int SetConnectTimeout( int timeout );
        int SetServerUrl( string server_url );
        
private:
	static  Cplugin*      	_this;
        Cplugin();
        ~Cplugin();

        CriSection 		m_map_batseq_lock;  
	StrSeqSortMap 		m_map_batseq;
	int			m_pull_count;
	
	int			m_connect_timeout;
	int			m_timeout;
	int			m_fusing_time;
	int			m_full_fusing_time;
	
	vector<HostInfo>	m_vector_host;
        threadpool_t		*m_threadpool;
        string			m_server_url;
        
	int ConnectNonBlock( struct NetAddress *p_netaddr, int connect_timeout );
	int RequestAndResponseV( struct NetAddress *p_netaddr , struct HttpEnv *http , int timeout , char *format , va_list valist );
	int HttpRequestFormat( struct HttpEnv *p_http_env,char *format , ... );
	int Disconnect( struct NetAddress *p_netaddr );
        int HttpRequest( char* seq_name, int count, UhpResponse *p_response );
   
        int GetOneSeqFromBatch( BatSeqAtrr *p_batch );
        struct HostInfo* SelectValidHost( );
        unsigned long long GetLocalSequece( BatSeqAtrr *p_batch );
        int PullRemoteSequence( BatSeqAtrr *p_batch );
        int PushHostInfo( string host_url );
        
        int InitLogEnvV( char *service_name , char *module_name , char *event_output , int process_loglevel , char *process_output_format , va_list valist );
        int InitLogEnv_Format( char *service_name , char *module_name , char *event_output , int process_loglevel , char *process_output_format , ... );

};

#endif
