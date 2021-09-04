//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#include "Amp12.h"
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
	get_device(acceleratorView)->QueryInterface<ID3D11On12Device>(&m_device11On12);
}

Amp12::~Amp12()
{
}

bool Amp12::Init(CommandList* pCommandList,  vector<Resource::uptr>& uploaders,
	Format rtFormat, const wchar_t* fileName)
{
	// Load input image
	{
		DDS::Loader textureLoader;
		DDS::AlphaMode alphaMode;

		uploaders.emplace_back(Resource::MakeUnique());
		N_RETURN(textureLoader.CreateTextureFromFile(m_device.get(), pCommandList, fileName,
			8192, false, m_source, uploaders.back().get(), &alphaMode), false);
	}

	// Create resources
	m_imageSize.x = m_source->GetWidth();
	m_imageSize.y = dynamic_pointer_cast<Texture2D, ShaderResource>(m_source)->GetHeight();

	m_result = Texture2D::MakeUnique();
	N_RETURN(m_result->Create(m_device.get(), m_imageSize.x, m_imageSize.y, rtFormat, 1,
		ResourceFlag::ALLOW_UNORDERED_ACCESS | ResourceFlag::ALLOW_SIMULTANEOUS_ACCESS,
		1, 1, MemoryType::DEFAULT, false, L"Result"), false);

	// Wrap DX11 resources
	D3D11_RESOURCE_FLAGS dx11ResourceFlags = { D3D11_BIND_SHADER_RESOURCE };
	//dx11ResourceFlags.MiscFlags = D3D11_RESOURCE_MISC_RESTRICT_SHARED_RESOURCE;
	if (FAILED(m_device11On12->CreateWrappedResource(reinterpret_cast<IUnknown*>(m_source->GetHandle()),
		&dx11ResourceFlags, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
		D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, IID_PPV_ARGS(&m_source11))))
		return false;

	dx11ResourceFlags.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	if (FAILED(m_device11On12->CreateWrappedResource(reinterpret_cast<IUnknown*>(m_result->GetHandle()),
		&dx11ResourceFlags, D3D12_RESOURCE_STATE_COPY_SOURCE,
		D3D12_RESOURCE_STATE_COPY_SOURCE, IID_PPV_ARGS(&m_result11))))
		return false;

	// Wrap AMP resources
	m_sourceAMP = make_unique<texture<unorm_4, 2>>(make_texture<unorm_4, 2>(m_acceleratorView, m_source11.get()));
	m_resultAMP = make_unique<texture<unorm_4, 2>>(make_texture<unorm_4, 2>(m_acceleratorView, m_result11.get()));

	ResourceBarrier barrier;
	m_result->SetBarrier(&barrier, ResourceState::COPY_SOURCE);

	return true;
}

void Amp12::Process()
{
	ID3D11Resource* const pResources11[] = { m_source11.get(), m_result11.get() };
	m_device11On12->AcquireWrappedResources(pResources11, static_cast<uint32_t>(size(pResources11)));

	const auto source = texture_view<const unorm_4, 2>(*m_sourceAMP);
	const auto result = texture_view<unorm_4, 2>(*m_resultAMP);

	parallel_for_each(
		// Define the compute domain, which is the set of threads that are created.
		result.extent,
		// Define the code to run on each thread on the accelerator.
		[=](const index<2> idx) restrict(amp)
		{
			const auto src = source[idx];
			const auto dst = src.x * 0.299f + src.y * 0.587f + src.z * 0.114f;

			result.set(idx, unorm_4(dst, dst, dst, src.w));
		}
	);

	m_device11On12->ReleaseWrappedResources(pResources11, static_cast<uint32_t>(size(pResources11)));
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
