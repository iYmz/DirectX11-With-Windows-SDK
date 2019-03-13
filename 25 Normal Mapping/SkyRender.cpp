#include "SkyRender.h"
#include "Geometry.h"
#include "d3dUtil.h"
#include "DXTrace.h"
using namespace DirectX;
using namespace Microsoft::WRL;

SkyRender::SkyRender(
	ComPtr<ID3D11Device> device,
	ComPtr<ID3D11DeviceContext> deviceContext,
	const std::wstring & cubemapFilename,
	float skySphereRadius,
	bool generateMips)
{
	// ��պ���������
	if (cubemapFilename.substr(cubemapFilename.size() - 3) == L"dds")
	{
		HR(CreateDDSTextureFromFile(
			device.Get(),
			generateMips ? deviceContext.Get() : nullptr,
			cubemapFilename.c_str(),
			nullptr,
			mTextureCubeSRV.GetAddressOf()
		));
	}
	else
	{
		HR(CreateWICTexture2DCubeFromFile(
			device.Get(),
			deviceContext.Get(),
			cubemapFilename,
			nullptr,
			mTextureCubeSRV.GetAddressOf(),
			generateMips
		));
	}

	InitResource(device, skySphereRadius);
}

SkyRender::SkyRender(ComPtr<ID3D11Device> device,
	ComPtr<ID3D11DeviceContext> deviceContext,
	const std::vector<std::wstring>& cubemapFilenames,
	float skySphereRadius,
	bool generateMips)
{
	// ��պ���������

	HR(CreateWICTexture2DCubeFromFile(
		device.Get(),
		deviceContext.Get(),
		cubemapFilenames,
		nullptr,
		mTextureCubeSRV.GetAddressOf(),
		generateMips
	));

	InitResource(device, skySphereRadius);
}

ComPtr<ID3D11ShaderResourceView> SkyRender::GetTextureCube()
{
	return mTextureCubeSRV;
}

