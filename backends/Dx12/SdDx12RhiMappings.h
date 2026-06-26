#pragma once

#include "Rhi/SdRhi.h"

#include <d3d12.h>
#include <dxgiformat.h>

namespace Sodium::Backends
{
	inline DXGI_FORMAT MapTextureFormat(Rhi::SdTextureFormat format) noexcept
	{
		switch (format)
		{
		case Rhi::SdTextureFormat::R8Unorm: return DXGI_FORMAT_R8_UNORM;
		case Rhi::SdTextureFormat::Rgba8Unorm: return DXGI_FORMAT_R8G8B8A8_UNORM;
		case Rhi::SdTextureFormat::Rgba8UnormSrgb: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		case Rhi::SdTextureFormat::Bgra8Unorm: return DXGI_FORMAT_B8G8R8A8_UNORM;
		case Rhi::SdTextureFormat::Bgra8UnormSrgb: return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
		case Rhi::SdTextureFormat::R16Float: return DXGI_FORMAT_R16_FLOAT;
		case Rhi::SdTextureFormat::Rgba16Float: return DXGI_FORMAT_R16G16B16A16_FLOAT;
		case Rhi::SdTextureFormat::Rgba32Float: return DXGI_FORMAT_R32G32B32A32_FLOAT;
		case Rhi::SdTextureFormat::Depth24Stencil8: return DXGI_FORMAT_D24_UNORM_S8_UINT;
		case Rhi::SdTextureFormat::Depth32Float: return DXGI_FORMAT_D32_FLOAT;
		case Rhi::SdTextureFormat::Unknown:
		default: return DXGI_FORMAT_UNKNOWN;
		}
	}

	inline Rhi::SdTextureFormat MapDxgiFormatToRhi(DXGI_FORMAT format) noexcept
	{
		switch (format)
		{
		case DXGI_FORMAT_R8_UNORM: return Rhi::SdTextureFormat::R8Unorm;
		case DXGI_FORMAT_R8G8B8A8_UNORM: return Rhi::SdTextureFormat::Rgba8Unorm;
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return Rhi::SdTextureFormat::Rgba8UnormSrgb;
		case DXGI_FORMAT_B8G8R8A8_UNORM: return Rhi::SdTextureFormat::Bgra8Unorm;
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return Rhi::SdTextureFormat::Bgra8UnormSrgb;
		case DXGI_FORMAT_R16_FLOAT: return Rhi::SdTextureFormat::R16Float;
		case DXGI_FORMAT_R16G16B16A16_FLOAT: return Rhi::SdTextureFormat::Rgba16Float;
		case DXGI_FORMAT_R32G32B32A32_FLOAT: return Rhi::SdTextureFormat::Rgba32Float;
		case DXGI_FORMAT_D24_UNORM_S8_UINT: return Rhi::SdTextureFormat::Depth24Stencil8;
		case DXGI_FORMAT_D32_FLOAT: return Rhi::SdTextureFormat::Depth32Float;
		default: return Rhi::SdTextureFormat::Unknown;
		}
	}

	inline D3D12_BLEND MapBlendFactor(Rhi::SdBlendFactor factor) noexcept
	{
		switch (factor)
		{
		case Rhi::SdBlendFactor::Zero: return D3D12_BLEND_ZERO;
		case Rhi::SdBlendFactor::SrcColor: return D3D12_BLEND_SRC_COLOR;
		case Rhi::SdBlendFactor::InvSrcColor: return D3D12_BLEND_INV_SRC_COLOR;
		case Rhi::SdBlendFactor::DstColor: return D3D12_BLEND_DEST_COLOR;
		case Rhi::SdBlendFactor::InvDstColor: return D3D12_BLEND_INV_DEST_COLOR;
		case Rhi::SdBlendFactor::DstAlpha: return D3D12_BLEND_DEST_ALPHA;
		case Rhi::SdBlendFactor::InvDstAlpha: return D3D12_BLEND_INV_DEST_ALPHA;
		case Rhi::SdBlendFactor::SrcAlpha: return D3D12_BLEND_SRC_ALPHA;
		case Rhi::SdBlendFactor::InvSrcAlpha: return D3D12_BLEND_INV_SRC_ALPHA;
		case Rhi::SdBlendFactor::One:
		default: return D3D12_BLEND_ONE;
		}
	}

	inline D3D12_BLEND_OP MapBlendOp(Rhi::SdBlendOp op) noexcept
	{
		switch (op)
		{
		case Rhi::SdBlendOp::Subtract: return D3D12_BLEND_OP_SUBTRACT;
		case Rhi::SdBlendOp::ReverseSubtract: return D3D12_BLEND_OP_REV_SUBTRACT;
		case Rhi::SdBlendOp::Min: return D3D12_BLEND_OP_MIN;
		case Rhi::SdBlendOp::Max: return D3D12_BLEND_OP_MAX;
		case Rhi::SdBlendOp::Add:
		default: return D3D12_BLEND_OP_ADD;
		}
	}

