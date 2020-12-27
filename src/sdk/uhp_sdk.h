
#ifndef _H_UPH_
#define _H_UPH_


#ifdef __cplusplus
extern "C" {
#endif

int Load();
int Unload();
unsigned long long GetSequence( char* name ) ;
int SetConnectTimeout( int timeout );
int SetServerUrl( char* server_url );

#ifdef __cplusplus
}
#endif

#endif
