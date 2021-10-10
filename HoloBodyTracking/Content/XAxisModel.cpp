#include "pch.h"
#include "XAxisModel.h"

using namespace BasicHologram;
using namespace DirectX;

DirectX::XMMATRIX XAxisModel::GetModelRotation()
{
	return DirectX::XMMatrixIdentity();
}

void XAxisModel::GetModelVertices(std::vector<VertexPositionColor> &modelVertices)
{
	GetModelXAxisVertices(modelVertices);
}

void XAxisModel::GetModelXAxisVertices(std::vector<VertexPositionColor> &modelVertices)
{
	modelVertices.push_back({ XMFLOAT3(0.f, -(m_thick/2), -(m_thick/2)), m_originColor, XMFLOAT2(0.f, 0.f)});
	modelVertices.push_back({ XMFLOAT3(0.f, -(m_thick/2),  (m_thick/2)), m_originColor, XMFLOAT2(0.f, 1.f)});
	modelVertices.push_back({ XMFLOAT3(0.f,  (m_thick/2), -(m_thick/2)), m_originColor, XMFLOAT2(1.f, 0.f)});
	modelVertices.push_back({ XMFLOAT3(0.f,  (m_thick/2),  (m_thick/2)), m_originColor, XMFLOAT2(1.f, 1.f)});
	modelVertices.push_back({ XMFLOAT3(m_length, -(m_thick/2), -(m_thick/2)), m_endColor, XMFLOAT2(0.f, 0.f)});
	modelVertices.push_back({ XMFLOAT3(m_length, -(m_thick/2),  (m_thick/2)), m_endColor, XMFLOAT2(0.f, 1.f)});
	modelVertices.push_back({ XMFLOAT3(m_length,  (m_thick/2), -(m_thick/2)), m_endColor, XMFLOAT2(1.f, 0.f)});
	modelVertices.push_back({ XMFLOAT3(m_length,  (m_thick/2),  (m_thick/2)), m_endColor, XMFLOAT2(1.f, 1.f)});
}

void XAxisModel::GetOriginCenterdCubeModelVertices(std::vector<VertexPositionColor> &modelVertices)
{
	modelVertices.push_back({ XMFLOAT3(-0.2f, -0.2f, -0.2f), XMFLOAT3(0.f, 0.f, 0.f), XMFLOAT2(0.f, 0.f)});
	modelVertices.push_back({ XMFLOAT3(-0.2f, -0.2f,  0.2f), XMFLOAT3(0.f, 0.f, 1.f), XMFLOAT2(0.f, 1.f)});
	modelVertices.push_back({ XMFLOAT3(-0.2f,  0.2f, -0.2f), XMFLOAT3(0.f, 1.f, 0.f), XMFLOAT2(1.f, 0.f)});
	modelVertices.push_back({ XMFLOAT3(-0.2f,  0.2f,  0.2f), XMFLOAT3(0.f, 1.f, 1.f), XMFLOAT2(1.f, 1.f)});
	modelVertices.push_back({ XMFLOAT3(0.2f, -0.2f, -0.2f), XMFLOAT3(1.f, 0.f, 0.f), XMFLOAT2(0.f, 0.f)});
	modelVertices.push_back({ XMFLOAT3(0.2f, -0.2f,  0.2f), XMFLOAT3(1.f, 0.f, 1.f), XMFLOAT2(0.f, 1.f)});
	modelVertices.push_back({ XMFLOAT3(0.2f,  0.2f, -0.2f), XMFLOAT3(1.f, 1.f, 0.f), XMFLOAT2(1.f, 0.f)});
	modelVertices.push_back({ XMFLOAT3(0.2f,  0.2f,  0.2f), XMFLOAT3(1.f, 1.f, 1.f), XMFLOAT2(1.f, 1.f)});
}

void XAxisModel::GetModelTriangleIndices(std::vector<unsigned short> &triangleIndices)
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
