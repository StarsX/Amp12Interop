//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#include "Amp12.h"
#include "AmpVecMath.h"
#define _INDEPENDENT_DDS_LOADER_
#include "Advanced/XUSGDDSLoader.h"
#undef _INDEPENDENT_DDS_LOADER_

using namespace std;
using namespace Concurrency;
using namespace Concurrency::direct3d;
using namespace Concurrency::graphics;
using namespace Concurrency::graphics::direct3d;
using namespace DirectX;
using namespace XUSG;

Amp12::Amp12(const accelerator_view& acceleratorView, const Device::sptr& device) :
	m_acceleratorView(acceleratorView),
	m_device(device),
	m_imageSize(1, 1)
{
	const auto pDevice = get_device(acceleratorView);
	pDevice->QueryInterface<ID3D11Device1>(&m_device11);
	SAFE_RELEASE(pDevice);
}

Amp12::~Amp12()
{
	amp_uninitialize();
}

bool Amp12::Init(CommandList* pCommandList,  vector<Resource::uptr>& uploaders,
	Format rtFormat, const wchar_t* fileName, Texture::sptr* pSrcForNative11)
{
	m_useNativeDX11 = pSrcForNative11 ? true : false;
	auto& source = pSrcForNative11 ? *pSrcForNative11 : m_source;

	// Load input image
	{
		DDS::Loader textureLoader;
		DDS::AlphaMode alphaMode;

		uploaders.emplace_back(Resource::MakeUnique());
		N_RETURN(textureLoader.CreateTextureFromFile(m_device.get(), pCommandList, fileName,
			8192, false, source, uploaders.back().get(), &alphaMode, ResourceState::COMMON,
			MemoryFlag::SHARED), false);
	}

	// Create resources
	m_imageSize.x = static_cast<uint32_t>(source->GetWidth());
	m_imageSize.y = source->GetHeight();

	auto resourceFlags = ResourceFlag::ALLOW_UNORDERED_ACCESS | ResourceFlag::ALLOW_SIMULTANEOUS_ACCESS;
	resourceFlags |= m_useNativeDX11 ? ResourceFlag::ALLOW_RENDER_TARGET : ResourceFlag::NONE;
	m_result = Texture2D::MakeUnique();
	N_RETURN(m_result->Create(m_device.get(), m_imageSize.x, m_imageSize.y, rtFormat, 1,
		resourceFlags, 1, 1, false, MemoryFlag::SHARED, L"Result"), false);

	// Wrap DX11 resources
	if (m_useNativeDX11)
	{
		// Native DX11 interop only supports resources with ALLOW_RENDER_TARGET
		// So, we need to create a render-target-compatible resource and then copy the source data to it
		resourceFlags = ResourceFlag::ALLOW_RENDER_TARGET | ResourceFlag::ALLOW_SIMULTANEOUS_ACCESS;
		m_source = Texture2D::MakeUnique();
		N_RETURN(m_source->Create(m_device.get(), m_imageSize.x, m_imageSize.y, rtFormat, 1,
			resourceFlags, 1, 1, false, MemoryFlag::SHARED, L"Source"), false);

		// Share DX12 resources
		const auto pDevice12 = static_cast<ID3D12Device*>(m_device->GetHandle());
		HANDLE hSource;
		M_RETURN(FAILED(pDevice12->CreateSharedHandle(static_cast<ID3D12Resource*>(m_source->GetHandle()),
			nullptr, GENERIC_ALL, nullptr, &hSource)), cerr, "Failed to share Source.", false);

		HANDLE hResult;
		M_RETURN(FAILED(pDevice12->CreateSharedHandle(static_cast<ID3D12Resource*>(m_result->GetHandle()),
			nullptr, GENERIC_ALL, nullptr, &hResult)), cerr, "Failed to share Result.", false);

		// Open resource handles on DX11
		M_RETURN(FAILED(m_device11->OpenSharedResource1(hSource, IID_PPV_ARGS(&m_source11))),
			cerr, "Failed to open shared Source on DX11.", false);

		M_RETURN(FAILED(m_device11->OpenSharedResource1(hResult, IID_PPV_ARGS(&m_result11))),
			cerr, "Failed to open shared Result on DX11.", false);
	}
	else
	{
		// Wrap DX11 resources
		com_ptr<ID3D11On12Device> device11On12;
		m_device11->QueryInterface<ID3D11On12Device>(&device11On12);
		D3D11_RESOURCE_FLAGS dx11ResourceFlags = { D3D11_BIND_SHADER_RESOURCE };
		dx11ResourceFlags.MiscFlags = D3D11_RESOURCE_MISC_SHARED;
		if (FAILED(device11On12->CreateWrappedResource(reinterpret_cast<IUnknown*>(m_source->GetHandle()),
			&dx11ResourceFlags, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
			D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, IID_PPV_ARGS(&m_source11))))
			return false;

		dx11ResourceFlags.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
		if (FAILED(device11On12->CreateWrappedResource(reinterpret_cast<IUnknown*>(m_result->GetHandle()),
			&dx11ResourceFlags, D3D12_RESOURCE_STATE_COPY_SOURCE,
			D3D12_RESOURCE_STATE_COPY_SOURCE, IID_PPV_ARGS(&m_result11))))
			return false;
	}

	// Wrap AMP resources
	m_sourceAMP = make_unique<texture<unorm4, 2>>(make_texture<unorm4, 2>(m_acceleratorView, m_source11.get()));
	m_resultAMP = make_unique<texture<unorm4, 2>>(make_texture<unorm4, 2>(m_acceleratorView, m_result11.get()));

	ResourceBarrier barrier;
	m_result->SetBarrier(&barrier, ResourceState::COPY_SOURCE);
	if (m_useNativeDX11)
	{
		auto numBarriers = m_source->SetBarrier(&barrier, ResourceState::COPY_DEST);
		pCommandList->Barrier(numBarriers, &barrier);
		pCommandList->CopyResource(m_source.get(), source.get());
		numBarriers = m_source->SetBarrier(&barrier, ResourceState::NON_PIXEL_SHADER_RESOURCE);
		pCommandList->Barrier(numBarriers, &barrier);
	}

	return true;
}

