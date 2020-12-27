/*
 * author	: jack
 * email	: xulongzhong2004@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */
 
#ifndef _DBUTIL_H_
#define _DBUTIL_H_

#include <vector>
#include <map>
#include <deque>
#include<sys/timeb.h>
#include <string>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>

using namespace std;

#ifdef __cplusplus
extern "C" {
#endif

#define SQLNOTFOUND		100					//查询数据后没数据，返回错误代码
#define DB_SUCCESS		0   /*!< 成功标志*/
#define DB_FAILURE		-1   /*!< 失败标志*/

#define DB_MAX_RECORD_ROW 	10001   /*每次最多取100行数据*/
#define DB_MAX_BUFFER_LEN 	8192  /*自定义缓冲区的长度*/
#define DB_MAX_VARCHAR_LEN 	4001  /*数据库中varchar2的最大长度为4000*/
#define DB_MAX_SQLSTMT_LEN 	8192  //执行的sql的语句最大长度


#ifdef WIN32 // win32 thread
#include <winsock2.h>
#include <Windows.h>
#include <process.h>
#define SP_THREAD_CALL __stdcall
#else
#include <pthread.h>
#include <unistd.h>
#define SP_THREAD_CALL
#endif

enum emSQLType
{
	SQLTYPE_DEFAULT = 0,
        SQLTYPE_SELECT = 1,
        SQLTYPE_UPDATE = 2,
        SQLTYPE_DELETE = 3,
        SQLTYPE_INSERT = 4,
        SQLTYPE_DROP = 6,
        SQLTYPE_ALTER = 7
};

enum emDataType
{
	DATA_TYPE_FLOAT,
	DATA_TYPE_DOUBLE,
	DATA_TYPE_INT,
        DATA_TYPE_UINT,
        DATA_TYPE_LONG,
        DATA_TYPE_ULONG,
        DATA_TYPE_LONGLONG,
        DATA_TYPE_ULONGLONG,
        DATA_TYPE_STRING
};

//错误代码 
enum emDBTYPE
{
	DBTYPE_UNKNOW,
	DBTYPE_STRING,		   	
	DBTYPE_INT,
	DBTYPE_DOUBLE,
	DBTYPE_DATE,
	DBTYPE_DATETIME
};

typedef struct _STColPropty
{
	char	sColName[50];
	int	nColLen;	
	int	nColType;   
	int	nColScale;	//精度
	int     nStrLen;
	int     nColOffset;
}STColPropty;

struct  STSqlPropty
{
	STSqlPropty()
	{
		nColNum = 0;
		nTotalColSize = 0;
		nTotalColNameSize = 0;
	}

	int			nColNum;	 //列数
	vector<STColPropty> 	vecColPropty; //各类的属性
	int			nTotalColSize;    //一行的总列数的长度
	int			nTotalColNameSize; //总的列名长度
};

int GetSQLType( const char *szSQL );

//锁接口类   
class ILock  
{  
public:  
	virtual ~ILock() {}  
	virtual void Lock() const = 0;  
	virtual void Unlock() const = 0;  

};  

//临界区锁类   
class CriSection : public ILock  
{  
public:  
	CriSection();  
	~CriSection();  

	virtual void Lock() const;  
	virtual void Unlock() const;  
	int	GetLockref() const;

private:  
	 pthread_mutex_t	m_critclSection;
};  

//锁   
class CAutoLock  
{  
public:  
	CAutoLock(const ILock&);  
	~CAutoLock();  
	void Unlock();
	void Lock();

private:  
	const ILock& m_lock;
	bool  m_block;  

}; 

#ifdef __cplusplus
}
#endif

#endif
