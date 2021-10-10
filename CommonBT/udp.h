// Original Author: Shinji Chiba (MSKK)
#pragma once

#include <winsock2.h>

class WS2
{
private:
	SOCKET m_UdpSendSocket;
	SOCKADDR_IN m_UdpSendSocketAddrIn = { 0, };
	SOCKET m_UdpReceiveSocket;
	SOCKADDR_IN m_UdpReceiveSocketAddrIn = { 0, };

public:
	WS2();
	~WS2();
	HRESULT UdpCreateSend( const char* szDestination, unsigned short usPort );
	void UdpDestroySend();
	void UdpSendData( char* pData, ULONG uSize );
	HRESULT UdpCreateReceive( unsigned short usPort );
	void UdpDestroyReceive();
	int UdpReceiveData( char* pData, ULONG uSize );
};