	inline D3D12_COMPARISON_FUNC MapCompare(Rhi::SdCompareOp op) noexcept
	{
		switch (op)
		{
		case Rhi::SdCompareOp::Never: return D3D12_COMPARISON_FUNC_NEVER;
		case Rhi::SdCompareOp::Less: return D3D12_COMPARISON_FUNC_LESS;
		case Rhi::SdCompareOp::LessEqual: return D3D12_COMPARISON_FUNC_LESS_EQUAL;
		case Rhi::SdCompareOp::Equal: return D3D12_COMPARISON_FUNC_EQUAL;
		case Rhi::SdCompareOp::GreaterEqual: return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
		case Rhi::SdCompareOp::Greater: return D3D12_COMPARISON_FUNC_GREATER;
		case Rhi::SdCompareOp::Always:
		default: return D3D12_COMPARISON_FUNC_ALWAYS;
		}
	}

	inline D3D12_FILTER MapFilter(const Rhi::SdSamplerDesc& desc) noexcept
	{
		const bool linear = desc.minFilter == Rhi::SdFilterMode::Linear
			&& desc.magFilter == Rhi::SdFilterMode::Linear
			&& desc.mipFilter == Rhi::SdFilterMode::Linear;
		return linear ? D3D12_FILTER_MIN_MAG_MIP_LINEAR : D3D12_FILTER_MIN_MAG_MIP_POINT;
	}

	inline D3D12_TEXTURE_ADDRESS_MODE MapAddress(Rhi::SdAddressMode mode) noexcept
	{
		switch (mode)
		{
		case Rhi::SdAddressMode::Repeat: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		case Rhi::SdAddressMode::Mirror: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
		case Rhi::SdAddressMode::Clamp:
		default: return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		}
	}

	inline DXGI_FORMAT MapVertexFormat(Rhi::SdVertexFormat format) noexcept
	{
		switch (format)
		{
		case Rhi::SdVertexFormat::Float2: return DXGI_FORMAT_R32G32_FLOAT;
		case Rhi::SdVertexFormat::Float3: return DXGI_FORMAT_R32G32B32_FLOAT;
		case Rhi::SdVertexFormat::Float4: return DXGI_FORMAT_R32G32B32A32_FLOAT;
		case Rhi::SdVertexFormat::UByte4Norm: return DXGI_FORMAT_R8G8B8A8_UNORM;
		default: return DXGI_FORMAT_UNKNOWN;
		}
	}

	inline D3D12_PRIMITIVE_TOPOLOGY MapTopology(Rhi::SdPrimitiveTopology topology) noexcept
	{
		switch (topology)
		{
		case Rhi::SdPrimitiveTopology::TriangleStrip: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
		case Rhi::SdPrimitiveTopology::LineList: return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
		case Rhi::SdPrimitiveTopology::LineStrip: return D3D_PRIMITIVE_TOPOLOGY_LINESTRIP;
		case Rhi::SdPrimitiveTopology::TriangleList:
		default: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		}
	}

	inline D3D12_PRIMITIVE_TOPOLOGY_TYPE MapTopologyType(Rhi::SdPrimitiveTopology topology) noexcept
	{
		switch (topology)
		{
		case Rhi::SdPrimitiveTopology::LineList:
		case Rhi::SdPrimitiveTopology::LineStrip:
			return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		case Rhi::SdPrimitiveTopology::TriangleList:
		case Rhi::SdPrimitiveTopology::TriangleStrip:
		default:
			return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		}
	}

	inline D3D12_DESCRIPTOR_RANGE_TYPE MapDescriptorRangeType(Rhi::SdShaderResourceType type) noexcept
	{
		switch (type)
		{
		case Rhi::SdShaderResourceType::Sampler: return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
		case Rhi::SdShaderResourceType::UniformBuffer: return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
		case Rhi::SdShaderResourceType::StorageBuffer:
		case Rhi::SdShaderResourceType::StorageTexture: return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		case Rhi::SdShaderResourceType::Texture2D:
		default: return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		}
	}

	inline D3D12_SHADER_VISIBILITY MapShaderVisibility(Rhi::SdShaderStageFlags stages) noexcept
	{
		const bool vertex = Rhi::SdHasFlag(stages, Rhi::SdShaderStageFlag::Vertex);
		const bool pixel = Rhi::SdHasFlag(stages, Rhi::SdShaderStageFlag::Pixel);
		if (vertex && !pixel)
			return D3D12_SHADER_VISIBILITY_VERTEX;
		if (pixel && !vertex)
			return D3D12_SHADER_VISIBILITY_PIXEL;
		return D3D12_SHADER_VISIBILITY_ALL;
	}
}
