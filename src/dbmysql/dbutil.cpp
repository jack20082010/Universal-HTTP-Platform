/*
 * author	: jack
 * email	: xulongzhong2004@163.com
 *
 * Licensed under the LGPL v2.1, see the file LICENSE in base directory.
 */

#include "dbutil.h"

/***************************************************************************
函数名称: AllTrim
函数功能: 去字符串的空格函数
输出参数: sStr--原输入的字符串，返回处理后的字符串
		 
返 回 值: 返回处理后的字符串
***************************************************************************/
char* AllTrim(char *sStr)
{
	int i=0;
	int nLen=0;
	if (NULL==sStr || 0==sStr[0]) return sStr;
	for (i = 0; sStr[i] == ' '; i++);
	if (i >0) 
	{
		strcpy(sStr, sStr + i); //没找出内存重叠会发生问题
		//nLen=strlen(sStr);
		//memmove(sStr, sStr + i,nLen);
	}

	nLen=strlen(sStr);
	if ( 0==nLen)  return sStr;
	for (i = nLen - 1; sStr[i] == ' '; i--);
	sStr[i + 1] = '\0';

	return sStr;
}

int GetSQLType( const char *szSQL )
{
	char const* 	p = szSQL;
	int 		nIndex = 0;
	char 		sKey[10] = {0};

	while (*p != '\0') 
	{
		if ( 0 == nIndex && ( ' ' == *p|| '\t' == *p || '\n' == *p || '\\' == *p ) )
		{

		}
		else
		{
			if ( (*p >= 'a') && (*p <= 'z') )
			{
				sKey[nIndex] = toupper( *p );
			}
			else
			{
				sKey[nIndex] = *p;
			}
			nIndex++;
			if ( nIndex > 5 )
			{
				break;
			}
		}

		p++;
	}
	//返回的值与oci querytype一致
	int nRtn = SQLTYPE_DEFAULT;

	if( 0 == strcmp("SELECT",sKey) )
	{
		nRtn = 1;
	}
	else if( 0 == strcmp("UPDATE",sKey) )
	{
		nRtn = 2;
	}
	else if( 0 == strcmp("DELETE",sKey) )
	{
		nRtn = 3;
	}
	else if( 0 == strcmp("INSERT",sKey) )
	{
		nRtn = 4;
	}
	else if( 0 == strcmp("DROP",sKey) )
	{
		nRtn = 6;
	}
	else if( 0 == strcmp("ALTER",sKey) )
	{		
		nRtn = 7;
	}

	return nRtn;

}

//初始化临界资源对象   
CriSection::CriSection()  
{  
	pthread_mutex_init( &m_critclSection, NULL );
}  

//释放临界资源对象   
CriSection::~CriSection()  
{  
	pthread_mutex_destroy( &m_critclSection );
}  

//进入临界区，加锁   
void CriSection::Lock() const  
{
	pthread_mutex_lock( (pthread_mutex_t*)&m_critclSection );
}     

//离开临界区，解锁   
void CriSection::Unlock() const  
{  
	pthread_mutex_unlock( (pthread_mutex_t*)&m_critclSection );
} 


//利用C++特性，进行自动加锁   
CAutoLock::CAutoLock(const ILock& m) : m_lock(m)  
{ 
	m_lock.Lock(); 
	m_block = true; 
}  

//利用C++特性，进行自动解锁   
CAutoLock::~CAutoLock()  
{  
        if( m_block )
        {
                m_lock.Unlock();
                m_block=false;
        }
}  

void CAutoLock::Lock()
{
        if( !m_block )
        {
                m_lock.Lock();
                m_block = true;
        }
}

void CAutoLock::Unlock()
{
        if( m_block )
        {
                m_lock.Unlock();
                m_block = false;
        }
}
