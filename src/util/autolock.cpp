#include "autolock.h"

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
CAutoLock::CAutoLock( const ILock& m, bool lock ) : m_lock(m)  
{ 
	if( lock )
	{
		m_lock.Lock(); 
		m_block = true; 
	}
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
