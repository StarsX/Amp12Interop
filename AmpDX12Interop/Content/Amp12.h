//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

#include "DXFramework.h"
#include "Core/XUSG.h"

class Amp12
{
public:
	Amp12(const XUSG::Device::sptr& device, const concurrency::accelerator_view& acceleratorView,
		const XUSG::com_ptr<ID3D11On12Device>& device11On12);
	virtual ~Amp12();

	bool Init(XUSG::CommandList* pCommandList, std::vector<XUSG::Resource::uptr>& uploaders,
		XUSG::Format rtFormat, const wchar_t* fileName);

	void Process(const XUSG::CommandList* pCommandList);

	void GetImageSize(uint32_t& width, uint32_t& height) const;

	const XUSG::Texture2D* GetResult() const;

protected:
	concurrency::accelerator_view m_acceleratorView;
	XUSG::Device::sptr m_device;

	XUSG::ShaderResource::sptr			m_source;
	XUSG::Texture2D::uptr				m_result;

	XUSG::com_ptr<ID3D11On12Device>		m_device11On12;
	XUSG::com_ptr<ID3D11Texture2D>		m_source11;
	XUSG::com_ptr<ID3D11Texture2D>		m_result11;

	std::unique_ptr<concurrency::graphics::texture<concurrency::graphics::unorm_4, 2>> m_sourceAMP;
	std::unique_ptr<concurrency::graphics::texture<concurrency::graphics::unorm_4, 2>> m_resultAMP;

	DirectX::XMUINT2					m_imageSize;
};
