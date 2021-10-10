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
#include "HoloBodyTracking.h"

extern "C" HMODULE LoadLibraryA(LPCSTR lpLibFileName);

using namespace BasicHologram;
using namespace winrt::Windows::Foundation::Numerics;

static ResearchModeSensorConsent camAccessCheck;
static HANDLE camConsentGiven;

#define TRANS_OFSMUL	(0.46f)
#define TRANS_OFSADD	(0.27f)

SensorVisualizationScenario::SensorVisualizationScenario(std::shared_ptr<DX::DeviceResources> const& deviceResources) :
	Scenario(deviceResources)
{
	m_pImageRBT = nullptr;
	m_lDepthImageUpdatedRendererIndex = -1;
	ZeroMemory( m_SkeletonStatic, sizeof(k4abt_skeleton_t) * MAX_BODIES );
}

SensorVisualizationScenario::~SensorVisualizationScenario()
{
	if (m_pSensor)
	{
		m_pSensor->Release();
	}
	if (m_pSensorDevice)
	{
		m_pSensorDevice->EnableEyeSelection();
		m_pSensorDevice->Release();
	}
}

void SensorVisualizationScenario::CamAccessOnComplete(ResearchModeSensorConsent consent)
{
	camAccessCheck = consent;
	SetEvent(camConsentGiven);
}

void SensorVisualizationScenario::IntializeSensors()
{
	HRESULT hr = S_OK;
	size_t sensorCount = 0;
	camConsentGiven = CreateEvent(nullptr, true, false, nullptr);

	HMODULE hrResearchMode = LoadLibraryA("ResearchModeAPI");
	if (hrResearchMode)
	{
		typedef HRESULT(__cdecl* PFN_CREATEPROVIDER) (IResearchModeSensorDevice** ppSensorDevice);
		PFN_CREATEPROVIDER pfnCreate = reinterpret_cast<PFN_CREATEPROVIDER>(GetProcAddress(hrResearchMode, "CreateResearchModeSensorDevice"));
		if (pfnCreate)
		{
			winrt::check_hresult(pfnCreate(&m_pSensorDevice));
		}
		else
		{
			winrt::check_hresult(E_INVALIDARG);
		}
	}

	winrt::check_hresult(m_pSensorDevice->QueryInterface(IID_PPV_ARGS(&m_pSensorDeviceConsent)));
	winrt::check_hresult(m_pSensorDeviceConsent->RequestCamAccessAsync(SensorVisualizationScenario::CamAccessOnComplete));

	m_pSensorDevice->DisableEyeSelection();

	winrt::check_hresult(m_pSensorDevice->GetSensorCount(&sensorCount));
	m_sensorDescriptors.resize(sensorCount);

	winrt::check_hresult(m_pSensorDevice->GetSensorDescriptors(m_sensorDescriptors.data(), m_sensorDescriptors.size(), &sensorCount));

	for (auto sensorDescriptor : m_sensorDescriptors)
	{
		if (sensorDescriptor.sensorType == DEPTH_LONG_THROW || sensorDescriptor.sensorType == DEPTH_AHAT)
			winrt::check_hresult(m_pSensorDevice->GetSensor(sensorDescriptor.sensorType, &m_pSensor));
	}
}

