#include "SdDx12Renderer.h"

#include "backends/Dx12/SdDx12RhiMappings.h"
#include "Effects/SdEffectRegistry.hpp"

#include <d3dcompiler.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <cmath>
#include <cstring>
#include <iterator>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

#ifdef min
#undef min
#endif

#ifdef max
#undef max
#endif

namespace Sodium::Backends
{
	namespace
	{
		constexpr UINT kInvalidRenderTargetIndex = std::numeric_limits<UINT>::max();
		constexpr UINT kResourceDescriptorCapacity = 8192;
		constexpr UINT kSamplerDescriptorCapacity = 1024;
		constexpr UINT kRtvDescriptorCapacity = 1024;
		constexpr UINT kDsvDescriptorCapacity = 128;
		constexpr UINT kFrameContextCount = 3;

		const char* const kVertexShader = SODIUM_STRING(R"(
			cbuffer FrameConstants : register(b0)
			{
				float4x4 ProjectionMatrix;
			};

			struct VSInput
			{
				float2 position : POSITION;
				float2 uv : TEXCOORD0;
				float4 color : COLOR0;
			};

			struct PSInput
			{
				float4 position : SV_POSITION;
				float2 uv : TEXCOORD0;
				float4 color : COLOR0;
			};

			PSInput main(VSInput input)
			{
				PSInput output;
				output.position = mul(ProjectionMatrix, float4(input.position, 0.0f, 1.0f));
				output.uv = input.uv;
				output.color = input.color;
				return output;
			})");

		const char* const kPixelShader = SODIUM_STRING(R"(
			Texture2D texture0 : register(t0);
			SamplerState sampler0 : register(s0);

			struct PSInput
			{
				float4 position : SV_POSITION;
				float2 uv : TEXCOORD0;
				float4 color : COLOR0;
			};

			float4 main(PSInput input) : SV_Target
			{
				return input.color * texture0.Sample(sampler0, input.uv);
			})");

		struct FrameConstants final
		{
			float projection[4][4] = {};
		};

		SdRect IntersectRect(const SdRect& a, const SdRect& b) noexcept
		{
			return {
				std::max(a.min.x, b.min.x),
				std::max(a.min.y, b.min.y),
				std::min(a.max.x, b.max.x),
				std::min(a.max.y, b.max.y)
			};
		}

		UINT64 AlignUp(UINT64 value, UINT64 alignment) noexcept
		{
			return (value + alignment - 1) & ~(alignment - 1);
		}

		D3D12_RESOURCE_BARRIER MakeTransitionBarrier(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) noexcept
		{
			D3D12_RESOURCE_BARRIER barrier = {};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Transition.pResource = resource;
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			barrier.Transition.StateBefore = before;
			barrier.Transition.StateAfter = after;
			return barrier;
		}

		D3D12_HEAP_PROPERTIES MakeHeapProperties(D3D12_HEAP_TYPE type) noexcept
		{
			D3D12_HEAP_PROPERTIES properties = {};
			properties.Type = type;
			properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
			properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
			properties.CreationNodeMask = 1;
			properties.VisibleNodeMask = 1;
			return properties;
		}

		D3D12_RESOURCE_DESC MakeBufferDesc(UINT64 size) noexcept
		{
			D3D12_RESOURCE_DESC desc = {};
			desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			desc.Width = size;
			desc.Height = 1;
			desc.DepthOrArraySize = 1;
			desc.MipLevels = 1;
			desc.SampleDesc.Count = 1;
			desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
			return desc;
		}

		struct Dx12DescriptorAllocation final
		{
			D3D12_CPU_DESCRIPTOR_HANDLE cpu = {};
			D3D12_GPU_DESCRIPTOR_HANDLE gpu = {};
			UINT index = 0;
			UINT count = 0;

			bool IsValid() const noexcept { return count != 0; }
		};

		enum class Dx12DescriptorHeapKind : SdUInt8
		{
			Resource,
			Sampler,
			Rtv,
			Dsv
		};

		struct DeferredDescriptorFree final
		{
			Dx12DescriptorHeapKind kind = Dx12DescriptorHeapKind::Resource;
			Dx12DescriptorAllocation allocation = {};
		};

		struct DeferredCleanup final
		{
			UINT64 fenceValue = 0;
			std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> resources = {};
			std::vector<DeferredDescriptorFree> descriptorFrees = {};
		};

		class Dx12DescriptorHeap final
		{
		private:
			Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap = {};
			D3D12_DESCRIPTOR_HEAP_TYPE type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			UINT descriptorSize = 0;
			UINT capacity = 0;
			UINT nextIndex = 0;
			bool shaderVisible = false;
			std::vector<UINT> freeList = {};

		public:
			bool Initialize(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT descriptorCapacity, bool visible)
			{
				if (!device || descriptorCapacity == 0)
					return false;

				type = heapType;
				capacity = descriptorCapacity;
				shaderVisible = visible;
				nextIndex = 0;
				freeList.clear();

				D3D12_DESCRIPTOR_HEAP_DESC desc = {};
				desc.Type = type;
				desc.NumDescriptors = capacity;
				desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
				if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(heap.ReleaseAndGetAddressOf()))))
					return false;