void Amp12::Process()
{
	com_ptr<ID3D11On12Device> device11On12;
	ID3D11Resource* const pResources11[] = { m_source11.get(), m_result11.get() };
	if (!m_useNativeDX11)
	{
		m_device11->QueryInterface<ID3D11On12Device>(&device11On12);
		device11On12->AcquireWrappedResources(pResources11, static_cast<uint32_t>(size(pResources11)));
	}

	const auto source = texture_view<const unorm4, 2>(*m_sourceAMP);
	const auto result = texture_view<unorm4, 2>(*m_resultAMP);

	parallel_for_each(
		// Define the compute domain, which is the set of threads that are created.
		result.extent,
		// Define the code to run on each thread on the accelerator.
		[=](const index<2>& idx) restrict(amp)
		{
			const uint2 xy(idx[1], idx[0]);
			const uint2 imageSize(result.extent[1], result.extent[0]);
			const auto uv = (float2(xy) + 0.5f) / float2(imageSize);

			const auto src = source.sample(uv, 0.0f);
			const auto dst = dot(src.xyz, unorm3(0.299f, 0.587f, 0.114f));

			result.set(idx, unorm4(dst, dst, dst, src.w));
		}
	);

	if (!m_useNativeDX11)
		device11On12->ReleaseWrappedResources(pResources11, static_cast<uint32_t>(size(pResources11)));
}

void Amp12::GetImageSize(uint32_t& width, uint32_t& height) const
{
	width = m_imageSize.x;
	height = m_imageSize.y;
}

const Texture2D* Amp12::GetResult() const
{
	return m_result.get();
}
