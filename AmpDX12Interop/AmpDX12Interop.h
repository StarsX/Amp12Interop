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

#pragma once

#include "DXFramework.h"
#include "StepTimer.h"
#include "Amp12.h"

using namespace DirectX;

// Note that while ComPtr is used to manage the lifetime of resources on the CPU,
// it has no understanding of the lifetime of resources on the GPU. Apps must account
// for the GPU lifetime of resources to avoid destroying objects that may still be
// referenced by the GPU.
// An example of this can be found in the class method: OnDestroy().

class AmpDX12Interop : public DXFramework
{
public:
	AmpDX12Interop(uint32_t width, uint32_t height, std::wstring name);
	virtual ~AmpDX12Interop();

	virtual void OnInit();
	virtual void OnUpdate();
	virtual void OnRender();
	virtual void OnDestroy();

	virtual void OnKeyUp(uint8_t /*key*/);

	virtual void ParseCommandLineArgs(wchar_t* argv[], int argc);

private:
	enum DeviceType : uint8_t
	{
		DEVICE_DISCRETE,
		DEVICE_UMA,
		DEVICE_WARP
	};

	static const uint8_t FrameCount = 3;

	XUSG::SwapChain::uptr			m_swapChain;
	XUSG::CommandAllocator::uptr	m_commandAllocators[FrameCount];
	XUSG::CommandQueue::uptr		m_commandQueue;

	XUSG::Device::uptr			m_device;
	XUSG::RenderTarget::uptr	m_renderTargets[FrameCount];
	XUSG::CommandList::uptr		m_commandList;

	// App resources.
	std::unique_ptr<Amp12> m_amp12;

	// Synchronization objects.
	uint32_t	m_frameIndex;
	HANDLE		m_fenceEvent;
	XUSG::Fence::uptr m_fence;
	uint64_t	m_fenceValues[FrameCount];

	// Application state
	DeviceType	m_deviceType;
	StepTimer	m_timer;
	bool		m_showFPS;
	bool		m_isPaused;

	// User external settings
	std::string m_fileName;
	bool m_useNativeDX11;

	// Screen-shot helpers and state
	XUSG::Buffer::uptr	m_readBuffer;
	uint32_t			m_rowPitch;
	uint8_t				m_screenShot;

	void LoadPipeline(std::vector<XUSG::Resource::uptr>& uploaders, XUSG::Texture::uptr& srcForNative11);
	void LoadAssets();
	void PopulateCommandList();
	void WaitForGpu();
	void MoveToNextFrame();
	void SaveImage(char const* fileName, XUSG::Buffer* pImageBuffer,
		uint32_t w, uint32_t h, uint32_t rowPitch, uint8_t comp = 3);
	double CalculateFrameStats(float* fTimeStep = nullptr);
};