				descriptorSize = device->GetDescriptorHandleIncrementSize(type);
				return true;
			}

			void Shutdown()
			{
				freeList.clear();
				nextIndex = 0;
				capacity = 0;
				descriptorSize = 0;
				heap.Reset();
			}

			ID3D12DescriptorHeap* GetHeap() const noexcept { return heap.Get(); }

			Dx12DescriptorAllocation Allocate(UINT count = 1)
			{
				if (!heap || count == 0)
					return {};

				UINT index = 0;
				if (count == 1 && !freeList.empty())
				{
					index = freeList.back();
					freeList.pop_back();
				}
				else
				{
					if (nextIndex + count > capacity)
						return {};
					index = nextIndex;
					nextIndex += count;
				}

				Dx12DescriptorAllocation allocation = {};
				allocation.index = index;
				allocation.count = count;
				allocation.cpu = heap->GetCPUDescriptorHandleForHeapStart();
				allocation.cpu.ptr += static_cast<SIZE_T>(index) * descriptorSize;
				if (shaderVisible)
				{
					allocation.gpu = heap->GetGPUDescriptorHandleForHeapStart();
					allocation.gpu.ptr += static_cast<UINT64>(index) * descriptorSize;
				}
				return allocation;
			}

			void Free(const Dx12DescriptorAllocation& allocation)
			{
				if (!allocation.IsValid() || allocation.count != 1)
					return;
				freeList.push_back(allocation.index);
			}
		};

		class SdDx12GpuDevice;

		struct SdDx12GpuDeviceConfig final
		{
			ID3D12Device* device = nullptr;
			ID3D12CommandQueue* commandQueue = nullptr;
		};

		class SdDx12CommandEncoder final : public Rhi::ISdCommandEncoder
		{
		private:
			SdDx12GpuDevice* device = nullptr;
			bool renderPassActive = false;
			std::vector<Rhi::SdTextureHandle> currentColorAttachments = {};

		public:
			void Reset(SdDx12GpuDevice& gpuDevice) noexcept
			{
				device = &gpuDevice;
				renderPassActive = false;
				currentColorAttachments.clear();
			}

			void BeginRenderPass(const Rhi::SdRenderPassDesc& desc) override;
			void EndRenderPass() override;
			void SetPipeline(Rhi::SdPipelineHandle pipeline) override;
			void SetResourceSet(SdUInt32 setIndex, Rhi::SdResourceSetHandle resourceSet) override;
			void SetVertexBuffer(SdUInt32 slot, Rhi::SdBufferHandle buffer, SdUInt32 stride, SdUInt64 offset) override;
			void SetIndexBuffer(Rhi::SdBufferHandle buffer, Rhi::SdIndexFormat format, SdUInt64 offset) override;
			void SetViewport(const Rhi::SdViewport& viewport) override;
			void SetScissorRect(const Rhi::SdRectI& rect) override;
			void Draw(SdUInt32 vertexCount, SdUInt32 instanceCount, SdUInt32 firstVertex, SdUInt32 firstInstance) override;
			void DrawIndexed(SdUInt32 indexCount, SdUInt32 instanceCount, SdUInt32 firstIndex, SdInt32 vertexOffset, SdUInt32 firstInstance) override;
		};

		class SdDx12GpuDevice final : public Rhi::ISdGpuDevice
		{
		private:
			struct TextureEntry final
			{
				Microsoft::WRL::ComPtr<ID3D12Resource> resource = {};
				Rhi::SdTextureDesc desc = {};
				D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
				Dx12DescriptorAllocation srv = {};
				Dx12DescriptorAllocation rtv = {};
				Dx12DescriptorAllocation dsv = {};
				SdUInt32 generation = 0;
				bool occupied = false;
				bool imported = false;
			};

			struct BufferEntry final
			{
				Microsoft::WRL::ComPtr<ID3D12Resource> resource = {};
				Rhi::SdBufferDesc desc = {};
				D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
				void* mappedData = nullptr;
				SdUInt64 allocatedSize = 0;
				SdUInt64 frameStride = 0;
				SdUInt32 generation = 0;
				bool occupied = false;
			};

			struct ShaderEntry final
			{
				std::vector<std::byte> bytecode = {};
				Rhi::SdShaderStage stage = Rhi::SdShaderStage::Vertex;
				SdUInt32 generation = 0;
				bool occupied = false;
			};

			struct SamplerEntry final
			{
				Dx12DescriptorAllocation descriptor = {};
				SdUInt32 generation = 0;
				bool occupied = false;
			};

			struct VertexLayoutEntry final
			{
				std::vector<Rhi::SdVertexAttributeDesc> attributes = {};
				std::vector<std::string> semanticNames = {};
				SdUInt32 generation = 0;
				bool occupied = false;
			};

			struct ResourceBindingRuntime final
			{
				Rhi::SdShaderBindingDesc desc = {};
				UINT rootParameterIndex = 0;
				D3D12_DESCRIPTOR_RANGE_TYPE rangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
			};

			struct UniformBufferBindingRuntime final
			{
				Rhi::SdBufferHandle buffer = {};
				SdUInt64 offset = 0;
				SdUInt64 size = 0;
				std::array<D3D12_GPU_DESCRIPTOR_HANDLE, kFrameContextCount> rootTables = {};
				bool occupied = false;
			};

			struct ResourceSetLayoutEntry final
			{
				Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = {};
				std::vector<ResourceBindingRuntime> bindings = {};
				SdUInt32 generation = 0;
				bool occupied = false;
			};

			struct ResourceSetEntry final
			{
				Rhi::SdResourceSetLayoutHandle layout = {};
				std::vector<D3D12_GPU_DESCRIPTOR_HANDLE> rootTables = {};
				std::vector<Rhi::SdTextureHandle> sampledTextures = {};
				std::vector<UniformBufferBindingRuntime> uniformBuffers = {};
				std::vector<Dx12DescriptorAllocation> ownedResourceDescriptors = {};
				SdUInt32 generation = 0;
				bool occupied = false;
			};

			struct PipelineEntry final
			{
				Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline = {};
				Rhi::SdResourceSetLayoutHandle resourceSetLayout = {};
				Rhi::SdPrimitiveTopology topology = Rhi::SdPrimitiveTopology::TriangleList;
				SdUInt32 generation = 0;
				bool occupied = false;
			};

			struct FrameContext final
			{
				Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator = {};
				UINT64 fenceValue = 0;
			};

			Microsoft::WRL::ComPtr<ID3D12Device> nativeDevice = {};
			Microsoft::WRL::ComPtr<ID3D12CommandQueue> nativeCommandQueue = {};
			std::array<FrameContext, kFrameContextCount> frameContexts = {};
			UINT frameContextIndex = 0;
			UINT activeFrameContextIndex = 0;
			FrameContext* activeFrameContext = nullptr;
			Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList = {};
			Microsoft::WRL::ComPtr<ID3D12Fence> fence = {};
			Microsoft::WRL::ComPtr<ID3D12RootSignature> emptyRootSignature = {};
			HANDLE fenceEvent = nullptr;
			UINT64 nextFenceValue = 1;
			UINT64 lastSubmittedFenceValue = 0;
			bool commandListOpen = false;

			Rhi::SdGpuCaps caps = {};
			Rhi::SdRhiStatus lastStatus = Rhi::SdRhiStatus::Ok;
			Dx12DescriptorHeap resourceDescriptors = {};
			Dx12DescriptorHeap samplerDescriptors = {};
			Dx12DescriptorHeap rtvDescriptors = {};
			Dx12DescriptorHeap dsvDescriptors = {};
			std::vector<TextureEntry> textures = {};
			std::vector<BufferEntry> buffers = {};
			std::vector<ShaderEntry> shaders = {};
			std::vector<SamplerEntry> samplers = {};
			std::vector<VertexLayoutEntry> vertexLayouts = {};
			std::vector<ResourceSetLayoutEntry> resourceSetLayouts = {};
			std::vector<ResourceSetEntry> resourceSets = {};
			std::vector<PipelineEntry> pipelines = {};
			std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> deferredResources = {};
			std::vector<DeferredDescriptorFree> deferredDescriptorFrees = {};
			std::vector<DeferredCleanup> pendingCleanups = {};
			SdDx12CommandEncoder immediateEncoder = {};
			Rhi::SdTextureHandle currentRenderTarget = {};

			template<typename THandle, typename Entry>
			static THandle Allocate(std::vector<Entry>& entries)
			{
				SdUInt32 index = 1;
				for (; index < entries.size(); ++index)
				{
					if (!entries[index].occupied)
						break;
				}
				if (entries.size() <= index)
					entries.resize(index + 1);
				Entry& entry = entries[index];
				entry.generation = entry.generation == 0 ? 1 : entry.generation + 1;
				entry.occupied = true;
				return THandle(index, entry.generation);
			}

			template<typename Entry, typename THandle>
			static Entry* Find(std::vector<Entry>& entries, THandle handle) noexcept
			{
				if (!handle.IsValid() || handle.index >= entries.size())
					return nullptr;
				Entry& entry = entries[handle.index];
				if (!entry.occupied || entry.generation != handle.generation)
					return nullptr;
				return &entry;
			}

			template<typename Entry, typename THandle>
			static const Entry* Find(const std::vector<Entry>& entries, THandle handle) noexcept
			{
				if (!handle.IsValid() || handle.index >= entries.size())
					return nullptr;
				const Entry& entry = entries[handle.index];
				if (!entry.occupied || entry.generation != handle.generation)
					return nullptr;
				return &entry;
			}

			void InitializeCaps()
			{
				caps.maxColorAttachments = 1;
				caps.maxTextureSize = D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;
				caps.uniformBufferOffsetAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
				caps.supportsComputeShader = false;
				caps.supportsStorageTexture = false;
				caps.supportsIndependentBlend = false;
				caps.supportsFramebufferFetch = false;
				caps.supportsMultisampleRenderTarget = true;
				caps.supportedShaderLanguages = static_cast<Rhi::SdShaderLanguageFlags>(Rhi::SdShaderLanguageFlag::Hlsl);
			}

			bool CreateEmptyRootSignature()
			{
				D3D12_ROOT_SIGNATURE_DESC desc = {};
				desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

				Microsoft::WRL::ComPtr<ID3DBlob> signature = {};
				Microsoft::WRL::ComPtr<ID3DBlob> errors = {};
				if (FAILED(::D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, signature.GetAddressOf(), errors.GetAddressOf())))
					return false;
				return SUCCEEDED(nativeDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(emptyRootSignature.ReleaseAndGetAddressOf())));
			}

			void DeferDescriptorFree(Dx12DescriptorHeapKind kind, const Dx12DescriptorAllocation& allocation)
			{
				if (allocation.IsValid())
				{
					if (commandListOpen)
						deferredDescriptorFrees.push_back({ kind, allocation });
					else
						QueueDeferredDescriptorFree(kind, allocation, lastSubmittedFenceValue);
				}
			}

			void FreeDescriptor(Dx12DescriptorHeapKind kind, const Dx12DescriptorAllocation& allocation)
			{
				switch (kind)
				{
				case Dx12DescriptorHeapKind::Sampler:
					samplerDescriptors.Free(allocation);
					break;
				case Dx12DescriptorHeapKind::Rtv:
					rtvDescriptors.Free(allocation);
					break;
				case Dx12DescriptorHeapKind::Dsv:
					dsvDescriptors.Free(allocation);
					break;
				case Dx12DescriptorHeapKind::Resource:
				default:
					resourceDescriptors.Free(allocation);
					break;
				}
			}

			void DeferResource(Microsoft::WRL::ComPtr<ID3D12Resource>& resource)
			{
				if (resource)
				{
					if (commandListOpen)
						deferredResources.push_back(std::move(resource));
					else
						QueueDeferredResource(resource, lastSubmittedFenceValue);
				}
				resource.Reset();
			}

			bool IsFenceComplete(UINT64 value) const noexcept
			{
				return value == 0 || !fence || fence->GetCompletedValue() >= value;
			}

			void ReleaseDeferredCleanup(DeferredCleanup& cleanup)
			{
				for (const DeferredDescriptorFree& descriptor : cleanup.descriptorFrees)
					FreeDescriptor(descriptor.kind, descriptor.allocation);
				cleanup.descriptorFrees.clear();
				cleanup.resources.clear();
			}

			void QueueDeferredDescriptorFree(Dx12DescriptorHeapKind kind, const Dx12DescriptorAllocation& allocation, UINT64 cleanupFenceValue)
			{
				if (!allocation.IsValid())
					return;
				if (IsFenceComplete(cleanupFenceValue))
				{
					FreeDescriptor(kind, allocation);
					return;
				}
				DeferredCleanup cleanup = {};
				cleanup.fenceValue = cleanupFenceValue;
				cleanup.descriptorFrees.push_back({ kind, allocation });
				pendingCleanups.push_back(std::move(cleanup));
			}

			void QueueDeferredResource(Microsoft::WRL::ComPtr<ID3D12Resource>& resource, UINT64 cleanupFenceValue)
			{
				if (!resource)
					return;
				if (IsFenceComplete(cleanupFenceValue))
					return;
				DeferredCleanup cleanup = {};
				cleanup.fenceValue = cleanupFenceValue;
				cleanup.resources.push_back(std::move(resource));
				pendingCleanups.push_back(std::move(cleanup));
			}

			void FlushDeferred(UINT64 cleanupFenceValue)
			{
				if (deferredResources.empty() && deferredDescriptorFrees.empty())
					return;
				if (IsFenceComplete(cleanupFenceValue))
				{
					for (const DeferredDescriptorFree& descriptor : deferredDescriptorFrees)
						FreeDescriptor(descriptor.kind, descriptor.allocation);
					deferredDescriptorFrees.clear();
					deferredResources.clear();
					return;
				}

				DeferredCleanup cleanup = {};
				cleanup.fenceValue = cleanupFenceValue;
				cleanup.resources = std::move(deferredResources);
				cleanup.descriptorFrees = std::move(deferredDescriptorFrees);
				pendingCleanups.push_back(std::move(cleanup));
				deferredResources.clear();
				deferredDescriptorFrees.clear();
			}

			void CollectCompletedDeferred()
			{
				auto write = pendingCleanups.begin();
				for (auto read = pendingCleanups.begin(); read != pendingCleanups.end(); ++read)
				{
					if (IsFenceComplete(read->fenceValue))
					{
						ReleaseDeferredCleanup(*read);
						continue;
					}

					if (write != read)
						*write = std::move(*read);
					++write;
				}
				pendingCleanups.erase(write, pendingCleanups.end());
			}

			UINT GetActiveFrameResourceIndex() const noexcept
			{
				return commandListOpen ? activeFrameContextIndex : frameContextIndex;
			}

			static SdUInt64 GetBufferFrameOffset(const BufferEntry& entry, UINT frameIndex) noexcept
			{
				return entry.frameStride > 0 ? static_cast<SdUInt64>(frameIndex) * entry.frameStride : 0;
			}

			SdUInt64 GetCurrentBufferFrameOffset(const BufferEntry& entry) const noexcept
			{
				return GetBufferFrameOffset(entry, GetActiveFrameResourceIndex());
			}

			void CreateConstantBufferView(const BufferEntry& entry, SdUInt64 offset, SdUInt64 size, UINT frameIndex, D3D12_CPU_DESCRIPTOR_HANDLE destination)
			{
				if (!nativeDevice || !entry.resource || offset >= entry.desc.size)
					return;

				const SdUInt64 availableSize = entry.desc.size - offset;
				const SdUInt64 requestedSize = size == 0 ? availableSize : std::min(size, availableSize);
				if (requestedSize == 0)
					return;

				D3D12_CONSTANT_BUFFER_VIEW_DESC cbv = {};
				cbv.BufferLocation = entry.resource->GetGPUVirtualAddress() + GetBufferFrameOffset(entry, frameIndex) + offset;
				cbv.SizeInBytes = static_cast<UINT>(AlignUp(requestedSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));
				nativeDevice->CreateConstantBufferView(&cbv, destination);
			}

			void InitializeCpuVisibleBufferData(BufferEntry& entry, const void* initialData, SdUInt64 size)
			{
				if (!entry.mappedData || !initialData || size == 0)
					return;

				const SdUInt64 copySize = std::min(size, entry.desc.size);
				for (UINT frameIndex = 0; frameIndex < kFrameContextCount; ++frameIndex)
				{
					std::memcpy(
						static_cast<std::byte*>(entry.mappedData) + GetBufferFrameOffset(entry, frameIndex),
						initialData,
						static_cast<SdSize>(copySize));
				}
			}

			void TransitionTexture(TextureEntry& entry, D3D12_RESOURCE_STATES newState)
			{
				if (!commandList || !entry.resource || entry.state == newState)
					return;

				const D3D12_RESOURCE_BARRIER barrier = MakeTransitionBarrier(entry.resource.Get(), entry.state, newState);
				commandList->ResourceBarrier(1, &barrier);
				entry.state = newState;
			}

			bool CreateTextureDescriptors(TextureEntry& entry)
			{
				const DXGI_FORMAT format = MapTextureFormat(entry.desc.format);
				if (format == DXGI_FORMAT_UNKNOWN)
					return false;

				if (Rhi::SdHasFlag(entry.desc.usage, Rhi::SdTextureUsage::ShaderRead))
				{
					entry.srv = resourceDescriptors.Allocate();
					if (!entry.srv.IsValid())
						return false;

					D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
					srvDesc.Format = format;
					srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
					srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
					srvDesc.Texture2D.MipLevels = entry.desc.mipLevels;
					nativeDevice->CreateShaderResourceView(entry.resource.Get(), &srvDesc, entry.srv.cpu);
				}

				if (Rhi::SdHasFlag(entry.desc.usage, Rhi::SdTextureUsage::RenderTarget))
				{
					entry.rtv = rtvDescriptors.Allocate();
					if (!entry.rtv.IsValid())
						return false;
					nativeDevice->CreateRenderTargetView(entry.resource.Get(), nullptr, entry.rtv.cpu);
				}

				if (Rhi::SdHasFlag(entry.desc.usage, Rhi::SdTextureUsage::DepthStencil))
				{
					entry.dsv = dsvDescriptors.Allocate();
					if (!entry.dsv.IsValid())
						return false;
					nativeDevice->CreateDepthStencilView(entry.resource.Get(), nullptr, entry.dsv.cpu);
				}

				return true;
			}

			void ReleaseTextureEntry(TextureEntry& entry)
			{
				DeferDescriptorFree(Dx12DescriptorHeapKind::Resource, entry.srv);
				DeferDescriptorFree(Dx12DescriptorHeapKind::Rtv, entry.rtv);
				DeferDescriptorFree(Dx12DescriptorHeapKind::Dsv, entry.dsv);
				DeferResource(entry.resource);
				entry = {};
			}

			void ReleaseBufferEntry(BufferEntry& entry)
			{
				if (entry.resource && entry.mappedData)
					entry.resource->Unmap(0, nullptr);
				entry.mappedData = nullptr;
				DeferResource(entry.resource);
				entry = {};
			}

			void ReleaseResourceSetEntry(ResourceSetEntry& entry)
			{
				for (const Dx12DescriptorAllocation& allocation : entry.ownedResourceDescriptors)
					DeferDescriptorFree(Dx12DescriptorHeapKind::Resource, allocation);
				entry = {};
			}

			static std::string NormalizeDx12ShaderTarget(Rhi::SdShaderStage stage, SdUtf8StringView requested)
			{
				const char* defaultProfile = nullptr;
				switch (stage)
				{
				case Rhi::SdShaderStage::Vertex:
					defaultProfile = SODIUM_STRING("vs_5_0");
					break;
				case Rhi::SdShaderStage::Pixel:
					defaultProfile = SODIUM_STRING("ps_5_0");
					break;
				case Rhi::SdShaderStage::Compute:
					defaultProfile = SODIUM_STRING("cs_5_0");
					break;
				default:
					return {};
				}

				std::string target = requested.empty() ? std::string(defaultProfile) : std::string(requested);
				if (stage == Rhi::SdShaderStage::Vertex && target.rfind(SODIUM_STRING("vs_4"), 0) == 0)
					target = SODIUM_STRING("vs_5_0");
				else if (stage == Rhi::SdShaderStage::Pixel && target.rfind(SODIUM_STRING("ps_4"), 0) == 0)
					target = SODIUM_STRING("ps_5_0");
				return target;
			}

		public:
			using Config = SdDx12GpuDeviceConfig;

			~SdDx12GpuDevice() override { Shutdown(); }

			bool Initialize(const Config& config)
			{
				if (!config.device || !config.commandQueue)
				{
					lastStatus = Rhi::SdRhiStatus::InvalidArgument;
					return false;
				}

				nativeDevice = config.device;
				nativeCommandQueue = config.commandQueue;
				InitializeCaps();

				if (!resourceDescriptors.Initialize(nativeDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, kResourceDescriptorCapacity, true))
					return false;
				if (!samplerDescriptors.Initialize(nativeDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, kSamplerDescriptorCapacity, true))
					return false;
				if (!rtvDescriptors.Initialize(nativeDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_RTV, kRtvDescriptorCapacity, false))
					return false;
				if (!dsvDescriptors.Initialize(nativeDevice.Get(), D3D12_DESCRIPTOR_HEAP_TYPE_DSV, kDsvDescriptorCapacity, false))
					return false;
				for (FrameContext& frameContext : frameContexts)
				{
					if (FAILED(nativeDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(frameContext.commandAllocator.ReleaseAndGetAddressOf()))))
						return false;
					frameContext.fenceValue = 0;
				}
				if (FAILED(nativeDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, frameContexts[0].commandAllocator.Get(), nullptr, IID_PPV_ARGS(commandList.ReleaseAndGetAddressOf()))))
					return false;
				commandList->Close();
				if (FAILED(nativeDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence.ReleaseAndGetAddressOf()))))
					return false;
				fenceEvent = ::CreateEventW(nullptr, FALSE, FALSE, nullptr);
				if (!fenceEvent)
					return false;
				if (!CreateEmptyRootSignature())
					return false;

				immediateEncoder.Reset(*this);
				lastStatus = Rhi::SdRhiStatus::Ok;
				return true;
			}

			void Shutdown()
			{
				WaitForGpu();
				for (TextureEntry& entry : textures)
				{
					if (entry.occupied)
						ReleaseTextureEntry(entry);
				}
				for (BufferEntry& entry : buffers)
				{
					if (entry.occupied)
						ReleaseBufferEntry(entry);
				}
				for (SamplerEntry& entry : samplers)
				{
					if (entry.occupied)
						DeferDescriptorFree(Dx12DescriptorHeapKind::Sampler, entry.descriptor);
				}
				for (ResourceSetEntry& entry : resourceSets)
				{
					if (entry.occupied)
						ReleaseResourceSetEntry(entry);
				}
				CollectCompletedDeferred();
				pipelines.clear();
				resourceSets.clear();
				resourceSetLayouts.clear();
				vertexLayouts.clear();
				samplers.clear();
				shaders.clear();
				buffers.clear();
				textures.clear();
				resourceDescriptors.Shutdown();
				samplerDescriptors.Shutdown();
				rtvDescriptors.Shutdown();
				dsvDescriptors.Shutdown();
				emptyRootSignature.Reset();
				commandList.Reset();
				for (FrameContext& frameContext : frameContexts)
					frameContext = {};
				frameContextIndex = 0;
				activeFrameContextIndex = 0;
				activeFrameContext = nullptr;
				fence.Reset();
				nativeCommandQueue.Reset();
				nativeDevice.Reset();
				caps = {};
				lastStatus = Rhi::SdRhiStatus::Ok;
				currentRenderTarget = {};
				commandListOpen = false;
				nextFenceValue = 1;
				lastSubmittedFenceValue = 0;
				deferredResources.clear();
				deferredDescriptorFrees.clear();
				pendingCleanups.clear();
				if (fenceEvent)
				{
					::CloseHandle(fenceEvent);
					fenceEvent = nullptr;
				}
			}

			bool IsInitialized() const noexcept { return nativeDevice.Get() && nativeCommandQueue.Get() && commandList.Get(); }
			const Rhi::SdGpuCaps& GetCaps() const noexcept override { return caps; }
			Rhi::SdRhiStatus GetLastStatus() const noexcept override { return lastStatus; }
			void WaitIdle() override { WaitForGpu(); }
			Rhi::ISdCommandEncoder& GetImmediateEncoder() noexcept { return immediateEncoder; }
			ID3D12GraphicsCommandList* GetCommandList() const noexcept { return commandList.Get(); }
			ID3D12DescriptorHeap* GetResourceHeap() const noexcept { return resourceDescriptors.GetHeap(); }
			ID3D12DescriptorHeap* GetSamplerHeap() const noexcept { return samplerDescriptors.GetHeap(); }

			void WaitForGpu()
			{
				if (!nativeCommandQueue || !fence || !fenceEvent)
					return;

				const UINT64 signalValue = nextFenceValue++;
				if (FAILED(nativeCommandQueue->Signal(fence.Get(), signalValue)))
					return;
				lastSubmittedFenceValue = signalValue;
				if (fence->GetCompletedValue() < signalValue)
				{
					if (SUCCEEDED(fence->SetEventOnCompletion(signalValue, fenceEvent)))
						::WaitForSingleObject(fenceEvent, INFINITE);
				}
				FlushDeferred(signalValue);
				CollectCompletedDeferred();
			}

			bool BeginFrame()
			{
				if (!IsInitialized() || commandListOpen)
					return false;

				CollectCompletedDeferred();
				FrameContext& frameContext = frameContexts[frameContextIndex];
				if (frameContext.fenceValue != 0 && fence->GetCompletedValue() < frameContext.fenceValue)
				{
					if (FAILED(fence->SetEventOnCompletion(frameContext.fenceValue, fenceEvent)))
						return false;
					::WaitForSingleObject(fenceEvent, INFINITE);
					CollectCompletedDeferred();
				}

				if (FAILED(frameContext.commandAllocator->Reset()))
					return false;
				if (FAILED(commandList->Reset(frameContext.commandAllocator.Get(), nullptr)))
					return false;
				ID3D12DescriptorHeap* heaps[] = { resourceDescriptors.GetHeap(), samplerDescriptors.GetHeap() };
				commandList->SetDescriptorHeaps(static_cast<UINT>(std::size(heaps)), heaps);
				commandListOpen = true;
				activeFrameContext = &frameContext;
				activeFrameContextIndex = frameContextIndex;
				currentRenderTarget = {};
				immediateEncoder.Reset(*this);
				return true;
			}

			void EndFrame(Rhi::SdTextureHandle frameRenderTarget)
			{
				if (!commandListOpen)
					return;
				if (TextureEntry* frameTarget = FindTexture(frameRenderTarget))
					TransitionTexture(*frameTarget, D3D12_RESOURCE_STATE_PRESENT);
				commandList->Close();
				ID3D12CommandList* lists[] = { commandList.Get() };
				nativeCommandQueue->ExecuteCommandLists(static_cast<UINT>(std::size(lists)), lists);
				commandListOpen = false;
				const UINT64 signalValue = nextFenceValue++;
				if (SUCCEEDED(nativeCommandQueue->Signal(fence.Get(), signalValue)))
				{
					if (activeFrameContext)
						activeFrameContext->fenceValue = signalValue;
					lastSubmittedFenceValue = signalValue;
					FlushDeferred(signalValue);
					frameContextIndex = (frameContextIndex + 1) % kFrameContextCount;
					CollectCompletedDeferred();
				}
				activeFrameContext = nullptr;
			}

			Rhi::SdTextureHandle ImportTexture2D(ID3D12Resource* texture, const Rhi::SdTextureDesc& desc, D3D12_RESOURCE_STATES initialState)
			{
				if (!nativeDevice || !texture)
					return {};

				Rhi::SdTextureHandle handle = Allocate<Rhi::SdTextureHandle>(textures);
				TextureEntry& entry = textures[handle.index];
				entry.resource = texture;
				entry.desc = desc;
				entry.state = initialState;
				entry.imported = true;
				if (!CreateTextureDescriptors(entry))
				{
					ReleaseTextureEntry(entry);
					return {};
				}
				return handle;
			}

			Rhi::SdTextureHandle CreateTexture(const Rhi::SdTextureDesc& desc) override
			{
				if (!nativeDevice || desc.width == 0 || desc.height == 0)
				{
					lastStatus = Rhi::SdRhiStatus::InvalidArgument;
					return {};
				}

				const DXGI_FORMAT format = MapTextureFormat(desc.format);
				if (format == DXGI_FORMAT_UNKNOWN)
				{
					lastStatus = Rhi::SdRhiStatus::Unsupported;
					return {};
				}

				D3D12_RESOURCE_DESC nativeDesc = {};
				nativeDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
				nativeDesc.Width = desc.width;
				nativeDesc.Height = desc.height;
				nativeDesc.DepthOrArraySize = desc.arraySize;
				nativeDesc.MipLevels = desc.mipLevels;
				nativeDesc.Format = format;
				nativeDesc.SampleDesc.Count = desc.sampleCount;
				nativeDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
				if (Rhi::SdHasFlag(desc.usage, Rhi::SdTextureUsage::RenderTarget))
					nativeDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
				if (Rhi::SdHasFlag(desc.usage, Rhi::SdTextureUsage::DepthStencil))
					nativeDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

				D3D12_RESOURCE_STATES initialState = Rhi::SdHasFlag(desc.usage, Rhi::SdTextureUsage::ShaderRead)
					? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
					: D3D12_RESOURCE_STATE_COMMON;
				D3D12_CLEAR_VALUE clearValue = {};
				D3D12_CLEAR_VALUE* clearValuePtr = nullptr;
				if (Rhi::SdHasFlag(desc.usage, Rhi::SdTextureUsage::RenderTarget))
				{
					clearValue.Format = format;
					clearValuePtr = &clearValue;
				}
				else if (Rhi::SdHasFlag(desc.usage, Rhi::SdTextureUsage::DepthStencil))
				{
					clearValue.Format = format;
					clearValue.DepthStencil.Depth = 1.0f;
					clearValuePtr = &clearValue;
				}

				Microsoft::WRL::ComPtr<ID3D12Resource> resource = {};
				const D3D12_HEAP_PROPERTIES heapProperties = MakeHeapProperties(D3D12_HEAP_TYPE_DEFAULT);
				if (FAILED(nativeDevice->CreateCommittedResource(
					&heapProperties,
					D3D12_HEAP_FLAG_NONE,
					&nativeDesc,
					initialState,
					clearValuePtr,
					IID_PPV_ARGS(resource.ReleaseAndGetAddressOf()))))
				{
					lastStatus = Rhi::SdRhiStatus::BackendError;
					return {};
				}

				Rhi::SdTextureHandle handle = Allocate<Rhi::SdTextureHandle>(textures);
				TextureEntry& entry = textures[handle.index];
				entry.resource = resource;
				entry.desc = desc;
				entry.state = initialState;
				if (!CreateTextureDescriptors(entry))
				{
					ReleaseTextureEntry(entry);
					lastStatus = Rhi::SdRhiStatus::BackendError;
					return {};
				}
				lastStatus = Rhi::SdRhiStatus::Ok;
				return handle;
			}

			void DestroyTexture(Rhi::SdTextureHandle texture) override
			{
				TextureEntry* entry = Find(textures, texture);
				if (!entry)
					return;
				const SdUInt32 nextGeneration = entry->generation + 1;
				ReleaseTextureEntry(*entry);
				entry->generation = nextGeneration;
			}

			bool UpdateTexture(Rhi::SdTextureHandle texture, const void* pixels, SdUInt32 rowPitch) override
			{
				if (!nativeDevice || !pixels || rowPitch == 0)
					return false;

				TextureEntry* entry = Find(textures, texture);
				if (!entry || !entry->resource)
					return false;

				bool openedUploadCommands = false;
				if (!commandListOpen)
				{
					if (!BeginFrame())
						return false;
					openedUploadCommands = true;
				}

				const DXGI_FORMAT format = MapTextureFormat(entry->desc.format);
				D3D12_RESOURCE_DESC resourceDesc = entry->resource->GetDesc();
				UINT64 uploadSize = 0;
				D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
				UINT rowCount = 0;
				UINT64 rowSize = 0;
				nativeDevice->GetCopyableFootprints(&resourceDesc, 0, 1, 0, &footprint, &rowCount, &rowSize, &uploadSize);

				Microsoft::WRL::ComPtr<ID3D12Resource> upload = {};
				const D3D12_HEAP_PROPERTIES heapProperties = MakeHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
				const D3D12_RESOURCE_DESC uploadDesc = MakeBufferDesc(uploadSize);
				if (FAILED(nativeDevice->CreateCommittedResource(
					&heapProperties,
					D3D12_HEAP_FLAG_NONE,
					&uploadDesc,
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(upload.ReleaseAndGetAddressOf()))))
				{
					if (openedUploadCommands)
						EndFrame({});
					return false;
				}

				void* mapped = nullptr;
				if (FAILED(upload->Map(0, nullptr, &mapped)))
				{
					if (openedUploadCommands)
						EndFrame({});
					return false;
				}
				const std::byte* source = static_cast<const std::byte*>(pixels);
				std::byte* destination = static_cast<std::byte*>(mapped) + footprint.Offset;
				const UINT sourceRowBytes = std::min(rowPitch, static_cast<SdUInt32>(rowSize));
				for (UINT row = 0; row < rowCount; ++row)
					std::memcpy(destination + static_cast<SdSize>(row) * footprint.Footprint.RowPitch, source + static_cast<SdSize>(row) * rowPitch, sourceRowBytes);
				upload->Unmap(0, nullptr);

				const D3D12_RESOURCE_STATES oldState = entry->state;
				TransitionTexture(*entry, D3D12_RESOURCE_STATE_COPY_DEST);
				D3D12_TEXTURE_COPY_LOCATION dst = {};
				dst.pResource = entry->resource.Get();
				dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				dst.SubresourceIndex = 0;
				D3D12_TEXTURE_COPY_LOCATION src = {};
				src.pResource = upload.Get();
				src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
				src.PlacedFootprint = footprint;
				src.PlacedFootprint.Footprint.Format = format;
				commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
				TransitionTexture(*entry, oldState == D3D12_RESOURCE_STATE_COMMON ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : oldState);
				deferredResources.push_back(std::move(upload));
				if (openedUploadCommands)
					EndFrame({});
				return true;
			}

			bool CopyCurrentRenderTargetToTexture(Rhi::SdTextureHandle destination, const Rhi::SdRectI& sourceRect) override
			{
				if (!commandListOpen || sourceRect.Width() == 0 || sourceRect.Height() == 0)
					return false;

				TextureEntry* source = Find(textures, currentRenderTarget);
				TextureEntry* dest = Find(textures, destination);
				if (!source || !source->resource || !dest || !dest->resource)
					return false;

				const D3D12_RESOURCE_STATES sourceOldState = source->state;
				const D3D12_RESOURCE_STATES destOldState = dest->state;
				commandList->OMSetRenderTargets(0, nullptr, FALSE, nullptr);
				TransitionTexture(*source, D3D12_RESOURCE_STATE_COPY_SOURCE);
				TransitionTexture(*dest, D3D12_RESOURCE_STATE_COPY_DEST);

				D3D12_TEXTURE_COPY_LOCATION dst = {};
				dst.pResource = dest->resource.Get();
				dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				dst.SubresourceIndex = 0;
				D3D12_TEXTURE_COPY_LOCATION src = {};
				src.pResource = source->resource.Get();
				src.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				src.SubresourceIndex = 0;
				D3D12_BOX box = {};
				box.left = static_cast<UINT>(std::max(0, sourceRect.left));
				box.top = static_cast<UINT>(std::max(0, sourceRect.top));
				box.right = static_cast<UINT>(std::max(sourceRect.left, sourceRect.right));
				box.bottom = static_cast<UINT>(std::max(sourceRect.top, sourceRect.bottom));
				box.front = 0;
				box.back = 1;
				commandList->CopyTextureRegion(&dst, 0, 0, 0, &src, &box);

				TransitionTexture(*dest, destOldState == D3D12_RESOURCE_STATE_COMMON ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE : destOldState);
				TransitionTexture(*source, sourceOldState);
				return true;
			}

			Rhi::SdBufferHandle CreateBuffer(const Rhi::SdBufferDesc& desc, const void* initialData) override
			{
				if (!nativeDevice || desc.size == 0)
					return {};

				const bool cpuVisible = desc.memoryUsage == Rhi::SdMemoryUsage::CpuToGpu;
				const UINT64 requestedSize = Rhi::SdHasFlag(desc.usage, Rhi::SdBufferUsage::Uniform)
					? AlignUp(desc.size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
					: desc.size;
				const UINT64 frameStride = cpuVisible
					? AlignUp(requestedSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT)
					: 0;
				const UINT64 allocatedSize = cpuVisible ? frameStride * kFrameContextCount : requestedSize;
				const D3D12_HEAP_PROPERTIES heapProperties = MakeHeapProperties(cpuVisible ? D3D12_HEAP_TYPE_UPLOAD : D3D12_HEAP_TYPE_DEFAULT);
				const D3D12_RESOURCE_DESC resourceDesc = MakeBufferDesc(allocatedSize);
				const D3D12_RESOURCE_STATES initialState = cpuVisible ? D3D12_RESOURCE_STATE_GENERIC_READ : D3D12_RESOURCE_STATE_COMMON;

				Microsoft::WRL::ComPtr<ID3D12Resource> resource = {};
				if (FAILED(nativeDevice->CreateCommittedResource(
					&heapProperties,
					D3D12_HEAP_FLAG_NONE,
					&resourceDesc,
					initialState,
					nullptr,
					IID_PPV_ARGS(resource.ReleaseAndGetAddressOf()))))
				{
					return {};
				}

				Rhi::SdBufferHandle handle = Allocate<Rhi::SdBufferHandle>(buffers);
				BufferEntry& entry = buffers[handle.index];
				entry.resource = resource;
				entry.desc = desc;
				entry.state = initialState;
				entry.allocatedSize = allocatedSize;
				entry.frameStride = frameStride;
				if (cpuVisible)
				{
					if (FAILED(entry.resource->Map(0, nullptr, &entry.mappedData)))
					{
						ReleaseBufferEntry(entry);
						return {};
					}
				}
				if (initialData && cpuVisible)
				{
					InitializeCpuVisibleBufferData(entry, initialData, desc.size);
				}
				else if (initialData && !UpdateBuffer(handle, initialData, desc.size, 0))
				{
					ReleaseBufferEntry(entry);
					return {};
				}
				return handle;
			}

			void DestroyBuffer(Rhi::SdBufferHandle buffer) override
			{
				BufferEntry* entry = Find(buffers, buffer);
				if (!entry)
					return;
				const SdUInt32 nextGeneration = entry->generation + 1;
				ReleaseBufferEntry(*entry);
				entry->generation = nextGeneration;
			}

			bool UpdateBuffer(Rhi::SdBufferHandle buffer, const void* data, SdUInt64 size, SdUInt64 offset) override
			{
				if (!data || size == 0)
					return false;

				BufferEntry* entry = Find(buffers, buffer);
				if (!entry || !entry->resource || offset > entry->desc.size || size > entry->desc.size - offset)
					return false;

				if (entry->mappedData)
				{
					std::memcpy(static_cast<std::byte*>(entry->mappedData) + GetCurrentBufferFrameOffset(*entry) + offset, data, static_cast<SdSize>(size));
					return true;
				}

				bool openedUploadCommands = false;
				if (!commandListOpen)
				{
					if (!BeginFrame())
						return false;
					openedUploadCommands = true;
				}

				Microsoft::WRL::ComPtr<ID3D12Resource> upload = {};
				const D3D12_HEAP_PROPERTIES heapProperties = MakeHeapProperties(D3D12_HEAP_TYPE_UPLOAD);
				const D3D12_RESOURCE_DESC uploadDesc = MakeBufferDesc(size);
				if (FAILED(nativeDevice->CreateCommittedResource(
					&heapProperties,
					D3D12_HEAP_FLAG_NONE,
					&uploadDesc,
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(upload.ReleaseAndGetAddressOf()))))
				{
					if (openedUploadCommands)
						EndFrame({});
					return false;
				}

				void* mapped = nullptr;
				if (FAILED(upload->Map(0, nullptr, &mapped)))
				{
					if (openedUploadCommands)
						EndFrame({});
					return false;
				}
				std::memcpy(mapped, data, static_cast<SdSize>(size));
				upload->Unmap(0, nullptr);

				const D3D12_RESOURCE_STATES oldState = entry->state;
				if (oldState != D3D12_RESOURCE_STATE_COPY_DEST)
				{
					const D3D12_RESOURCE_BARRIER barrier = MakeTransitionBarrier(entry->resource.Get(), oldState, D3D12_RESOURCE_STATE_COPY_DEST);
					commandList->ResourceBarrier(1, &barrier);
					entry->state = D3D12_RESOURCE_STATE_COPY_DEST;
				}
				commandList->CopyBufferRegion(entry->resource.Get(), offset, upload.Get(), 0, size);
				if (oldState != D3D12_RESOURCE_STATE_COPY_DEST)
				{
					const D3D12_RESOURCE_BARRIER barrier = MakeTransitionBarrier(entry->resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, oldState);
					commandList->ResourceBarrier(1, &barrier);
					entry->state = oldState;
				}
				deferredResources.push_back(std::move(upload));
				if (openedUploadCommands)
					EndFrame({});
				return true;
			}

			Rhi::SdShaderHandle CreateShader(const Rhi::SdShaderDesc& desc) override
			{
				if (desc.bytecode.empty())
					return {};

				Rhi::SdShaderHandle handle = Allocate<Rhi::SdShaderHandle>(shaders);
				ShaderEntry& entry = shaders[handle.index];
				entry.bytecode.assign(desc.bytecode.begin(), desc.bytecode.end());
				entry.stage = desc.stage;
				return handle;
			}

			Rhi::SdShaderHandle CreateShaderFromSource(const Rhi::SdShaderSourceDesc& desc) override
			{
				if (desc.source.empty() || desc.language != Rhi::SdShaderLanguage::Hlsl)
					return {};

				const std::string entryPoint = desc.entryPoint.empty() ? std::string(SODIUM_STRING("main")) : std::string(desc.entryPoint);
				const std::string targetProfile = NormalizeDx12ShaderTarget(desc.stage, desc.targetProfile);
				if (targetProfile.empty())
					return {};

				Microsoft::WRL::ComPtr<ID3DBlob> bytecode = {};
				Microsoft::WRL::ComPtr<ID3DBlob> errors = {};
				const UINT flags =
#if defined(_DEBUG)
					D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
					D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif
				if (FAILED(::D3DCompile(
					desc.source.data(),
					desc.source.size(),
					nullptr,
					nullptr,
					nullptr,
					entryPoint.c_str(),
					targetProfile.c_str(),
					flags,
					0,
					bytecode.GetAddressOf(),
					errors.GetAddressOf())))
				{
					return {};
				}

				return CreateShader({
					desc.stage,
					SdSpan<const Rhi::SdByte>(static_cast<const Rhi::SdByte*>(bytecode->GetBufferPointer()), bytecode->GetBufferSize()),
					entryPoint,
					desc.debugName
					});
			}

			void DestroyShader(Rhi::SdShaderHandle shader) override
			{
				ShaderEntry* entry = Find(shaders, shader);
				if (!entry)
					return;
				*entry = {};
				entry->generation = shader.generation + 1;
			}

			Rhi::SdSamplerHandle CreateSampler(const Rhi::SdSamplerDesc& desc) override
			{
				if (!nativeDevice)
					return {};

				Dx12DescriptorAllocation descriptor = samplerDescriptors.Allocate();
				if (!descriptor.IsValid())
					return {};

				D3D12_SAMPLER_DESC samplerDesc = {};
				samplerDesc.Filter = MapFilter(desc);
				samplerDesc.AddressU = MapAddress(desc.addressU);
				samplerDesc.AddressV = MapAddress(desc.addressV);
				samplerDesc.AddressW = MapAddress(desc.addressW);
				samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
				nativeDevice->CreateSampler(&samplerDesc, descriptor.cpu);

				Rhi::SdSamplerHandle handle = Allocate<Rhi::SdSamplerHandle>(samplers);
				samplers[handle.index].descriptor = descriptor;
				return handle;
			}

			void DestroySampler(Rhi::SdSamplerHandle sampler) override
			{
				SamplerEntry* entry = Find(samplers, sampler);
				if (!entry)
					return;
				DeferDescriptorFree(Dx12DescriptorHeapKind::Sampler, entry->descriptor);
				const SdUInt32 nextGeneration = entry->generation + 1;
				*entry = {};
				entry->generation = nextGeneration;
			}

			Rhi::SdVertexLayoutHandle CreateVertexLayout(const Rhi::SdVertexLayoutDesc& desc) override
			{
				Rhi::SdVertexLayoutHandle handle = Allocate<Rhi::SdVertexLayoutHandle>(vertexLayouts);
				VertexLayoutEntry& entry = vertexLayouts[handle.index];
				entry.attributes.assign(desc.attributes.begin(), desc.attributes.end());
				entry.semanticNames.clear();
				entry.semanticNames.reserve(entry.attributes.size());
				for (Rhi::SdVertexAttributeDesc& attribute : entry.attributes)
				{
					entry.semanticNames.emplace_back(attribute.semanticName);
					attribute.semanticName = entry.semanticNames.back();
				}
				return handle;
			}

			void DestroyVertexLayout(Rhi::SdVertexLayoutHandle layout) override
			{
				VertexLayoutEntry* entry = Find(vertexLayouts, layout);
				if (!entry)
					return;
				*entry = {};
				entry->generation = layout.generation + 1;
			}

			Rhi::SdResourceSetLayoutHandle CreateResourceSetLayout(const Rhi::SdResourceSetLayoutDesc& desc) override
			{
				std::vector<D3D12_DESCRIPTOR_RANGE> ranges(desc.bindings.size());
				std::vector<D3D12_ROOT_PARAMETER> parameters(desc.bindings.size());
				std::vector<ResourceBindingRuntime> runtimeBindings(desc.bindings.size());
				for (SdSize i = 0; i < desc.bindings.size(); ++i)
				{
					const Rhi::SdShaderBindingDesc& binding = desc.bindings[i];
					const D3D12_DESCRIPTOR_RANGE_TYPE rangeType = MapDescriptorRangeType(binding.type);
					ranges[i].RangeType = rangeType;
					ranges[i].NumDescriptors = 1;
					ranges[i].BaseShaderRegister = binding.binding;
					ranges[i].RegisterSpace = binding.set;
					ranges[i].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

					parameters[i].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
					parameters[i].DescriptorTable.NumDescriptorRanges = 1;
					parameters[i].DescriptorTable.pDescriptorRanges = &ranges[i];
					parameters[i].ShaderVisibility = MapShaderVisibility(binding.stages);

					runtimeBindings[i].desc = binding;
					runtimeBindings[i].rootParameterIndex = static_cast<UINT>(i);
					runtimeBindings[i].rangeType = rangeType;
				}

				D3D12_ROOT_SIGNATURE_DESC nativeDesc = {};
				nativeDesc.NumParameters = static_cast<UINT>(parameters.size());
				nativeDesc.pParameters = parameters.empty() ? nullptr : parameters.data();
				nativeDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

				Microsoft::WRL::ComPtr<ID3DBlob> signature = {};
				Microsoft::WRL::ComPtr<ID3DBlob> errors = {};
				if (FAILED(::D3D12SerializeRootSignature(&nativeDesc, D3D_ROOT_SIGNATURE_VERSION_1, signature.GetAddressOf(), errors.GetAddressOf())))
					return {};

				Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature = {};
				if (FAILED(nativeDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(rootSignature.ReleaseAndGetAddressOf()))))
					return {};

				Rhi::SdResourceSetLayoutHandle handle = Allocate<Rhi::SdResourceSetLayoutHandle>(resourceSetLayouts);
				ResourceSetLayoutEntry& entry = resourceSetLayouts[handle.index];
				entry.rootSignature = rootSignature;
				entry.bindings = std::move(runtimeBindings);
				return handle;
			}

			void DestroyResourceSetLayout(Rhi::SdResourceSetLayoutHandle layout) override
			{
				ResourceSetLayoutEntry* entry = Find(resourceSetLayouts, layout);
				if (!entry)
					return;
				*entry = {};
				entry->generation = layout.generation + 1;
			}

			Rhi::SdResourceSetHandle CreateResourceSet(const Rhi::SdResourceSetDesc& desc) override
			{
				ResourceSetLayoutEntry* layout = Find(resourceSetLayouts, desc.layout);
				if (!layout)
					return {};

				Rhi::SdResourceSetHandle handle = Allocate<Rhi::SdResourceSetHandle>(resourceSets);
				ResourceSetEntry& entry = resourceSets[handle.index];
				entry.layout = desc.layout;
				entry.rootTables.resize(layout->bindings.size());
				entry.sampledTextures.resize(layout->bindings.size());
				entry.uniformBuffers.resize(layout->bindings.size());

				for (SdSize i = 0; i < layout->bindings.size(); ++i)
				{
					const ResourceBindingRuntime& binding = layout->bindings[i];
					if (binding.desc.type == Rhi::SdShaderResourceType::Texture2D)
					{
						Rhi::SdTextureHandle texture = {};
						for (const Rhi::SdBoundTexture& bound : desc.textures)
						{
							if (bound.binding == binding.desc.binding)
							{
								texture = bound.texture;
								break;
							}
						}
						TextureEntry* textureEntry = Find(textures, texture);
						if (!textureEntry || !textureEntry->srv.IsValid())
						{
							ReleaseResourceSetEntry(entry);
							return {};
						}
						entry.sampledTextures[i] = texture;
						entry.rootTables[i] = textureEntry->srv.gpu;
					}
					else if (binding.desc.type == Rhi::SdShaderResourceType::Sampler)
					{
						Rhi::SdSamplerHandle sampler = {};
						for (const Rhi::SdBoundSampler& bound : desc.samplers)
						{
							if (bound.binding == binding.desc.binding)
							{
								sampler = bound.sampler;
								break;
							}
						}
						SamplerEntry* samplerEntry = Find(samplers, sampler);
						if (!samplerEntry || !samplerEntry->descriptor.IsValid())
						{
							ReleaseResourceSetEntry(entry);
							return {};
						}
						entry.rootTables[i] = samplerEntry->descriptor.gpu;
					}
					else if (binding.desc.type == Rhi::SdShaderResourceType::UniformBuffer)
					{
						Rhi::SdBoundBuffer boundBuffer = {};
						bool found = false;
						for (const Rhi::SdBoundBuffer& bound : desc.buffers)
						{
							if (bound.binding == binding.desc.binding)
							{
								boundBuffer = bound;
								found = true;
								break;
							}
						}
						BufferEntry* bufferEntry = found ? Find(buffers, boundBuffer.buffer) : nullptr;
						if (!bufferEntry || !bufferEntry->resource)
						{
							ReleaseResourceSetEntry(entry);
							return {};
						}
						UniformBufferBindingRuntime uniformBinding = {};
						uniformBinding.buffer = boundBuffer.buffer;
						uniformBinding.offset = boundBuffer.offset;
						uniformBinding.size = boundBuffer.size;
						uniformBinding.occupied = true;
						for (UINT frameIndex = 0; frameIndex < kFrameContextCount; ++frameIndex)
						{
							Dx12DescriptorAllocation allocation = resourceDescriptors.Allocate();
							if (!allocation.IsValid())
							{
								ReleaseResourceSetEntry(entry);
								return {};
							}
							CreateConstantBufferView(*bufferEntry, boundBuffer.offset, boundBuffer.size, frameIndex, allocation.cpu);
							entry.ownedResourceDescriptors.push_back(allocation);
							uniformBinding.rootTables[frameIndex] = allocation.gpu;
						}
						entry.uniformBuffers[i] = uniformBinding;
						entry.rootTables[i] = uniformBinding.rootTables[0];
					}
				}
				return handle;
			}

			void DestroyResourceSet(Rhi::SdResourceSetHandle resourceSet) override
			{
				ResourceSetEntry* entry = Find(resourceSets, resourceSet);
				if (!entry)
					return;
				const SdUInt32 nextGeneration = entry->generation + 1;
				ReleaseResourceSetEntry(*entry);
				entry->generation = nextGeneration;
			}

			Rhi::SdPipelineHandle CreateGraphicsPipeline(const Rhi::SdGraphicsPipelineDesc& desc) override
			{
				const ShaderEntry* vs = Find(shaders, desc.vertexShader);
				const ShaderEntry* ps = Find(shaders, desc.pixelShader);
				if (!vs || vs->stage != Rhi::SdShaderStage::Vertex || !ps || ps->stage != Rhi::SdShaderStage::Pixel)
					return {};

				const ResourceSetLayoutEntry* resourceLayout = Find(resourceSetLayouts, desc.resourceSetLayout);
				ID3D12RootSignature* rootSignature = resourceLayout ? resourceLayout->rootSignature.Get() : emptyRootSignature.Get();
				if (!rootSignature)
					return {};

				std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements = {};
				if (const VertexLayoutEntry* layout = Find(vertexLayouts, desc.vertexLayout))
				{
					inputElements.reserve(layout->attributes.size());
					for (const Rhi::SdVertexAttributeDesc& attribute : layout->attributes)
					{
						inputElements.push_back({
							attribute.semanticName.data(),
							attribute.semanticIndex,
							MapVertexFormat(attribute.format),
							attribute.bufferSlot,
							attribute.offset,
							D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
							0
							});
					}
				}

				D3D12_BLEND_DESC blendDesc = {};
				blendDesc.AlphaToCoverageEnable = desc.blendState.alphaToCoverage;
				const Rhi::SdBlendAttachmentDesc& colorBlend = desc.blendState.color;
				D3D12_RENDER_TARGET_BLEND_DESC& targetBlend = blendDesc.RenderTarget[0];
				targetBlend.BlendEnable = colorBlend.blendEnabled;
				targetBlend.SrcBlend = MapBlendFactor(colorBlend.srcColor);
				targetBlend.DestBlend = MapBlendFactor(colorBlend.dstColor);
				targetBlend.BlendOp = MapBlendOp(colorBlend.colorOp);
				targetBlend.SrcBlendAlpha = MapBlendFactor(colorBlend.srcAlpha);
				targetBlend.DestBlendAlpha = MapBlendFactor(colorBlend.dstAlpha);
				targetBlend.BlendOpAlpha = MapBlendOp(colorBlend.alphaOp);
				targetBlend.LogicOp = D3D12_LOGIC_OP_NOOP;
				targetBlend.RenderTargetWriteMask = static_cast<UINT8>(colorBlend.writeMask);

				D3D12_RASTERIZER_DESC rasterDesc = {};
				rasterDesc.FillMode = desc.rasterState.fillMode == Rhi::SdFillMode::Wireframe ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
				rasterDesc.CullMode = desc.rasterState.cullMode == Rhi::SdCullMode::Front ? D3D12_CULL_MODE_FRONT : (desc.rasterState.cullMode == Rhi::SdCullMode::Back ? D3D12_CULL_MODE_BACK : D3D12_CULL_MODE_NONE);
				rasterDesc.DepthClipEnable = desc.rasterState.depthClipEnabled;
				rasterDesc.MultisampleEnable = desc.sampleCount > 1;

				D3D12_DEPTH_STENCIL_DESC depthDesc = {};
				depthDesc.DepthEnable = desc.depthStencilState.depthTestEnabled;
				depthDesc.DepthWriteMask = desc.depthStencilState.depthWriteEnabled ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
				depthDesc.DepthFunc = MapCompare(desc.depthStencilState.depthCompare);
				depthDesc.StencilEnable = false;

				D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = {};
				pipelineDesc.pRootSignature = rootSignature;
				pipelineDesc.VS = { vs->bytecode.data(), vs->bytecode.size() };
				pipelineDesc.PS = { ps->bytecode.data(), ps->bytecode.size() };
				pipelineDesc.BlendState = blendDesc;
				pipelineDesc.SampleMask = UINT_MAX;
				pipelineDesc.RasterizerState = rasterDesc;
				pipelineDesc.DepthStencilState = depthDesc;
				pipelineDesc.InputLayout = { inputElements.empty() ? nullptr : inputElements.data(), static_cast<UINT>(inputElements.size()) };
				pipelineDesc.PrimitiveTopologyType = MapTopologyType(desc.topology);
				pipelineDesc.NumRenderTargets = 1;
				pipelineDesc.RTVFormats[0] = MapTextureFormat(desc.colorFormat);
				pipelineDesc.DSVFormat = MapTextureFormat(desc.depthFormat);
				pipelineDesc.SampleDesc.Count = desc.sampleCount;

				Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline = {};
				if (FAILED(nativeDevice->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(pipeline.ReleaseAndGetAddressOf()))))
					return {};

				Rhi::SdPipelineHandle handle = Allocate<Rhi::SdPipelineHandle>(pipelines);
				PipelineEntry& entry = pipelines[handle.index];
				entry.pipeline = pipeline;
				entry.resourceSetLayout = desc.resourceSetLayout;
				entry.topology = desc.topology;
				return handle;
			}

			void DestroyPipeline(Rhi::SdPipelineHandle pipeline) override
			{
				PipelineEntry* entry = Find(pipelines, pipeline);
				if (!entry)
					return;
				*entry = {};
				entry->generation = pipeline.generation + 1;
			}

		private:
			friend class SdDx12CommandEncoder;

			TextureEntry* FindTexture(Rhi::SdTextureHandle handle) noexcept { return Find(textures, handle); }
			BufferEntry* FindBuffer(Rhi::SdBufferHandle handle) noexcept { return Find(buffers, handle); }
			SamplerEntry* FindSampler(Rhi::SdSamplerHandle handle) noexcept { return Find(samplers, handle); }
			ResourceSetLayoutEntry* FindResourceSetLayout(Rhi::SdResourceSetLayoutHandle handle) noexcept { return Find(resourceSetLayouts, handle); }
			ResourceSetEntry* FindResourceSet(Rhi::SdResourceSetHandle handle) noexcept { return Find(resourceSets, handle); }
			PipelineEntry* FindPipeline(Rhi::SdPipelineHandle handle) noexcept { return Find(pipelines, handle); }
		};

		void SdDx12CommandEncoder::BeginRenderPass(const Rhi::SdRenderPassDesc& desc)
		{
			if (!device || !device->commandList)
				return;
			if (renderPassActive)
				EndRenderPass();

			std::array<D3D12_CPU_DESCRIPTOR_HANDLE, 8> rtvs = {};
			UINT rtvCount = 0;
			currentColorAttachments.clear();
			for (const Rhi::SdRenderPassColorAttachment& attachment : desc.colorAttachments)
			{
				if (rtvCount >= rtvs.size())
					break;
				SdDx12GpuDevice::TextureEntry* texture = device->FindTexture(attachment.texture);
				if (!texture || !texture->rtv.IsValid())
					continue;

				device->TransitionTexture(*texture, D3D12_RESOURCE_STATE_RENDER_TARGET);
				rtvs[rtvCount++] = texture->rtv.cpu;
				currentColorAttachments.push_back(attachment.texture);
				if (attachment.loadOp == Rhi::SdLoadOp::Clear)
				{
					const FLOAT clearColor[4] =
					{
						attachment.clearColor.r,
						attachment.clearColor.g,
						attachment.clearColor.b,
						attachment.clearColor.a
					};
					device->commandList->ClearRenderTargetView(texture->rtv.cpu, clearColor, 0, nullptr);
				}
			}

			D3D12_CPU_DESCRIPTOR_HANDLE* dsv = nullptr;
			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {};
			if (desc.depthStencil.IsValid())
			{
				if (SdDx12GpuDevice::TextureEntry* depth = device->FindTexture(desc.depthStencil))
				{
					device->TransitionTexture(*depth, D3D12_RESOURCE_STATE_DEPTH_WRITE);
					dsvHandle = depth->dsv.cpu;
					dsv = &dsvHandle;
				}
			}

			device->commandList->OMSetRenderTargets(rtvCount, rtvCount > 0 ? rtvs.data() : nullptr, FALSE, dsv);
			device->currentRenderTarget = currentColorAttachments.empty() ? Rhi::SdTextureHandle{} : currentColorAttachments.front();
			renderPassActive = true;

			if (desc.renderArea.Width() > 0 && desc.renderArea.Height() > 0)
			{
				SetViewport({
					static_cast<float>(desc.renderArea.left),
					static_cast<float>(desc.renderArea.top),
					static_cast<float>(desc.renderArea.Width()),
					static_cast<float>(desc.renderArea.Height()),
					0.0f,
					1.0f
					});
				SetScissorRect(desc.renderArea);
			}
		}

		void SdDx12CommandEncoder::EndRenderPass()
		{
			if (!device || !device->commandList || !renderPassActive)
				return;
			for (Rhi::SdTextureHandle handle : currentColorAttachments)
			{
				SdDx12GpuDevice::TextureEntry* texture = device->FindTexture(handle);
				if (!texture)
					continue;
				const D3D12_RESOURCE_STATES finalState = Rhi::SdHasFlag(texture->desc.usage, Rhi::SdTextureUsage::ShaderRead)
					? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
					: D3D12_RESOURCE_STATE_COMMON;
				device->TransitionTexture(*texture, finalState);
			}
			currentColorAttachments.clear();
			device->currentRenderTarget = {};
			renderPassActive = false;
		}

		void SdDx12CommandEncoder::SetPipeline(Rhi::SdPipelineHandle pipeline)
		{
			if (!device || !device->commandList)
				return;

			SdDx12GpuDevice::PipelineEntry* entry = device->FindPipeline(pipeline);
			if (!entry || !entry->pipeline)
				return;
			SdDx12GpuDevice::ResourceSetLayoutEntry* layout = device->FindResourceSetLayout(entry->resourceSetLayout);
			device->commandList->SetPipelineState(entry->pipeline.Get());
			device->commandList->SetGraphicsRootSignature(layout ? layout->rootSignature.Get() : device->emptyRootSignature.Get());
			device->commandList->IASetPrimitiveTopology(MapTopology(entry->topology));
		}

		void SdDx12CommandEncoder::SetResourceSet(SdUInt32 setIndex, Rhi::SdResourceSetHandle resourceSet)
		{
			if (!device || !device->commandList)
				return;

			SdDx12GpuDevice::ResourceSetEntry* set = device->FindResourceSet(resourceSet);
			if (!set)
				return;
			SdDx12GpuDevice::ResourceSetLayoutEntry* layout = device->FindResourceSetLayout(set->layout);
			if (!layout)
				return;

			for (Rhi::SdTextureHandle textureHandle : set->sampledTextures)
			{
				if (SdDx12GpuDevice::TextureEntry* texture = device->FindTexture(textureHandle))
					device->TransitionTexture(*texture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			}

			ID3D12DescriptorHeap* heaps[] = { device->GetResourceHeap(), device->GetSamplerHeap() };
			device->commandList->SetDescriptorHeaps(static_cast<UINT>(std::size(heaps)), heaps);
			for (SdSize i = 0; i < layout->bindings.size(); ++i)
			{
				const SdDx12GpuDevice::ResourceBindingRuntime& binding = layout->bindings[i];
				if (binding.desc.set != setIndex || i >= set->rootTables.size())
					continue;
				D3D12_GPU_DESCRIPTOR_HANDLE rootTable = set->rootTables[i];
				if (i < set->uniformBuffers.size() && set->uniformBuffers[i].occupied)
					rootTable = set->uniformBuffers[i].rootTables[device->GetActiveFrameResourceIndex()];
				if (rootTable.ptr == 0)
					continue;
				device->commandList->SetGraphicsRootDescriptorTable(binding.rootParameterIndex, rootTable);
			}
		}

			void SdDx12CommandEncoder::SetVertexBuffer(SdUInt32 slot, Rhi::SdBufferHandle buffer, SdUInt32 stride, SdUInt64 offset)
			{
				if (!device || !device->commandList)
					return;
				SdDx12GpuDevice::BufferEntry* entry = device->FindBuffer(buffer);
				if (!entry || !entry->resource || offset >= entry->desc.size)
					return;
				D3D12_VERTEX_BUFFER_VIEW view = {};
				view.BufferLocation = entry->resource->GetGPUVirtualAddress() + device->GetCurrentBufferFrameOffset(*entry) + offset;
				view.SizeInBytes = static_cast<UINT>(entry->desc.size - offset);
				view.StrideInBytes = stride;
				device->commandList->IASetVertexBuffers(slot, 1, &view);
			}

			void SdDx12CommandEncoder::SetIndexBuffer(Rhi::SdBufferHandle buffer, Rhi::SdIndexFormat format, SdUInt64 offset)
			{
				if (!device || !device->commandList)
					return;
				SdDx12GpuDevice::BufferEntry* entry = device->FindBuffer(buffer);
				if (!entry || !entry->resource || offset >= entry->desc.size)
					return;
				D3D12_INDEX_BUFFER_VIEW view = {};
				view.BufferLocation = entry->resource->GetGPUVirtualAddress() + device->GetCurrentBufferFrameOffset(*entry) + offset;
				view.SizeInBytes = static_cast<UINT>(entry->desc.size - offset);
				view.Format = format == Rhi::SdIndexFormat::UInt16 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
				device->commandList->IASetIndexBuffer(&view);
			}

		void SdDx12CommandEncoder::SetViewport(const Rhi::SdViewport& viewport)
		{
			if (!device || !device->commandList)
				return;
			D3D12_VIEWPORT nativeViewport = {};
			nativeViewport.TopLeftX = viewport.x;
			nativeViewport.TopLeftY = viewport.y;
			nativeViewport.Width = viewport.width;
			nativeViewport.Height = viewport.height;
			nativeViewport.MinDepth = viewport.minDepth;
			nativeViewport.MaxDepth = viewport.maxDepth;
			device->commandList->RSSetViewports(1, &nativeViewport);
		}

		void SdDx12CommandEncoder::SetScissorRect(const Rhi::SdRectI& rect)
		{
			if (!device || !device->commandList)
				return;
			D3D12_RECT nativeRect = {};
			nativeRect.left = rect.left;
			nativeRect.top = rect.top;
			nativeRect.right = rect.right;
			nativeRect.bottom = rect.bottom;
			device->commandList->RSSetScissorRects(1, &nativeRect);
		}

		void SdDx12CommandEncoder::Draw(SdUInt32 vertexCount, SdUInt32 instanceCount, SdUInt32 firstVertex, SdUInt32 firstInstance)
		{
			if (device && device->commandList)
				device->commandList->DrawInstanced(vertexCount, instanceCount, firstVertex, firstInstance);
		}

		void SdDx12CommandEncoder::DrawIndexed(SdUInt32 indexCount, SdUInt32 instanceCount, SdUInt32 firstIndex, SdInt32 vertexOffset, SdUInt32 firstInstance)
		{
			if (device && device->commandList)
				device->commandList->DrawIndexedInstanced(indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
		}

		class Dx12RenderLayerRuntime final : public ISdRenderLayerProvider
		{
		private:
			struct Entry final
			{
				SdRenderLayerId id = SdInvalidRenderLayerId;
				Rhi::SdTextureHandle texture = {};
				Rhi::SdTextureFormat format = Rhi::SdTextureFormat::Unknown;
				SdUInt32 width = 0;
				SdUInt32 height = 0;
				SdRect bounds = {};
				bool occupied = false;
			};

			SdDx12GpuDevice& device;
			Rhi::ISdCommandEncoder& encoder;
			SdVec2 displaySize = {};
			Rhi::SdTextureHandle rootTexture = {};
			Rhi::SdTextureFormat rootFormat = Rhi::SdTextureFormat::Rgba8Unorm;
			SdUInt32 rootWidth = 0;
			SdUInt32 rootHeight = 0;
			std::vector<Entry> entries = {};
			std::vector<SdRenderLayerId> targetStack = {};
			SdRenderLayerId currentLayer = SdRootRenderLayerId;

			Entry* TryGetEntry(SdRenderLayerId layerId) noexcept
			{
				if (layerId == SdRootRenderLayerId || layerId == SdInvalidRenderLayerId || layerId >= entries.size())
					return nullptr;
				Entry& entry = entries[layerId.value];
				return entry.occupied ? &entry : nullptr;
			}

			const Entry* TryGetEntry(SdRenderLayerId layerId) const noexcept
			{
				if (layerId == SdRootRenderLayerId || layerId == SdInvalidRenderLayerId || layerId >= entries.size())
					return nullptr;
				const Entry& entry = entries[layerId.value];
				return entry.occupied ? &entry : nullptr;
			}

			Entry& EnsureEntry(SdRenderLayerId layerId)
			{
				if (entries.size() <= layerId)
					entries.resize(layerId.value + 1);
				Entry& entry = entries[layerId.value];
				entry.id = layerId;
				entry.occupied = true;
				return entry;
			}

			Rhi::SdRectI MakeLayerRenderArea(const SdEffectRenderLayer& layer) const noexcept
			{
				return {
					0,
					0,
					static_cast<SdInt32>(layer.width),
					static_cast<SdInt32>(layer.height)
				};
			}

		public:
			Dx12RenderLayerRuntime(
				SdDx12GpuDevice& gpuDevice,
				Rhi::SdTextureHandle frameTexture,
				Rhi::SdTextureFormat frameFormat,
				SdUInt32 frameWidth,
				SdUInt32 frameHeight,
				SdVec2 frameDisplaySize)
				: device(gpuDevice),
				encoder(gpuDevice.GetImmediateEncoder()),
				displaySize(frameDisplaySize),
				rootTexture(frameTexture),
				rootFormat(frameFormat),
				rootWidth(frameWidth),
				rootHeight(frameHeight)
			{
			}

			~Dx12RenderLayerRuntime() override
			{
				encoder.EndRenderPass();
				for (Entry& entry : entries)
				{
					if (entry.texture.IsValid())
						device.DestroyTexture(entry.texture);
				}
			}

			SdEffectRenderLayer FindRenderLayer(SdRenderLayerId layerId) const noexcept override
			{
				if (layerId == SdRootRenderLayerId)
				{
					return {
						SdRootRenderLayerId,
						rootTexture,
						rootFormat,
						rootWidth != 0 ? rootWidth : static_cast<SdUInt32>(std::max(1.0f, displaySize.x)),
						rootHeight != 0 ? rootHeight : static_cast<SdUInt32>(std::max(1.0f, displaySize.y)),
						{ 0.0f, 0.0f, displaySize.x, displaySize.y }
					};
				}

				const Entry* entry = TryGetEntry(layerId);
				if (!entry)
					return {};
				return {
					entry->id,
					entry->texture,
					entry->format,
					entry->width,
					entry->height,
					entry->bounds
				};
			}

			bool BindRenderLayerTarget(SdRenderLayerId layerId, bool clear, const SdColorLinear& clearColor)
			{
				const SdEffectRenderLayer layer = FindRenderLayer(layerId);
				if (!layer.IsValid())
					return false;

				const Rhi::SdRenderPassColorAttachment colorAttachment =
				{
					layer.texture,
					clear ? Rhi::SdLoadOp::Clear : Rhi::SdLoadOp::Load,
					Rhi::SdStoreOp::Store,
					clearColor
				};
				const std::array<Rhi::SdRenderPassColorAttachment, 1> colorAttachments = { colorAttachment };
				const Rhi::SdRenderPassDesc renderPass =
				{
					colorAttachments,
					{},
					MakeLayerRenderArea(layer),
					layerId == SdRootRenderLayerId ? SODIUM_STRING("Sodium.RenderLayer.RootPass") : SODIUM_STRING("Sodium.RenderLayer.OffscreenPass")
				};
				encoder.BeginRenderPass(renderPass);
				currentLayer = layerId;
				return true;
			}

			bool BeginRenderLayer(const SdBeginRenderLayerPayload& payload)
			{
				if (payload.layerId == SdInvalidRenderLayerId || payload.layerId == SdRootRenderLayerId)
					return false;

				const SdUInt32 width = std::max(1u, static_cast<SdUInt32>(std::ceil(payload.bounds.Width())));
				const SdUInt32 height = std::max(1u, static_cast<SdUInt32>(std::ceil(payload.bounds.Height())));
				const Rhi::SdTextureFormat format = payload.format == Rhi::SdTextureFormat::Unknown ? rootFormat : payload.format;

				Entry& entry = EnsureEntry(payload.layerId);
				if (entry.texture.IsValid())
					device.DestroyTexture(entry.texture);

				const Rhi::SdTextureDesc textureDesc =
				{
					width,
					height,
					1,
					1,
					format,
					Rhi::SdTextureUsage::RenderTarget | Rhi::SdTextureUsage::ShaderRead,
					1,
					false,
					SODIUM_STRING("Sodium.RenderLayer.Texture")
				};
				entry.texture = device.CreateTexture(textureDesc);
				if (!entry.texture.IsValid())
				{
					entry = {};
					return false;
				}

				entry.format = format;
				entry.width = width;
				entry.height = height;
				entry.bounds = payload.bounds;
				entry.occupied = true;

				targetStack.push_back(currentLayer);
				return BindRenderLayerTarget(payload.layerId, payload.clear, payload.clearColor);
			}

			bool EndRenderLayer(SdRenderLayerId)
			{
				encoder.EndRenderPass();
				currentLayer = targetStack.empty() ? SdRootRenderLayerId : targetStack.back();
				if (!targetStack.empty())
					targetStack.pop_back();
				return BindRenderLayerTarget(currentLayer, false, {});
			}

			Rhi::SdTextureFormat GetRenderLayerFormat(SdRenderLayerId layerId) const noexcept
			{
				const SdEffectRenderLayer layer = FindRenderLayer(layerId);
				return layer.format == Rhi::SdTextureFormat::Unknown ? rootFormat : layer.format;
			}

			SdRenderLayerId GetCurrentLayer() const noexcept { return currentLayer; }
		};
	}

	struct SdDx12Renderer::Impl final
	{
		struct TextureEntry final
		{
			Rhi::SdTextureHandle texture = {};
			Rhi::SdResourceSetHandle resourceSet = {};
			SdUInt32 width = 0;
			SdUInt32 height = 0;
			SdUInt32 generation = 0;
			bool occupied = false;
		};

		struct FrameRenderTarget final
		{
			Rhi::SdTextureHandle texture = {};
			Rhi::SdTextureFormat format = Rhi::SdTextureFormat::Rgba8Unorm;
			SdUInt32 width = 0;
			SdUInt32 height = 0;
		};

		SdDx12GpuDevice rhiDevice = {};
		Microsoft::WRL::ComPtr<IDXGIFactory4> factory = {};
		Microsoft::WRL::ComPtr<ID3D12Device> ownedDevice = {};
		Microsoft::WRL::ComPtr<ID3D12CommandQueue> ownedCommandQueue = {};
		Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain = {};
		std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> swapchainRenderTargets = {};
		Rhi::SdBufferHandle vertexBuffer = {};
		Rhi::SdBufferHandle indexBuffer = {};
		Rhi::SdBufferHandle frameConstantBuffer = {};
		Rhi::SdShaderHandle vertexShader = {};
		Rhi::SdShaderHandle pixelShader = {};
		Rhi::SdVertexLayoutHandle vertexLayout = {};
		Rhi::SdSamplerHandle sampler = {};
		Rhi::SdResourceSetLayoutHandle resourceSetLayout = {};
		Rhi::SdPipelineHandle pipeline = {};
		std::vector<TextureEntry> textures = {};
		std::vector<FrameRenderTarget> frameRenderTargets = {};
		std::array<float, 4> frameClearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
		DXGI_FORMAT renderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		UINT currentRenderTargetIndex = kInvalidRenderTargetIndex;
		UINT swapchainFrameIndex = 0;
		UINT presentSyncInterval = 0;
		SdUInt32 vertexCapacity = 0;
		SdUInt32 indexCapacity = 0;
		SdUInt32 swapchainWidth = 0;
		SdUInt32 swapchainHeight = 0;
		bool clearFrameTarget = false;
		bool swapchainOccluded = false;
		bool ownsDeviceResources = false;

		bool CreateOwnedDevice()
		{
			UINT factoryFlags = 0;
#if defined(_DEBUG)
			Microsoft::WRL::ComPtr<ID3D12Debug> debugController = {};
			if (SUCCEEDED(::D3D12GetDebugInterface(IID_PPV_ARGS(debugController.GetAddressOf()))))
			{
				debugController->EnableDebugLayer();
				factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
			}
#endif

			if (FAILED(::CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(factory.ReleaseAndGetAddressOf()))))
			{
				factoryFlags = 0;
				if (FAILED(::CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(factory.ReleaseAndGetAddressOf()))))
					return false;
			}

			if (FAILED(::D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(ownedDevice.ReleaseAndGetAddressOf()))))
			{
				Microsoft::WRL::ComPtr<IDXGIAdapter> warpAdapter = {};
				if (FAILED(factory->EnumWarpAdapter(IID_PPV_ARGS(warpAdapter.GetAddressOf()))))
					return false;
				if (FAILED(::D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(ownedDevice.ReleaseAndGetAddressOf()))))
					return false;
			}

			D3D12_COMMAND_QUEUE_DESC queueDesc = {};
			queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
			return SUCCEEDED(ownedDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(ownedCommandQueue.ReleaseAndGetAddressOf())));
		}

		bool CreateOwnedDeviceAndSwapchain(const Rhi::SdSwapchainDesc& desc)
		{
			HWND windowHandle = static_cast<HWND>(desc.nativeWindow);
			if (!windowHandle || desc.width == 0 || desc.height == 0)
				return false;

			renderTargetFormat = MapTextureFormat(desc.colorFormat);
			if (renderTargetFormat == DXGI_FORMAT_UNKNOWN)
				return false;
			swapchainWidth = desc.width;
			swapchainHeight = desc.height;
			presentSyncInterval = desc.presentMode == Rhi::SdPresentMode::Fifo ? 1u : 0u;

			if (!CreateOwnedDevice())
				return false;

			DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
			swapChainDesc.Width = desc.width;
			swapChainDesc.Height = desc.height;
			swapChainDesc.Format = renderTargetFormat;
			swapChainDesc.BufferCount = std::max(2u, desc.bufferCount);
			swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapChainDesc.SampleDesc.Count = 1;
			swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

			Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1 = {};
			if (FAILED(factory->CreateSwapChainForHwnd(ownedCommandQueue.Get(), windowHandle, &swapChainDesc, nullptr, nullptr, swapChain1.ReleaseAndGetAddressOf())))
				return false;
			factory->MakeWindowAssociation(windowHandle, DXGI_MWA_NO_ALT_ENTER);
			if (FAILED(swapChain1.As(&swapChain)))
				return false;
			swapchainFrameIndex = swapChain->GetCurrentBackBufferIndex();
			ownsDeviceResources = true;
			return true;
		}

		bool RefreshSwapchainRenderTargets()
		{
			if (!swapChain)
				return false;

			DXGI_SWAP_CHAIN_DESC desc = {};
			if (FAILED(swapChain->GetDesc(&desc)) || desc.BufferCount == 0)
				return false;

			swapchainRenderTargets.resize(desc.BufferCount);
			std::vector<ID3D12Resource*> renderTargets(desc.BufferCount, nullptr);
			for (UINT i = 0; i < desc.BufferCount; ++i)
			{
				if (FAILED(swapChain->GetBuffer(i, IID_PPV_ARGS(swapchainRenderTargets[i].ReleaseAndGetAddressOf()))))
					return false;
				renderTargets[i] = swapchainRenderTargets[i].Get();
			}
			return SetRenderTargets(renderTargets.data(), static_cast<UINT>(renderTargets.size()), renderTargetFormat);
		}

		bool ResizeSwapchain(SdUInt32 width, SdUInt32 height)
		{
			if (!swapChain || width == 0 || height == 0)
				return true;
			if (width == swapchainWidth && height == swapchainHeight)
				return true;

			rhiDevice.WaitIdle();
			ReleaseRenderTargets();
			for (Microsoft::WRL::ComPtr<ID3D12Resource>& target : swapchainRenderTargets)
				target.Reset();
			swapchainRenderTargets.clear();

			if (FAILED(swapChain->ResizeBuffers(0, width, height, renderTargetFormat, 0)))
				return false;
			swapchainWidth = width;
			swapchainHeight = height;
			swapchainFrameIndex = swapChain->GetCurrentBackBufferIndex();
			return RefreshSwapchainRenderTargets();
		}

		bool Present()
		{
			if (!swapChain)
				return true;
			const HRESULT hr = swapChain->Present(presentSyncInterval, 0);
			swapchainOccluded = hr == DXGI_STATUS_OCCLUDED;
			if (SUCCEEDED(hr) || hr == DXGI_STATUS_OCCLUDED)
			{
				swapchainFrameIndex = swapChain->GetCurrentBackBufferIndex();
				return true;
			}
			return false;
		}

		bool CreatePipelineState()
		{
			vertexShader = rhiDevice.CreateShaderFromSource({
				Rhi::SdShaderStage::Vertex,
				Rhi::SdShaderLanguage::Hlsl,
				kVertexShader,
				SODIUM_STRING("main"),
				SODIUM_STRING("vs_5_0"),
				SODIUM_STRING("Sodium.Gui.Default.VS")
				});
			pixelShader = rhiDevice.CreateShaderFromSource({
				Rhi::SdShaderStage::Pixel,
				Rhi::SdShaderLanguage::Hlsl,
				kPixelShader,
				SODIUM_STRING("main"),
				SODIUM_STRING("ps_5_0"),
				SODIUM_STRING("Sodium.Gui.Default.PS")
				});
			if (!vertexShader.IsValid() || !pixelShader.IsValid())
				return false;

			const Rhi::SdVertexAttributeDesc attributes[] =
			{
				{ SODIUM_STRING("POSITION"), 0, Rhi::SdVertexFormat::Float2, static_cast<SdUInt32>(offsetof(SdVertex, position)), 0 },
				{ SODIUM_STRING("TEXCOORD"), 0, Rhi::SdVertexFormat::Float2, static_cast<SdUInt32>(offsetof(SdVertex, uv)), 0 },
				{ SODIUM_STRING("COLOR"), 0, Rhi::SdVertexFormat::UByte4Norm, static_cast<SdUInt32>(offsetof(SdVertex, color)), 0 }
			};
			vertexLayout = rhiDevice.CreateVertexLayout({ attributes, SODIUM_STRING("Sodium.Gui.Default.VertexLayout") });
			if (!vertexLayout.IsValid())
				return false;

			const Rhi::SdShaderBindingDesc bindings[] =
			{
				{ SODIUM_STRING("FrameConstants"), Rhi::SdShaderResourceType::UniformBuffer, 0, 0, static_cast<Rhi::SdShaderStageFlags>(Rhi::SdShaderStageFlag::Vertex) },
				{ SODIUM_STRING("texture0"), Rhi::SdShaderResourceType::Texture2D, 0, 0, static_cast<Rhi::SdShaderStageFlags>(Rhi::SdShaderStageFlag::Pixel) },
				{ SODIUM_STRING("sampler0"), Rhi::SdShaderResourceType::Sampler, 0, 0, static_cast<Rhi::SdShaderStageFlags>(Rhi::SdShaderStageFlag::Pixel) }
			};
			resourceSetLayout = rhiDevice.CreateResourceSetLayout({ bindings, SODIUM_STRING("Sodium.Gui.Default.ResourceLayout") });
			if (!resourceSetLayout.IsValid())
				return false;

			sampler = rhiDevice.CreateSampler({ Rhi::SdFilterMode::Linear, Rhi::SdFilterMode::Linear, Rhi::SdFilterMode::Linear, Rhi::SdAddressMode::Clamp, Rhi::SdAddressMode::Clamp, Rhi::SdAddressMode::Clamp, SODIUM_STRING("Sodium.Gui.Default.Sampler") });
			if (!sampler.IsValid())
				return false;

			frameConstantBuffer = rhiDevice.CreateBuffer({
				sizeof(FrameConstants),
				static_cast<Rhi::SdBufferUsageFlags>(Rhi::SdBufferUsage::Uniform),
				Rhi::SdMemoryUsage::CpuToGpu,
				SODIUM_STRING("Sodium.Gui.FrameConstants")
				}, nullptr);
			if (!frameConstantBuffer.IsValid())
				return false;

			Rhi::SdGraphicsPipelineDesc pipelineDesc = {};
			pipelineDesc.vertexShader = vertexShader;
			pipelineDesc.pixelShader = pixelShader;
			pipelineDesc.vertexLayout = vertexLayout;
			pipelineDesc.resourceSetLayout = resourceSetLayout;
			pipelineDesc.topology = Rhi::SdPrimitiveTopology::TriangleList;
			pipelineDesc.colorFormat = MapDxgiFormatToRhi(renderTargetFormat);
			pipelineDesc.debugName = SODIUM_STRING("Sodium.Gui.Default.Pipeline");
			pipeline = rhiDevice.CreateGraphicsPipeline(pipelineDesc);
			return pipeline.IsValid();
		}

		bool EnsureBuffers(SdUInt32 vertexCount, SdUInt32 indexCount)
		{
			if (vertexCount > vertexCapacity)
			{
				if (vertexBuffer.IsValid())
					rhiDevice.DestroyBuffer(vertexBuffer);
				vertexCapacity = std::max(vertexCount, vertexCapacity == 0 ? 1024u : vertexCapacity * 2u);
				vertexBuffer = rhiDevice.CreateBuffer({
					static_cast<SdUInt64>(vertexCapacity) * sizeof(SdVertex),
					static_cast<Rhi::SdBufferUsageFlags>(Rhi::SdBufferUsage::Vertex),
					Rhi::SdMemoryUsage::CpuToGpu,
					SODIUM_STRING("Sodium.Gui.VertexBuffer")
					}, nullptr);
				if (!vertexBuffer.IsValid())
					return false;
			}

			if (indexCount > indexCapacity)
			{
				if (indexBuffer.IsValid())
					rhiDevice.DestroyBuffer(indexBuffer);
				indexCapacity = std::max(indexCount, indexCapacity == 0 ? 2048u : indexCapacity * 2u);
				indexBuffer = rhiDevice.CreateBuffer({
					static_cast<SdUInt64>(indexCapacity) * sizeof(SdUInt32),
					static_cast<Rhi::SdBufferUsageFlags>(Rhi::SdBufferUsage::Index),
					Rhi::SdMemoryUsage::CpuToGpu,
					SODIUM_STRING("Sodium.Gui.IndexBuffer")
					}, nullptr);
				if (!indexBuffer.IsValid())
					return false;
			}
			return true;
		}

		bool UploadBuffers(const SdRenderPacket& packet)
		{
			if (!EnsureBuffers(static_cast<SdUInt32>(packet.vertices.size()), static_cast<SdUInt32>(packet.indices.size())))
				return false;
			if (packet.vertices.empty() || packet.indices.empty())
				return true;

			return rhiDevice.UpdateBuffer(vertexBuffer, packet.vertices.data(), packet.vertices.size() * sizeof(SdVertex), 0)
				&& rhiDevice.UpdateBuffer(indexBuffer, packet.indices.data(), packet.indices.size() * sizeof(SdUInt32), 0);
		}

		TextureEntry& EnsureTextureEntry(SdTextureHandle texture)
		{
			if (textures.size() <= texture.index)
				textures.resize(texture.index + 1);
			TextureEntry& entry = textures[texture.index];
			if (entry.generation == 0)
				entry.generation = texture.generation;
			entry.occupied = true;
			return entry;
		}

		bool RebuildTextureResourceSet(TextureEntry& entry)
		{
			if (!entry.texture.IsValid() || !sampler.IsValid() || !frameConstantBuffer.IsValid() || !resourceSetLayout.IsValid())
				return false;
			if (entry.resourceSet.IsValid())
				rhiDevice.DestroyResourceSet(entry.resourceSet);

			const Rhi::SdBoundTexture textures[] = { { 0, entry.texture } };
			const Rhi::SdBoundSampler samplers[] = { { 0, sampler } };
			const Rhi::SdBoundBuffer buffers[] = { { 0, frameConstantBuffer, 0, sizeof(FrameConstants) } };
			entry.resourceSet = rhiDevice.CreateResourceSet({
				resourceSetLayout,
				textures,
				samplers,
				buffers,
				SODIUM_STRING("Sodium.Gui.TextureResourceSet")
				});
			return entry.resourceSet.IsValid();
		}

		bool UploadTexture(const SdUploadRequest& request)
		{
			if (!request.texture.IsValid() || request.width == 0 || request.height == 0 || request.pixels.empty())
				return false;

			TextureEntry& entry = EnsureTextureEntry(request.texture);
			if (!entry.texture.IsValid() || entry.width != request.width || entry.height != request.height || entry.generation != request.texture.generation)
			{
				if (entry.resourceSet.IsValid())
				{
					rhiDevice.DestroyResourceSet(entry.resourceSet);
					entry.resourceSet = {};
				}
				if (entry.texture.IsValid())
					rhiDevice.DestroyTexture(entry.texture);

				entry.texture = rhiDevice.CreateTexture({
					request.width,
					request.height,
					1,
					1,
					Rhi::SdTextureFormat::Rgba8Unorm,
					static_cast<Rhi::SdTextureUsageFlags>(Rhi::SdTextureUsage::ShaderRead),
					1,
					false,
					SODIUM_STRING("Sodium.Gui.UploadedTexture")
					});
				if (!entry.texture.IsValid())
					return false;
				if (!RebuildTextureResourceSet(entry))
				{
					rhiDevice.DestroyTexture(entry.texture);
					entry.texture = {};
					return false;
				}
				entry.width = request.width;
				entry.height = request.height;
				entry.generation = request.texture.generation;
				entry.occupied = true;
			}

			return rhiDevice.UpdateTexture(entry.texture, request.pixels.data(), request.width * sizeof(SdUInt32));
		}

		void BindPipeline(SdVec2 displaySize)
		{
			const float left = 0.0f;
			const float right = displaySize.x;
			const float top = 0.0f;
			const float bottom = displaySize.y;

			FrameConstants constants = {};
			constants.projection[0][0] = 2.0f / (right - left);
			constants.projection[1][1] = 2.0f / (top - bottom);
			constants.projection[2][2] = 0.5f;
			constants.projection[3][0] = (right + left) / (left - right);
			constants.projection[3][1] = (top + bottom) / (bottom - top);
			constants.projection[3][2] = 0.5f;
			constants.projection[3][3] = 1.0f;

			rhiDevice.UpdateBuffer(frameConstantBuffer, &constants, sizeof(constants), 0);

			Rhi::ISdCommandEncoder& encoder = rhiDevice.GetImmediateEncoder();
			encoder.SetPipeline(pipeline);
			encoder.SetVertexBuffer(0, vertexBuffer, sizeof(SdVertex), 0);
			encoder.SetIndexBuffer(indexBuffer, Rhi::SdIndexFormat::UInt32, 0);
			encoder.SetViewport({ 0.0f, 0.0f, displaySize.x, displaySize.y, 0.0f, 1.0f });
		}

		TextureEntry* TryGetTexture(SdTextureHandle texture) noexcept
		{
			if (!texture.IsValid() || texture.index >= textures.size())
				return nullptr;
			TextureEntry& entry = textures[texture.index];
			if (!entry.occupied || entry.generation != texture.generation || !entry.resourceSet.IsValid())
				return nullptr;
			return &entry;
		}

		bool Initialize(const Config& config)
		{
			ID3D12Device* device = config.device;
			ID3D12CommandQueue* commandQueue = config.commandQueue;
			const bool useOwnedSwapchain = config.swapchain.nativeWindow != nullptr;
			if (useOwnedSwapchain)
			{
				if (!CreateOwnedDeviceAndSwapchain(config.swapchain))
					return false;
				device = ownedDevice.Get();
				commandQueue = ownedCommandQueue.Get();
			}
			else
			{
				renderTargetFormat = config.renderTargetFormat;
			}

			if (!rhiDevice.Initialize({ device, commandQueue }))
				return false;
			const bool renderTargetsReady = useOwnedSwapchain
				? RefreshSwapchainRenderTargets()
				: SetRenderTargets(config.renderTargets, config.renderTargetCount, config.renderTargetFormat);
			if (!renderTargetsReady)
			{
				Shutdown();
				return false;
			}
			if (!CreatePipelineState())
			{
				Shutdown();
				return false;
			}
			return true;
		}

		void Shutdown()
		{
			ReleaseRenderTargets();
			for (TextureEntry& texture : textures)
			{
				if (texture.resourceSet.IsValid())
					rhiDevice.DestroyResourceSet(texture.resourceSet);
				if (texture.texture.IsValid())
					rhiDevice.DestroyTexture(texture.texture);
			}
			textures.clear();
			if (pipeline.IsValid())
				rhiDevice.DestroyPipeline(pipeline);
			if (resourceSetLayout.IsValid())
				rhiDevice.DestroyResourceSetLayout(resourceSetLayout);
			if (sampler.IsValid())
				rhiDevice.DestroySampler(sampler);
			if (vertexLayout.IsValid())
				rhiDevice.DestroyVertexLayout(vertexLayout);
			if (pixelShader.IsValid())
				rhiDevice.DestroyShader(pixelShader);
			if (vertexShader.IsValid())
				rhiDevice.DestroyShader(vertexShader);
			if (frameConstantBuffer.IsValid())
				rhiDevice.DestroyBuffer(frameConstantBuffer);
			if (indexBuffer.IsValid())
				rhiDevice.DestroyBuffer(indexBuffer);
			if (vertexBuffer.IsValid())
				rhiDevice.DestroyBuffer(vertexBuffer);
			rhiDevice.Shutdown();
			swapchainRenderTargets.clear();
			swapChain.Reset();
			ownedCommandQueue.Reset();
			ownedDevice.Reset();
			factory.Reset();
			pipeline = {};
			resourceSetLayout = {};
			sampler = {};
			vertexLayout = {};
			pixelShader = {};
			vertexShader = {};
			frameConstantBuffer = {};
			indexBuffer = {};
			vertexBuffer = {};
			vertexCapacity = 0;
			indexCapacity = 0;
			currentRenderTargetIndex = kInvalidRenderTargetIndex;
			swapchainFrameIndex = 0;
			presentSyncInterval = 0;
			swapchainWidth = 0;
			swapchainHeight = 0;
			swapchainOccluded = false;
			ownsDeviceResources = false;
			clearFrameTarget = false;
		}

		bool SetRenderTargets(ID3D12Resource* const* renderTargets, UINT renderTargetCount, DXGI_FORMAT targetFormat)
		{
			if (!rhiDevice.IsInitialized() || !renderTargets || renderTargetCount == 0)
				return false;
			if (!renderTargets[0])
				return false;

			ReleaseRenderTargets();
			const D3D12_RESOURCE_DESC firstDesc = renderTargets[0]->GetDesc();
			const DXGI_FORMAT resolvedFormat = targetFormat == DXGI_FORMAT_UNKNOWN ? firstDesc.Format : targetFormat;
			if (resolvedFormat == DXGI_FORMAT_UNKNOWN)
				return false;
			renderTargetFormat = resolvedFormat;
			frameRenderTargets.resize(renderTargetCount);
			for (UINT i = 0; i < renderTargetCount; ++i)
			{
				if (!renderTargets[i])
				{
					ReleaseRenderTargets();
					return false;
				}

				D3D12_RESOURCE_DESC nativeDesc = renderTargets[i]->GetDesc();
				const Rhi::SdTextureFormat format = MapDxgiFormatToRhi(renderTargetFormat);
				const Rhi::SdTextureDesc desc =
				{
					static_cast<SdUInt32>(nativeDesc.Width),
					nativeDesc.Height,
					1,
					1,
					format,
					static_cast<Rhi::SdTextureUsageFlags>(Rhi::SdTextureUsage::RenderTarget),
					nativeDesc.SampleDesc.Count,
					false,
					SODIUM_STRING("Sodium.Dx12.FrameRenderTarget")
				};
				frameRenderTargets[i].texture = rhiDevice.ImportTexture2D(renderTargets[i], desc, D3D12_RESOURCE_STATE_PRESENT);
				frameRenderTargets[i].format = format;
				frameRenderTargets[i].width = desc.width;
				frameRenderTargets[i].height = desc.height;
				if (!frameRenderTargets[i].texture.IsValid())
				{
					ReleaseRenderTargets();
					return false;
				}
			}
			return true;
		}

		void ReleaseRenderTargets()
		{
			rhiDevice.WaitForGpu();
			for (FrameRenderTarget& target : frameRenderTargets)
			{
				if (target.texture.IsValid())
					rhiDevice.DestroyTexture(target.texture);
			}
			frameRenderTargets.clear();
			currentRenderTargetIndex = kInvalidRenderTargetIndex;
			clearFrameTarget = false;
		}

		SdTextureHandle CreateTexture(SdUInt32 width, SdUInt32 height, const void* rgbaPixels)
		{
			if (width == 0 || height == 0 || !rgbaPixels)
				return {};

			SdUInt32 index = 1;
			for (; index < textures.size(); ++index)
			{
				if (!textures[index].occupied)
					break;
			}
			if (textures.size() <= index)
				textures.resize(index + 1);

			TextureEntry& entry = textures[index];
			entry.texture = rhiDevice.CreateTexture({
				width,
				height,
				1,
				1,
				Rhi::SdTextureFormat::Rgba8Unorm,
				static_cast<Rhi::SdTextureUsageFlags>(Rhi::SdTextureUsage::ShaderRead),
				1,
				false,
				SODIUM_STRING("")
				});
			if (!entry.texture.IsValid())
				return {};
			if (!rhiDevice.UpdateTexture(entry.texture, rgbaPixels, width * sizeof(SdUInt32)))
			{
				rhiDevice.DestroyTexture(entry.texture);
				entry.texture = {};
				return {};
			}
			entry.width = width;
			entry.height = height;
			entry.generation = entry.generation == 0 ? 1 : entry.generation + 1;
			entry.occupied = true;
			if (!RebuildTextureResourceSet(entry))
			{
				rhiDevice.DestroyTexture(entry.texture);
				entry.texture = {};
				entry.occupied = false;
				return {};
			}
			return SdTextureHandle(index, entry.generation);
		}

		void Render(const SdRendererFrameInfo& frameInfo, const SdRenderPacket& packet)
		{
			if (!rhiDevice.IsInitialized() || currentRenderTargetIndex >= frameRenderTargets.size())
				return;
			if (!rhiDevice.BeginFrame())
				return;

			for (const SdUploadRequest& upload : packet.resourceUpdates)
				UploadTexture(upload);
			const bool hasGeometry = !packet.vertices.empty() && !packet.indices.empty();
			if (hasGeometry && !UploadBuffers(packet))
			{
				rhiDevice.EndFrame(frameRenderTargets[currentRenderTargetIndex].texture);
				return;
			}

			FrameRenderTarget& frameTarget = frameRenderTargets[currentRenderTargetIndex];
			Rhi::ISdCommandEncoder& encoder = rhiDevice.GetImmediateEncoder();
			Dx12RenderLayerRuntime renderLayers(
				rhiDevice,
				frameTarget.texture,
				frameTarget.format,
				frameTarget.width,
				frameTarget.height,
				frameInfo.displaySize);
			const SdColorLinear clearColor =
			{
				frameClearColor[0],
				frameClearColor[1],
				frameClearColor[2],
				frameClearColor[3]
			};
			renderLayers.BindRenderLayerTarget(SdRootRenderLayerId, clearFrameTarget, clearColor);
			BindPipeline(frameInfo.displaySize);

			const Rhi::SdRectI fullScissor =
			{
				0,
				0,
				static_cast<SdInt32>(frameInfo.displaySize.x),
				static_cast<SdInt32>(frameInfo.displaySize.y)
			};
			std::vector<Rhi::SdRectI> clipStack = {};
			Rhi::SdRectI currentScissor = fullScissor;
			const auto applyScissor = [&encoder, &currentScissor](const Rhi::SdRectI& scissor)
			{
				currentScissor = scissor;
				encoder.SetScissorRect(scissor);
			};
			const auto makeScissor = [&frameInfo](const SdRect& rect)
			{
				Rhi::SdRectI scissor = {};
				scissor.left = static_cast<SdInt32>(std::floor(std::clamp(rect.min.x, 0.0f, frameInfo.displaySize.x)));
				scissor.top = static_cast<SdInt32>(std::floor(std::clamp(rect.min.y, 0.0f, frameInfo.displaySize.y)));
				scissor.right = static_cast<SdInt32>(std::ceil(std::clamp(rect.max.x, 0.0f, frameInfo.displaySize.x)));
				scissor.bottom = static_cast<SdInt32>(std::ceil(std::clamp(rect.max.y, 0.0f, frameInfo.displaySize.y)));
				if (scissor.right < scissor.left)
					scissor.right = scissor.left;
				if (scissor.bottom < scissor.top)
					scissor.bottom = scissor.top;
				return scissor;
			};
			for (const SdRenderCommand& command : packet.commandBuffer.GetCommands())
			{
				if (command.kind == SdRenderCommandKind::DrawBatch)
				{
					if (!hasGeometry)
						continue;
					const SdDrawBatchPayload& payload = packet.commandBuffer.ReadPayload<SdDrawBatchPayload>(command);
					if (payload.batchIndex >= packet.batches.size() || payload.batchIndex < 0)
						continue;
					const SdRenderBatch& batch = packet.batches[payload.batchIndex];
					if (batch.indexCount <= 0)
						continue;
					TextureEntry* texture = TryGetTexture(batch.texture);
					if (!texture)
						continue;
					encoder.SetResourceSet(0, texture->resourceSet);
					encoder.DrawIndexed(batch.indexCount, 1, batch.indexOffset, 0, 0);
				}
				else if (command.kind == SdRenderCommandKind::PushClipRect)
				{
					const SdPushClipPayload& payload = packet.commandBuffer.ReadPayload<SdPushClipPayload>(command);
					if (payload.kind != SdClipKind::Rect)
						continue;
					const Rhi::SdRectI scissor = payload.rect.Width() > 0.0f && payload.rect.Height() > 0.0f
						? makeScissor(payload.rect)
						: Rhi::SdRectI{};
					clipStack.push_back(scissor);
					applyScissor(scissor);
				}
				else if (command.kind == SdRenderCommandKind::PopClip)
				{
					if (!clipStack.empty())
						clipStack.pop_back();
					applyScissor(clipStack.empty() ? fullScissor : clipStack.back());
				}
				else if (command.kind == SdRenderCommandKind::BeginRenderLayer)
				{
					const SdBeginRenderLayerPayload& payload = packet.commandBuffer.ReadPayload<SdBeginRenderLayerPayload>(command);
					renderLayers.BeginRenderLayer(payload);
					BindPipeline(frameInfo.displaySize);
					applyScissor(currentScissor);
				}
				else if (command.kind == SdRenderCommandKind::EndRenderLayer)
				{
					const SdEndRenderLayerPayload& payload = packet.commandBuffer.ReadPayload<SdEndRenderLayerPayload>(command);
					renderLayers.EndRenderLayer(payload.layerId);
					BindPipeline(frameInfo.displaySize);
					applyScissor(currentScissor);
				}
				else if (command.kind == SdRenderCommandKind::ApplyEffect)
				{
					const SdApplyEffectPayload& payload = packet.commandBuffer.ReadPayload<SdApplyEffectPayload>(command);
					ISdEffector* effector = packet.effectRegistry ? packet.effectRegistry->FindMutable(payload.effect) : nullptr;
					if (!effector)
						continue;

					const SdEffectApplyContext context =
					{
						rhiDevice,
						encoder,
						frameInfo,
						payload,
						packet.ReadEffectParameterBytes(payload),
						packet,
						&renderLayers
					};
					effector->Apply(context);
					renderLayers.BindRenderLayerTarget(renderLayers.GetCurrentLayer(), false, {});
					BindPipeline(frameInfo.displaySize);
					applyScissor(currentScissor);
				}
			}

			encoder.EndRenderPass();
			rhiDevice.EndFrame(frameTarget.texture);
			clearFrameTarget = false;
		}
	};

	SdDx12Renderer::SdDx12Renderer()
		: impl(std::make_unique<Impl>())
	{
	}

	SdDx12Renderer::~SdDx12Renderer()
	{
		Shutdown();
	}

	bool SdDx12Renderer::Initialize(const Config& config)
	{
		return impl && impl->Initialize(config);
	}

	void SdDx12Renderer::Shutdown()
	{
		if (impl)
			impl->Shutdown();
	}

	bool SdDx12Renderer::Resize(SdUInt32 width, SdUInt32 height)
	{
		return impl && impl->ResizeSwapchain(width, height);
	}

	void SdDx12Renderer::BeginFrame(const std::array<float, 4>& clearColor)
	{
		if (!impl)
			return;
		impl->currentRenderTargetIndex = impl->swapchainFrameIndex;
		impl->frameClearColor = clearColor;
		impl->clearFrameTarget = true;
	}

	bool SdDx12Renderer::Present()
	{
		return impl && impl->Present();
	}

	bool SdDx12Renderer::IsOccluded() const noexcept
	{
		return impl && impl->swapchainOccluded;
	}

	bool SdDx12Renderer::SetRenderTargets(ID3D12Resource* const* renderTargets, UINT renderTargetCount, DXGI_FORMAT renderTargetFormat)
	{
		return impl && impl->SetRenderTargets(renderTargets, renderTargetCount, renderTargetFormat);
	}

	void SdDx12Renderer::ReleaseRenderTargets()
	{
		if (impl)
			impl->ReleaseRenderTargets();
	}

	void SdDx12Renderer::BeginFrame(UINT renderTargetIndex)
	{
		if (!impl)
			return;
		impl->currentRenderTargetIndex = renderTargetIndex;
		impl->clearFrameTarget = false;
	}

	void SdDx12Renderer::BeginFrame(UINT renderTargetIndex, const std::array<float, 4>& clearColor)
	{
		if (!impl)
			return;
		impl->currentRenderTargetIndex = renderTargetIndex;
		impl->frameClearColor = clearColor;
		impl->clearFrameTarget = true;
	}

	bool SdDx12Renderer::IsInitialized() const noexcept
	{
		return impl && impl->rhiDevice.IsInitialized() && !impl->frameRenderTargets.empty() && impl->pipeline.IsValid();
	}

	Rhi::ISdGpuDevice& SdDx12Renderer::GetRhiDeviceInterface() noexcept
	{
		assert(impl);
		return impl->rhiDevice;
	}

	const Rhi::ISdGpuDevice& SdDx12Renderer::GetRhiDeviceInterface() const noexcept
	{
		assert(impl);
		return impl->rhiDevice;
	}

	SdTextureHandle SdDx12Renderer::CreateTexture(const SdTextureDesc& desc)
	{
		if (!impl || desc.width == 0 || desc.height == 0)
			return {};
		return impl->CreateTexture(desc.width, desc.height, desc.pixels.empty() ? nullptr : desc.pixels.data());
	}

	void SdDx12Renderer::DestroyTexture(SdTextureHandle texture)
	{
		if (!impl)
			return;
		Impl::TextureEntry* entry = impl->TryGetTexture(texture);
		if (!entry)
			return;
		if (entry->resourceSet.IsValid())
			impl->rhiDevice.DestroyResourceSet(entry->resourceSet);
		if (entry->texture.IsValid())
			impl->rhiDevice.DestroyTexture(entry->texture);
		entry->resourceSet = {};
		entry->texture = {};
		entry->width = 0;
		entry->height = 0;
		entry->occupied = false;
		++entry->generation;
	}

	void SdDx12Renderer::Render(const SdRendererFrameInfo& frameInfo, const SdRenderPacket& packet)
	{
		if (impl)
			impl->Render(frameInfo, packet);
	}
}
