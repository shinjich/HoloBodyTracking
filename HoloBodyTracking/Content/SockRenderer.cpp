#include "pch.h"
#include "SockRenderer.h"

using namespace BasicHologram;
using namespace DirectX;

void SocketRenderer::SocketUpdateLoop()
{
	while (!m_fExit)
	{
		if ( InterlockedExchange( m_plDepthImageUpdatedRendererIndex, -1 ) >= 0 )
		{
			for( int i = 0; i < PACKET_DIVIDE; i++ )
			{
				m_pImageRBTSaved[i].dwHeader = m_pImageRBT[i].dwHeader;
				m_bsave.Save( m_pImageRBTSaved[i].cDepth, m_pImageRBT[i].cDepth, RESOLUTION_SIZE / PACKET_DIVIDE, SCOPE_DISTANCE_BITS_D );
				m_bsave.Save( m_pImageRBTSaved[i].cIr, m_pImageRBT[i].cIr, RESOLUTION_SIZE / PACKET_DIVIDE, SCOPE_DISTANCE_BITS_I );
				m_pWs->UdpSendData( (char*) &m_pImageRBTSaved[i], sizeof(RBT_STRUCT_SAVED) );
			}
		}
		char sbtbuf[1400];
		UINT32 uBodies = 0;
		for( ; ; )
		{
			int iReceived = m_pWs->UdpReceiveData( sbtbuf, 1400 );
			if ( iReceived > 0 )
			{
				uBodies = ((SBT_STRUCT*) sbtbuf)->dwHeader;
				CopyMemory( &m_Skeleton[0], &((SBT_STRUCT*) sbtbuf)->skeleton[0], uBodies * sizeof(k4abt_skeleton_t) );
				CopyMemory( &m_fSkeleton2D[0][0], &((SBT_STRUCT*) sbtbuf)->skeleton2D[0][0], uBodies * K4ABT_JOINT_COUNT * sizeof(k4a_float2_t) );
				CopyMemory( &m_uBodyID[0], &((SBT_STRUCT*) sbtbuf)->bodyID[0], uBodies * sizeof(uint32_t) );
			}
			else if ( iReceived == 0 )
			{
				uBodies = 0;
				break;
			}
			else if ( iReceived < 0 )
			{
				m_uBodies = uBodies;
				break;
			}
		}
    }
}

RBT_STRUCT* SocketRenderer::GetRBT(volatile long* plDepthImageUpdatedRendererIndex)
{
	m_plDepthImageUpdatedRendererIndex = plDepthImageUpdatedRendererIndex;
	return m_pImageRBT;
}

uint32_t SocketRenderer::GetBodies()
{
	return m_uBodies;
}

void SocketRenderer::GetSkeleton(uint32_t uBody, k4abt_skeleton_t *pSkeleton)
{
	std::lock_guard<std::mutex> guard(m_sampleMutex);

	CopyMemory( pSkeleton, &m_Skeleton[uBody], sizeof(k4abt_skeleton_t) );
}

void SocketRenderer::GetSkeleton2D(uint32_t uBody, k4a_float2_t *pSkeleton2D)
{
	std::lock_guard<std::mutex> guard(m_sampleMutex);

	CopyMemory( pSkeleton2D, m_fSkeleton2D[uBody], sizeof(k4a_float2_t) * K4ABT_JOINT_COUNT );
}

void SocketRenderer::GetBodyID(uint32_t uBody, uint32_t *pBodyID)
{
	std::lock_guard<std::mutex> guard(m_sampleMutex);

	*pBodyID = m_uBodyID[uBody];
}

void SocketRenderer::SocketUpdateThread(SocketRenderer* pSocketRenderer, HANDLE hasData, ResearchModeSensorConsent *pCamAccessConsent)
{
	HRESULT hr = S_OK;

	if (hasData != nullptr)
	{
		DWORD waitResult = WaitForSingleObject(hasData, INFINITE);

		if (waitResult == WAIT_OBJECT_0)
		{
			switch (*pCamAccessConsent)
			{
			case ResearchModeSensorConsent::Allowed:
				OutputDebugString(L"Access is granted");
				break;
			case ResearchModeSensorConsent::DeniedBySystem:
				OutputDebugString(L"Access is denied by the system");
				hr = E_ACCESSDENIED;
				break;
			case ResearchModeSensorConsent::DeniedByUser:
				OutputDebugString(L"Access is denied by the user");
				hr = E_ACCESSDENIED;
				break;
			case ResearchModeSensorConsent::NotDeclaredByApp:
				OutputDebugString(L"Capability is not declared in the app manifest");
				hr = E_ACCESSDENIED;
				break;
			case ResearchModeSensorConsent::UserPromptRequired:
				OutputDebugString(L"Capability user prompt required");
				hr = E_ACCESSDENIED;
				break;
			default:
				OutputDebugString(L"Access is denied by the system");
				hr = E_ACCESSDENIED;
				break;
			}
		}
		else
		{
			hr = E_UNEXPECTED;
		}
	}

	if (FAILED(hr))
	{
		return;
	}

	pSocketRenderer->SocketUpdateLoop();
}