void SkyRender::Draw(ComPtr<ID3D11DeviceContext> deviceContext, SkyEffect & skyEffect, const Camera & camera)
{
	UINT strides[1] = { sizeof(XMFLOAT3) };
	UINT offsets[1] = { 0 };
	deviceContext->IASetVertexBuffers(0, 1, mVertexBuffer.GetAddressOf(), strides, offsets);
	deviceContext->IASetIndexBuffer(mIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

	XMFLOAT3 pos = camera.GetPosition();
	skyEffect.SetWorldViewProjMatrix(XMMatrixTranslation(pos.x, pos.y, pos.z) * camera.GetViewProjXM());
	skyEffect.SetTextureCube(mTextureCubeSRV);
	skyEffect.Apply(deviceContext);
	deviceContext->DrawIndexed(mIndexCount, 0, 0);
}

void SkyRender::InitResource(ComPtr<ID3D11Device> device, float skySphereRadius)
{
	auto sphere = Geometry::CreateSphere<VertexPos>(skySphereRadius);

	// ���㻺��������
	D3D11_BUFFER_DESC vbd;
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	vbd.ByteWidth = sizeof(XMFLOAT3) * (UINT)sphere.vertexVec.size();
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = 0;
	vbd.MiscFlags = 0;
	vbd.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA InitData;
	InitData.pSysMem = sphere.vertexVec.data();

	HR(device->CreateBuffer(&vbd, &InitData, &mVertexBuffer));

	// ��������������
	mIndexCount = (UINT)sphere.indexVec.size();

	D3D11_BUFFER_DESC ibd;
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.ByteWidth = sizeof(WORD) * mIndexCount;
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.CPUAccessFlags = 0;
	ibd.StructureByteStride = 0;
	ibd.MiscFlags = 0;

	InitData.pSysMem = sphere.indexVec.data();

	HR(device->CreateBuffer(&ibd, &InitData, &mIndexBuffer));

}

DynamicSkyRender::DynamicSkyRender(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> deviceContext, const std::wstring & cubemapFilename, float skySphereRadius, int dynamicCubeSize, bool generateMips)
	: SkyRender(device, deviceContext, cubemapFilename, skySphereRadius, generateMips)
{
	InitResource(device, dynamicCubeSize);
}

DynamicSkyRender::DynamicSkyRender(ComPtr<ID3D11Device> device, ComPtr<ID3D11DeviceContext> deviceContext, const std::vector<std::wstring>& cubemapFilenames, float skySphereRadius, int dynamicCubeSize, bool generateMips)
	: SkyRender(device, deviceContext, cubemapFilenames, skySphereRadius, generateMips)
{
	InitResource(device, dynamicCubeSize);
}

void DynamicSkyRender::Cache(ComPtr<ID3D11DeviceContext> deviceContext, BasicEffect& effect)
{
	deviceContext->OMGetRenderTargets(1, mCacheRTV.GetAddressOf(), mCacheDSV.GetAddressOf());

	// ���������ɫ���Ķ�̬��պУ���Ҫ������Ч
	effect.SetTextureCube(nullptr);
	effect.Apply(deviceContext);
}

void DynamicSkyRender::BeginCapture(ComPtr<ID3D11DeviceContext> deviceContext, BasicEffect& effect, const XMFLOAT3& pos,
	D3D11_TEXTURECUBE_FACE face, float nearZ, float farZ)
{
	static XMVECTORF32 ups[6] = {
		{{ 0.0f, 1.0f, 0.0f, 0.0f }},	// +X
		{{ 0.0f, 1.0f, 0.0f, 0.0f }},	// -X
		{{ 0.0f, 0.0f, -1.0f, 0.0f }},	// +Y
		{{ 0.0f, 0.0f, 1.0f, 0.0f }},	// -Y
		{{ 0.0f, 1.0f, 0.0f, 0.0f }},	// +Z
		{{ 0.0f, 1.0f, 0.0f, 0.0f }}	// -Z
	};

	static XMVECTORF32 looks[6] = {
		{{ 1.0f, 0.0f, 0.0f, 0.0f }},	// +X
		{{ -1.0f, 0.0f, 0.0f, 0.0f }},	// -X
		{{ 0.0f, 1.0f, 0.0f, 0.0f }},	// +Y
		{{ 0.0f, -1.0f, 0.0f, 0.0f }},	// -Y
		{{ 0.0f, 0.0f, 1.0f, 0.0f }},	// +Z
		{{ 0.0f, 0.0f, -1.0f, 0.0f }},	// -Z
	};
	
	// ������պ������
	mCamera.LookTo(XMLoadFloat3(&pos) , looks[face].v, ups[face].v);
	mCamera.UpdateViewMatrix();
	// ���ﾡ���ܲ������������
	mCamera.SetFrustum(XM_PIDIV2, 1.0f, nearZ, farZ);

	// Ӧ�ù۲����ͶӰ����
	effect.SetViewMatrix(mCamera.GetViewXM());
	effect.SetProjMatrix(mCamera.GetProjXM());

	// ��ջ�����
	deviceContext->ClearRenderTargetView(mDynamicCubeMapRTVs[face].Get(), reinterpret_cast<const float*>(&Colors::Black));
	deviceContext->ClearDepthStencilView(mDynamicCubeMapDSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	// ������ȾĿ������ģ����ͼ
	deviceContext->OMSetRenderTargets(1, mDynamicCubeMapRTVs[face].GetAddressOf(), mDynamicCubeMapDSV.Get());
	// �����ӿ�
	deviceContext->RSSetViewports(1, &mCamera.GetViewPort());
}



void DynamicSkyRender::Restore(ComPtr<ID3D11DeviceContext> deviceContext, BasicEffect& effect, const Camera & camera)
{
	// �ָ�Ĭ���趨
	deviceContext->RSSetViewports(1, &camera.GetViewPort());
	deviceContext->OMSetRenderTargets(1, mCacheRTV.GetAddressOf(), mCacheDSV.Get());

	// ���ɶ�̬��պк����Ҫ����mipmap��
	deviceContext->GenerateMips(mDynamicCubeMapSRV.Get());

	effect.SetViewMatrix(camera.GetViewXM());
	effect.SetProjMatrix(camera.GetProjXM());
	// �ָ��󶨵Ķ�̬��պ�
	effect.SetTextureCube(mDynamicCubeMapSRV);

	// �����ʱ�������ȾĿ����ͼ�����ģ����ͼ
	mCacheDSV.Reset();
	mCacheRTV.Reset();
}

ComPtr<ID3D11ShaderResourceView> DynamicSkyRender::GetDynamicTextureCube()
{
	return mDynamicCubeMapSRV;
}

const Camera & DynamicSkyRender::GetCamera() const
{
	return mCamera;
}

void DynamicSkyRender::InitResource(ComPtr<ID3D11Device> device, int dynamicCubeSize)
{

	// ******************
	// 1. ������������
	//

	ComPtr<ID3D11Texture2D> texCube;
	D3D11_TEXTURE2D_DESC texDesc;

	texDesc.Width = dynamicCubeSize;
	texDesc.Height = dynamicCubeSize;
	texDesc.MipLevels = 0;
	texDesc.ArraySize = 6;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS | D3D11_RESOURCE_MISC_TEXTURECUBE;
	
	// ����texCube�����½�����
	HR(device->CreateTexture2D(&texDesc, nullptr, texCube.ReleaseAndGetAddressOf()));

	// ******************
	// 2. ������ȾĿ����ͼ
	//

	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
	rtvDesc.Format = texDesc.Format;
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
	rtvDesc.Texture2DArray.MipSlice = 0;
	// һ����ͼֻ��Ӧһ����������Ԫ��
	rtvDesc.Texture2DArray.ArraySize = 1;

	// ÿ��Ԫ�ش���һ����ȾĿ����ͼ
	for (int i = 0; i < 6; ++i)
	{
		rtvDesc.Texture2DArray.FirstArraySlice = i;
		HR(device->CreateRenderTargetView(
			texCube.Get(),
			&rtvDesc,
			mDynamicCubeMapRTVs[i].GetAddressOf()));
	}

	// ******************
	// 3. ������ɫ��Ŀ����ͼ
	//

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	srvDesc.Format = texDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = -1;	// ʹ�����е�mip�ȼ�

	HR(device->CreateShaderResourceView(
		texCube.Get(),
		&srvDesc,
		mDynamicCubeMapSRV.GetAddressOf()));
	
	// ******************
	// 4. �������/ģ�建�������Ӧ����ͼ
	//

	texDesc.Width = dynamicCubeSize;
	texDesc.Height = dynamicCubeSize;
	texDesc.MipLevels = 0;
	texDesc.ArraySize = 1;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Format = DXGI_FORMAT_D32_FLOAT;
	texDesc.Usage = D3D11_USAGE_DEFAULT;
	texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	texDesc.CPUAccessFlags = 0;
	texDesc.MiscFlags = 0;

	ComPtr<ID3D11Texture2D> depthTex;
	device->CreateTexture2D(&texDesc, nullptr, depthTex.GetAddressOf());

	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Format = texDesc.Format;
	dsvDesc.Flags = 0;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;

	HR(device->CreateDepthStencilView(
		depthTex.Get(),
		&dsvDesc,
		mDynamicCubeMapDSV.GetAddressOf()));

	// ******************
	// 5. ��ʼ���ӿ�
	//

	mCamera.SetViewPort(0.0f, 0.0f, static_cast<float>(dynamicCubeSize), static_cast<float>(dynamicCubeSize));

}