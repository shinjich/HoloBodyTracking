//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#include "pch.h"
#include "SlateCameraRenderer.h"

using namespace BasicHologram;
using namespace DirectX;

void SlateCameraRenderer::CameraUpdateThread(SlateCameraRenderer* pSlateCameraRenderer, HANDLE hasData, ResearchModeSensorConsent *pCamAccessConsent)
{
	HRESULT hr = S_OK;
	LARGE_INTEGER qpf;
	uint64_t lastQpcNow = 0;

	QueryPerformanceFrequency(&qpf);

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

	winrt::check_hresult(pSlateCameraRenderer->m_pRMCameraSensor->OpenStream());

	while (!pSlateCameraRenderer->m_fExit && pSlateCameraRenderer->m_pRMCameraSensor)
	{
		static int gFrameCount = 0;
		HRESULT hr = S_OK;
		IResearchModeSensorFrame* pSensorFrame = nullptr;
		LARGE_INTEGER qpcNow;
		uint64_t uqpcNow;
		QueryPerformanceCounter(&qpcNow);
		uqpcNow = qpcNow.QuadPart;
		ResearchModeSensorTimestamp timeStamp;

		winrt::check_hresult(pSlateCameraRenderer->m_pRMCameraSensor->GetNextBuffer(&pSensorFrame));

		pSensorFrame->GetTimeStamp(&timeStamp);

		{
			if (lastQpcNow != 0)
			{
				pSlateCameraRenderer->m_refreshTimeInMilliseconds =
					(1000 *
					(uqpcNow - lastQpcNow)) /
					qpf.QuadPart;
			}

			if (pSlateCameraRenderer->m_lastHostTicks != 0)
			{
				pSlateCameraRenderer->m_sensorRefreshTime = timeStamp.HostTicks - pSlateCameraRenderer->m_lastHostTicks;
			}

			std::lock_guard<std::mutex> guard(pSlateCameraRenderer->m_mutex);

			if (pSlateCameraRenderer->m_frameCallback)
			{
				pSlateCameraRenderer->m_frameCallback(pSensorFrame, pSlateCameraRenderer->m_frameCtx);
			}

			if (pSlateCameraRenderer->m_pSensorFrame)
			{
				pSlateCameraRenderer->m_pSensorFrame->Release();
			}

			pSlateCameraRenderer->m_pSensorFrame = pSensorFrame;
			lastQpcNow = uqpcNow;
			pSlateCameraRenderer->m_lastHostTicks = timeStamp.HostTicks;
		}
	}

	if (pSlateCameraRenderer->m_pRMCameraSensor)
	{
		pSlateCameraRenderer->m_pRMCameraSensor->CloseStream();
	}
}

bool SlateCameraRenderer::UpdateSlateTexture(RBT_STRUCT* pImageRBT)
{
	bool bDepthImageUpdated = false;
	if (m_pSensorFrame)
	{
		EnsureTextureForCameraFrame(m_pSensorFrame);

		bDepthImageUpdated = UpdateTextureFromCameraFrame(m_pSensorFrame, m_texture2D, pImageRBT);
	}
	return bDepthImageUpdated;
}

void SlateCameraRenderer::EnsureTextureForCameraFrame(IResearchModeSensorFrame* pSensorFrame)
{
	ResearchModeSensorResolution resolution;
	BYTE *pImage = nullptr;

	if (m_texture2D == nullptr)
	{
		pSensorFrame->GetResolution(&resolution);

		const UINT32 textureWidth = resolution.Width * sizeof(UINT16);

		m_texture2D = std::make_shared<BasicHologram::Texture2D>(
			m_deviceResources,
			textureWidth,
			resolution.Height);
	}
}