void SensorVisualizationScenario::IntializeModelRendering()
{
	if (m_pSensor)
	{
		DirectX::XMMATRIX modelRotation = DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(0.f, 0.f, 1.f, 0.f), DirectX::XM_PIDIV2);

		// Initialize the sample hologram.
		auto slateCameraRenderer = std::make_shared<SlateCameraRenderer>(m_deviceResources, 0.25f, 0.125f, m_pSensor, camConsentGiven, &camAccessCheck);

		float3 offset;
		offset.x = -0.25f;
		offset.y = -0.25f;
		offset.z = 0.0f;

		slateCameraRenderer->SetOffset(offset);
		slateCameraRenderer->SetModelTransform(modelRotation);
		m_modelRenderers.push_back(slateCameraRenderer);
	}
	m_SocketRenderer = std::make_shared<SocketRenderer>(m_deviceResources, nullptr, camConsentGiven, &camAccessCheck);
	m_pImageRBT = m_SocketRenderer->GetRBT(&m_lDepthImageUpdatedRendererIndex);

	DirectX::XMMATRIX groupRotation = DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(0.f, 0.f, 1.f, 0.f), DirectX::XM_PIDIV2);
	DirectX::XMMATRIX groupRotationBT = DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(0.f, 0.f, -1.f, 0.f), DirectX::XM_PIDIV2) * DirectX::XMMatrixTranslation(0.f, -1.f, 1.f);
	const DirectX::XMFLOAT3 f3ColorTable[K4ABT_JOINT_COUNT] = {
		{ 1.f, 1.f, 1.f }, { 1.f, 1.f, 1.f }, { 1.f, 1.f, 1.f }, { 1.f, 1.f, 1.f },
		{ 0.f, 0.f, 1.f }, { 0.f, 0.f, 1.f }, { 0.f, 0.f, 1.f }, { 0.f, 0.f, 1.f }, { 0.f, 0.f, 1.f }, { 0.f, 0.f, 1.f }, { 0.f, 0.f, 1.f },
		{ 0.f, 1.f, 0.f }, { 0.f, 1.f, 0.f }, { 0.f, 1.f, 0.f }, { 0.f, 1.f, 0.f }, { 0.f, 1.f, 0.f }, { 0.f, 1.f, 0.f }, { 0.f, 1.f, 0.f },
		{ 1.f, 0.f, 0.f }, { 1.f, 0.f, 0.f }, { 1.f, 0.f, 0.f }, { 1.f, 0.f, 0.f },
		{ 0.f, 1.f, 1.f }, { 0.f, 1.f, 1.f }, { 0.f, 1.f, 1.f }, { 0.f, 1.f, 1.f },
		{ 1.f, 0.f, 1.f }, { 1.f, 0.f, 1.f }, { 1.f, 0.f, 1.f }, { 1.f, 0.f, 1.f }, { 1.f, 0.f, 1.f }, { 1.f, 0.f, 1.f },
	};
	const float3 offset = { 0.f, 0.f, 0.f };
	for( uint32_t j = 0; j < MAX_BODIES; j++ )
	{
		for( size_t i = 0; i < K4ABT_JOINT_COUNT; i++ )
		{
			auto btaxisOriginRenderer = std::make_shared<XAxisModel>(m_deviceResources, 0.02f, 0.01f);
			btaxisOriginRenderer->SetGroupScaleFactor(1.0f);
			btaxisOriginRenderer->SetGroupTransform(groupRotationBT);
			btaxisOriginRenderer->SetColor( f3ColorTable[i] );
			m_modelRenderers.push_back(btaxisOriginRenderer);
			btaxisOriginRenderer->SetOffset(offset);
			m_btaxisOriginRenderer[j][i] = btaxisOriginRenderer;
		}
	}
}

void SensorVisualizationScenario::PositionHologram(winrt::Windows::UI::Input::Spatial::SpatialPointerPose const& pointerPose, const DX::StepTimer& timer)
{
	// When a Pressed gesture is detected, the sample hologram will be repositioned
	// two meters in front of the user.
	for (size_t i = 0; i < m_modelRenderers.size(); i++)
	{
		m_modelRenderers[i]->PositionHologram(pointerPose, timer);
	}
}

void SensorVisualizationScenario::PositionHologramNoSmoothing(winrt::Windows::UI::Input::Spatial::SpatialPointerPose const& pointerPose)
{
	// When a Pressed gesture is detected, the sample hologram will be repositioned
	// two meters in front of the user.
	for (size_t i = 0; i < m_modelRenderers.size(); i++)
	{
		m_modelRenderers[i]->PositionHologramNoSmoothing(pointerPose);
	}
}

void SensorVisualizationScenario::UpdateModels(DX::StepTimer &timer)
{
	HRESULT hr = S_OK;

	for (size_t i = 0; i < m_modelRenderers.size(); i++)
		m_modelRenderers[i]->Update(timer);

	const float& deltaTime = static_cast<float>(timer.GetElapsedSeconds());
	const uint32_t uBodies = m_SocketRenderer->GetBodies();
	for( uint32_t j = 0; j < uBodies; j++ )
	{
		k4abt_skeleton_t skeleton;
		m_SocketRenderer->GetSkeleton( j, &skeleton );
		for( size_t i = 0; i < K4ABT_JOINT_COUNT; i++ )
		{
			const float3 smoothedPosition = lerp( (float3&) (m_SkeletonStatic[j].joints[i].position), (float3&) skeleton.joints[i].position, deltaTime * 16.0f );
			const float fOfx = (skeleton.joints[i].position.xyz.z * 0.001f - 3.0f) * TRANS_OFSMUL + TRANS_OFSADD;
			DirectX::XMMATRIX btscaleTransform = DirectX::XMMatrixTranslation( (smoothedPosition.y * 0.001f) + fOfx, (smoothedPosition.x * 0.001f), (smoothedPosition.z * -0.001f) );
			m_SkeletonStatic[j].joints[i].position = (k4a_float3_t&) smoothedPosition;
			m_btaxisOriginRenderer[j][i]->SetModelTransform(btscaleTransform);
		}
	}
}

void SensorVisualizationScenario::RenderModels()
{
	// Draw the sample hologram.
	for (size_t i = 0; i < m_modelRenderers.size(); i++)
	{
		if ( m_modelRenderers[i]->Render(m_pImageRBT) )
			InterlockedExchange( &m_lDepthImageUpdatedRendererIndex, (long) i );
	}
}

void SensorVisualizationScenario::OnDeviceLost()
{
	for (size_t i = 0; i < m_modelRenderers.size(); i++)
	{
		m_modelRenderers[i]->ReleaseDeviceDependentResources();
	}
}

void SensorVisualizationScenario::OnDeviceRestored()
{
	for (size_t i = 0; i < m_modelRenderers.size(); i++)
	{
		m_modelRenderers[i]->CreateDeviceDependentResources();
	}
}
