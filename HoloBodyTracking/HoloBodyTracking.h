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

#include "BasicHologramMain.h"
#include "BodyStruct.h"

namespace BasicHologram
{
	class SensorVisualizationScenario : public Scenario
	{
	public:
		SensorVisualizationScenario(std::shared_ptr<DX::DeviceResources> const& deviceResources);
		virtual ~SensorVisualizationScenario();

		void IntializeSensors();
		void IntializeModelRendering();
		void UpdateModels(DX::StepTimer &timer);
		void PositionHologram(winrt::Windows::UI::Input::Spatial::SpatialPointerPose const& pointerPose, const DX::StepTimer& timer);
		void PositionHologramNoSmoothing(winrt::Windows::UI::Input::Spatial::SpatialPointerPose const& pointerPose);
		winrt::Windows::Foundation::Numerics::float3 const& GetPosition()
		{
			return m_modelRenderers[0]->GetPosition();
		}
		void RenderModels();
		void OnDeviceLost();
		void OnDeviceRestored();
		static void CamAccessOnComplete(ResearchModeSensorConsent consent);

    protected:

		IResearchModeSensorDevice *m_pSensorDevice;
		IResearchModeSensorDeviceConsent* m_pSensorDeviceConsent;
		std::vector<ResearchModeSensorDescriptor> m_sensorDescriptors;
		IResearchModeSensor *m_pSensor = nullptr;
		std::shared_ptr<XAxisModel> m_btaxisOriginRenderer[MAX_BODIES][K4ABT_JOINT_COUNT];
		std::vector<std::shared_ptr<ModelRenderer>> m_modelRenderers;
		std::shared_ptr<SocketRenderer> m_SocketRenderer;

		volatile long m_lDepthImageUpdatedRendererIndex;
		RBT_STRUCT* m_pImageRBT;
		k4abt_skeleton_t m_SkeletonStatic[MAX_BODIES];
	};
}