bool SlateCameraRenderer::UpdateTextureFromCameraFrame(IResearchModeSensorFrame* pSensorFrame, std::shared_ptr<Texture2D> texture2D, RBT_STRUCT* pImageRBT)
{
	HRESULT hr = S_OK;

	bool bDepthImageUpdated = false;
	ResearchModeSensorResolution resolution;
	IResearchModeSensorDepthFrame *pDepthFrame = nullptr;
	size_t outBufferCount = 0;
	const BYTE *pImage = nullptr;

	pSensorFrame->GetResolution(&resolution);
	hr = pSensorFrame->QueryInterface(IID_PPV_ARGS(&pDepthFrame));
	if (pDepthFrame)
	{
#if RBT_LONGTHROW_DEPTH
		const BYTE *pSigma = nullptr;
		hr = pDepthFrame->GetSigmaBuffer(&pSigma, &outBufferCount);
		size_t i;
		DWORD dwImageSum = 0;
		for( i = 0; i < outBufferCount / sizeof(DWORD); i++ )
			dwImageSum += ((LPDWORD) pSigma)[i];
		if ( dwImageSum != m_dwImageSum )
		{
			// シグマバッファに更新が見られたときにのみ深度バッファ関係を処理する
			bDepthImageUpdated = true;
			m_dwImageSum = dwImageSum;
		}
		if ( bDepthImageUpdated )
#endif
		{
			const UINT16 *pDepth = nullptr;
			pDepthFrame->GetBuffer(&pDepth, &outBufferCount);
			void* mappedTexture = texture2D->MapCPUTexture<void>(D3D11_MAP_WRITE);

#if ! RBT_LONGTHROW_DEPTH
			DWORD dwImageSum = 0;
#endif
			const UINT32 uRpd4 = texture2D->GetRowPitch() / sizeof(UINT32);
			const UINT32 iW = resolution.Width;
			const UINT32 iH = resolution.Height;
			for (UINT i = 0; i < iH; i++)
			{
				UINT32 uRpd4i = uRpd4 * i + iW;
				const UINT32 iXY = iW * i;
				for (UINT j = 0; j < iW; j++)
				{
					const UINT32 iOffset = iXY + j;
					UINT16 wDepthTemp = 0;
					// 深度のクランプ処理
#if RBT_LONGTHROW_DEPTH
					if ( ! (pSigma[iOffset] & 0x80) )
#else
					dwImageSum += pDepth[iOffset];
					if ( ! (pDepth[iOffset] & 0xF800) )
#endif
						wDepthTemp = pDepth[iOffset] - MIN_OFFSET_DISTANCE;
					((UINT32*) mappedTexture)[--uRpd4i] = (wDepthTemp << 16) | (wDepthTemp << 8) | wDepthTemp;
					m_pImageTemp[iOffset] = wDepthTemp;
				}
			}
#if RBT_LONGTHROW_DEPTH
			{
#else
			if ( dwImageSum != m_dwImageSum )
			{
				// シグマバッファに更新が見られたときにのみ深度バッファ関係を処理する
				bDepthImageUpdated = true;
				m_dwImageSum = dwImageSum;
#endif
				for( int i = 0; i < PACKET_DIVIDE; i++ )
				{
					pImageRBT[i].dwHeader = i;
					CopyMemory( pImageRBT[i].cDepth, &m_pImageTemp[i * (RESOLUTION_SIZE / PACKET_DIVIDE)], RESOLUTION_SIZE * sizeof(UINT16) / PACKET_DIVIDE );
				}
			}

#if ! RBT_LONGTHROW_DEPTH
			if ( bDepthImageUpdated )
#endif
			{
				const UINT16 *pAbImage = nullptr;
				pDepthFrame->GetAbDepthBuffer(&pAbImage, &outBufferCount);

				const UINT32 uRpd4 = texture2D->GetRowPitch() / sizeof(UINT32);
				const UINT32 iW = resolution.Width;
				const UINT32 iH = resolution.Height;
				for (UINT i = 0; i < iH; i++)
				{
					UINT32 uRpd4iW = uRpd4 * i + iW * 2;
					const UINT32 iXY = iW * i;
					for (UINT j = 0; j < iW; j++)
					{
						const UINT32 iOffset = iXY + j;
#if RBT_LONGTHROW_DEPTH
						UINT16 wIrTemp = 0;
						// 深度のクランプ処理
						if ( ! (pSigma[iOffset] & 0x80) )
							wIrTemp = pAbImage[iOffset];
#else
						const UINT16 wIrTemp = pAbImage[iOffset];
#endif
						const UINT32 pixel = (wIrTemp << 16) | (wIrTemp << 8) | wIrTemp;

						((UINT32*) mappedTexture)[--uRpd4iW] = pixel;
						m_pImageTemp[iOffset] = wIrTemp;
					}
				}
				for( int i = 0; i < PACKET_DIVIDE; i++ )
				{
					CopyMemory( pImageRBT[i].cIr, &m_pImageTemp[i * (RESOLUTION_SIZE / PACKET_DIVIDE)], RESOLUTION_SIZE * sizeof(UINT16) / PACKET_DIVIDE );
				}
			}
		}
	}
	if (pDepthFrame)
		pDepthFrame->Release();

	texture2D->UnmapCPUTexture();
	texture2D->CopyCPU2GPU();

	return bDepthImageUpdated;
}

void SlateCameraRenderer::GetModelVertices(std::vector<VertexPositionColor> &modelVertices)
{
	modelVertices.push_back({ XMFLOAT3(-0.01f, -(m_slateWidth / 2), -(m_slateHeight / 2)), XMFLOAT3(0.f, 0.f, 0.f), XMFLOAT2(0.f, 0.f)});
	modelVertices.push_back({ XMFLOAT3(-0.01f, -(m_slateWidth / 2),  (m_slateHeight / 2)), XMFLOAT3(0.f, 0.f, 1.f), XMFLOAT2(0.f, 1.f)});
	modelVertices.push_back({ XMFLOAT3(-0.01f,  (m_slateWidth / 2), -(m_slateHeight / 2)), XMFLOAT3(0.f, 1.f, 0.f), XMFLOAT2(1.f, 0.f)});
	modelVertices.push_back({ XMFLOAT3(-0.01f,  (m_slateWidth / 2),  (m_slateHeight / 2)), XMFLOAT3(0.f, 1.f, 1.f), XMFLOAT2(1.f, 1.f)});
	modelVertices.push_back({ XMFLOAT3(0.01f, -(m_slateWidth / 2), -(m_slateHeight / 2)), XMFLOAT3(1.f, 0.f, 0.f), XMFLOAT2(0.f, 0.f)});
	modelVertices.push_back({ XMFLOAT3(0.01f, -(m_slateWidth / 2),  (m_slateHeight / 2)), XMFLOAT3(1.f, 0.f, 1.f), XMFLOAT2(0.f, 1.f)});
	modelVertices.push_back({ XMFLOAT3(0.01f,  (m_slateWidth / 2), -(m_slateHeight / 2)), XMFLOAT3(1.f, 1.f, 0.f), XMFLOAT2(1.f, 0.f)});
	modelVertices.push_back({ XMFLOAT3(0.01f,  (m_slateWidth / 2),  (m_slateHeight / 2)), XMFLOAT3(1.f, 1.f, 1.f), XMFLOAT2(1.f, 1.f)});
}

void SlateCameraRenderer::GetModelTriangleIndices(std::vector<unsigned short> &triangleIndices)
{
	/*
	2,1,0, // -x
	2,3,1,
	*/
	triangleIndices.push_back(2);
	triangleIndices.push_back(1);
	triangleIndices.push_back(0);

	triangleIndices.push_back(2);
	triangleIndices.push_back(3);
	triangleIndices.push_back(1);

	/*
	6,4,5, // +x
	6,5,7,
	*/
	triangleIndices.push_back(6);
	triangleIndices.push_back(4);
	triangleIndices.push_back(5);

	triangleIndices.push_back(6);
	triangleIndices.push_back(5);
	triangleIndices.push_back(7);

	/*
	0,1,5, // -y
	0,5,4,
	*/
	triangleIndices.push_back(0);
	triangleIndices.push_back(1);
	triangleIndices.push_back(5);

	triangleIndices.push_back(0);
	triangleIndices.push_back(5);
	triangleIndices.push_back(4);

	/*
	2,6,7, // +y
	2,7,3,    
	*/
	triangleIndices.push_back(2);
	triangleIndices.push_back(6);
	triangleIndices.push_back(7);

	triangleIndices.push_back(2);
	triangleIndices.push_back(7);
	triangleIndices.push_back(3);

	/*
	0,4,6, // -z
	0,6,2,
	*/

	triangleIndices.push_back(0);
	triangleIndices.push_back(4);
	triangleIndices.push_back(6);

	triangleIndices.push_back(0);
	triangleIndices.push_back(6);
	triangleIndices.push_back(2);

	/*
	1,3,7, // +z
	1,7,5,    
	*/
	triangleIndices.push_back(1);
	triangleIndices.push_back(3);
	triangleIndices.push_back(7);

	triangleIndices.push_back(1);
	triangleIndices.push_back(7);
	triangleIndices.push_back(5);
}
