#pragma once

#include "..\Common\DeviceResources.h"
#include "ShaderStructures.h"
#include "BodyStruct.h"
#include "BitSaving.h"
#include <ResearchModeApi.h>

namespace BasicHologram
{
	class SocketRenderer
	{
	public:
		SocketRenderer(std::shared_ptr<DX::DeviceResources> const& deviceResources, IResearchModeSensor *pSocketSensor, HANDLE hasData, ResearchModeSensorConsent *pCamAccessConsent)
		{
			m_pWs = new WS2;
			m_pWs->UdpCreateSend( DESTINATION_IP_ADDRESS, DESTINATION_PORT );
			m_pWs->UdpCreateReceive( SENDBACK_PORT );

			m_pImageRBT = new RBT_STRUCT[PACKET_DIVIDE];
			ZeroMemory( m_pImageRBT, sizeof(RBT_STRUCT) * PACKET_DIVIDE );
			m_pImageRBTSaved = new RBT_STRUCT_SAVED[PACKET_DIVIDE];
			ZeroMemory( m_pImageRBTSaved, sizeof(RBT_STRUCT_SAVED) * PACKET_DIVIDE );
			ZeroMemory( m_Skeleton, sizeof(k4abt_skeleton_t) * MAX_BODIES );
			ZeroMemory( m_fSkeleton2D, sizeof(k4a_float2_t) * MAX_BODIES * K4ABT_JOINT_COUNT );
			ZeroMemory( m_uBodyID, sizeof(uint32_t) * MAX_BODIES );
			m_uBodies = 0;

			m_plDepthImageUpdatedRendererIndex = nullptr;
            m_pSocketUpdateThread = new std::thread(SocketUpdateThread, this, hasData, pCamAccessConsent);
        }
        virtual ~SocketRenderer()
        {
            m_fExit = true;
            m_pSocketUpdateThread->join();
			if ( m_pImageRBTSaved )
			{
				delete [] m_pImageRBTSaved;
				m_pImageRBTSaved = nullptr;
			}
			if ( m_pImageRBT )
			{
				delete [] m_pImageRBT;
				m_pImageRBT = nullptr;
			}
			if ( m_pWs )
			{
				m_pWs->UdpDestroySend();
				m_pWs->UdpDestroyReceive();
				delete m_pWs;
				m_pWs = nullptr;
			}
		}

		RBT_STRUCT* GetRBT(volatile long* plDepthImageUpdatedRendererIndex);
		uint32_t GetBodies();
		void GetSkeleton(uint32_t uBody, k4abt_skeleton_t *pSkeleton);
		void GetSkeleton2D(uint32_t uBody, k4a_float2_t *pSkeleton2D);
		void GetBodyID(uint32_t uBody, uint32_t *pBodyID);

    private:
        static void SocketUpdateThread(SocketRenderer* p, HANDLE hasData, ResearchModeSensorConsent *pCamAccessConsent);
        void SocketUpdateLoop();

		WS2* m_pWs;
		RBT_STRUCT* m_pImageRBT;
		RBT_STRUCT_SAVED* m_pImageRBTSaved;
		BITSAVING m_bsave;

		k4abt_skeleton_t m_Skeleton[MAX_BODIES];
		k4a_float2_t m_fSkeleton2D[MAX_BODIES][K4ABT_JOINT_COUNT] = { 0.f, };
		uint32_t m_uBodyID[MAX_BODIES] = { K4ABT_INVALID_BODY_ID, };
		uint32_t m_uBodies = 0;

		volatile long* m_plDepthImageUpdatedRendererIndex;
		std::mutex m_sampleMutex;
        std::thread *m_pSocketUpdateThread;
        bool m_fExit = { false };
    };
}
