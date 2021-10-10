// Original Author: Shinji Chiba (MSKK)
#include "udp.h"

WS2::WS2() :
	m_UdpSendSocket( INVALID_SOCKET ),
	m_UdpReceiveSocket( INVALID_SOCKET )
{
	WSADATA wsadata;
	WSAStartup( MAKEWORD( 2, 2 ), &wsadata );
}

WS2::~WS2()
{
	WSACleanup();
}

HRESULT WS2::UdpCreateSend( const char* szDestination, unsigned short usPort )
{
	LPHOSTENT pHostent = gethostbyname( szDestination );
	if ( pHostent )
	{
		m_UdpSendSocketAddrIn.sin_family = AF_INET;
		m_UdpSendSocketAddrIn.sin_port = htons( usPort );
		CopyMemory( &(m_UdpSendSocketAddrIn.sin_addr), pHostent->h_addr_list[0], pHostent->h_length );
		m_UdpSendSocket = WSASocket( AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, 0 );
		if ( m_UdpSendSocket != INVALID_SOCKET )
			return S_OK;
	}
	return E_FAIL;
}

void WS2::UdpDestroySend()
{
	if ( m_UdpSendSocket != INVALID_SOCKET )
	{
		closesocket( m_UdpSendSocket );
		m_UdpSendSocket = INVALID_SOCKET;
	}
}

void WS2::UdpSendData( char* pData, ULONG uSize )
{
	WSABUF wsabuf = { uSize, pData };
	DWORD dwBytes;
	WSASendTo( m_UdpSendSocket, &wsabuf, 1, &dwBytes, 0, (const SOCKADDR*) &m_UdpSendSocketAddrIn, sizeof(SOCKADDR_IN), NULL, NULL );
}

HRESULT WS2::UdpCreateReceive( unsigned short usPort )
{
	m_UdpReceiveSocketAddrIn.sin_family = AF_INET;
	m_UdpReceiveSocketAddrIn.sin_port = htons( usPort );
	m_UdpReceiveSocketAddrIn.sin_addr.S_un.S_addr = INADDR_ANY;
	m_UdpReceiveSocket = WSASocket( AF_INET, SOCK_DGRAM, IPPROTO_UDP, NULL, 0, 0 );
	if ( m_UdpReceiveSocket != INVALID_SOCKET )
	{
		if ( bind( m_UdpReceiveSocket, (const SOCKADDR*) &m_UdpReceiveSocketAddrIn, sizeof(SOCKADDR_IN) ) != SOCKET_ERROR )
		{
			u_long val = 1;
			int iSize = 262144;
			ioctlsocket( m_UdpReceiveSocket, FIONBIO, &val );
			setsockopt( m_UdpReceiveSocket, SOL_SOCKET, SO_RCVBUF, (char*) &iSize, (int) sizeof(int) );
			return S_OK;
		}
		UdpDestroyReceive();
	}
	return E_FAIL;
}

void WS2::UdpDestroyReceive()
{
	if ( m_UdpReceiveSocket != INVALID_SOCKET )
	{
		closesocket( m_UdpReceiveSocket );
		m_UdpReceiveSocket = INVALID_SOCKET;
	}
}

int WS2::UdpReceiveData( char* pData, ULONG uSize )
{
	int iLen = 0;
	if ( m_UdpReceiveSocket != INVALID_SOCKET )
	{
		WSABUF wsabuf = { uSize, pData };
		DWORD dwBytes, dwFlags = 0;
		iLen = WSARecv( m_UdpReceiveSocket, &wsabuf, 1, &dwBytes, &dwFlags, NULL, NULL );
		if ( iLen != SOCKET_ERROR )
			iLen = (int) dwBytes;
	}
	return iLen;
}
