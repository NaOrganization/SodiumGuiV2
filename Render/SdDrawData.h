#pragma once

#include "Core/SdCore.h"
#include "Core/SdText.h"

#include <cmath>
#include <unordered_map>
#include <vector>

namespace Sodium
{
	namespace Backends
	{
		struct SdCachedParagraph;
	}

	struct SdVertex final
	{
		SdVec2 position = {};
		SdVec2 uv = {};
		SdUInt32 color = SdColorWhite.Pack();
	};

	struct SdRenderBatch final
	{
		SdUInt32 vertexOffset = 0;
		SdUInt32 vertexCount = 0;
		SdUInt32 indexOffset = 0;
		SdUInt32 indexCount = 0;
		SdTextureHandle texture = {};
		SdRect clipRect = {};
	};

	struct SdReferencedParagraphDraw final
	{
		const Backends::SdCachedParagraph* cache = nullptr;
		SdUInt32 batchIndex = 0;
		SdVec2 translation = {};
		SdRect clipRect = {};
	};

	enum class SdDrawCommandKind : SdUInt8
	{
		OwnedBatch,
		ReferencedParagraphBatch
	};

	struct SdDrawCommand final
	{
		SdDrawCommandKind kind = SdDrawCommandKind::OwnedBatch;
		SdUInt32 index = 0;
	};

	struct SdUploadRequest final
	{
		SdTextureHandle texture = {};
		SdUInt32 width = 0;
		SdUInt32 height = 0;
		std::vector<std::byte> pixels = {};
	};

	// Resource updates are frame-local commands carried to the renderer.
	// The renderer may copy/upload the pixel payload during Render; the CPU
	// storage belongs to the draw packet and is not valid after the frame.
	struct SdResourceUpdate final
	{
		SdUploadRequest upload = {};
	};

	// SdDrawPacket is a borrowed frame view over SdDrawData. Backends must
	// consume it synchronously inside Render and not retain its spans.
	struct SdDrawPacket final
	{
		std::span<const SdDrawCommand> commands = {};
		std::span<const SdVertex> vertices = {};
		std::span<const SdUInt32> indices = {};
		std::span<const SdRenderBatch> batches = {};
		std::span<const SdReferencedParagraphDraw> referencedParagraphs = {};
		std::span<const SdUploadRequest> resourceUpdates = {};
		SdUInt32 packetVersion = 0;
	};

	struct SdRendererFrameInfo final
	{
		SdVec2 displaySize = {};
	};

	// Source description for creating a backend-owned texture. A valid
	// SdTextureHandle is only meaningful for the renderer that created it.
	struct SdTextureDesc final
	{
		SdUInt32 width = 0;
		SdUInt32 height = 0;
		std::span<const std::byte> pixels = {};
	};

	struct SdGlyphKey final
	{
		SdFontHandle font = {};
		SdUInt32 glyphIndex = 0;
		SdUInt32 pixelSize = 0;
		bool colored = false;

		friend bool operator==(const SdGlyphKey&, const SdGlyphKey&) = default;
	};

	struct SdGlyphKeyHash final
	{
		SdSize operator()(const SdGlyphKey& key) const noexcept
		{
			SdUInt64 value = key.font.index;
			value = (value * 16777619ull) ^ key.font.generation;
			value = (value * 16777619ull) ^ key.glyphIndex;
			value = (value * 16777619ull) ^ key.pixelSize;
			value = (value * 16777619ull) ^ static_cast<SdUInt64>(key.colored);
			return static_cast<SdSize>(value);
		}
	};

	struct SdRenderSharedData final
	{
		static constexpr SdUInt32 BakedLineTextureCount = 64;

		enum Flag : SdUInt32
		{
			None = 0,
			UseAntiAliasing = 1 << 0
		};

		ISdFontBackend* fontBackend = nullptr;
		SdTextStyle defaultTextStyle = {};
		SdTextureHandle whiteTexture = {};
		SdRect whitePixelUv = { 0.0f, 0.0f, 1.0f, 1.0f };
		std::array<SdVec4, BakedLineTextureCount> bakedLineUvs = {};
		std::array<SdVec2, 32> fastArcSamples = {};
		SdUInt32 flags = UseAntiAliasing;

		SdRenderSharedData()
		{
			for (SdSize index = 0; index < fastArcSamples.size(); ++index)
			{
				const float angle = (static_cast<float>(index) / static_cast<float>(fastArcSamples.size())) * SdPi * 2.0f;
				fastArcSamples[index] = { std::cos(angle), std::sin(angle) };
			}
		}
	};

	struct SdDrawData final
	{
		std::vector<SdVertex> vertices = {};
		std::vector<SdUInt32> indices = {};
		std::vector<SdRenderBatch> batches = {};
		std::vector<SdReferencedParagraphDraw> referencedParagraphs = {};
		std::vector<SdDrawCommand> commands = {};
		std::vector<SdUploadRequest> uploads = {};

		void Clear()
		{
			vertices.clear();
			indices.clear();
			batches.clear();
			referencedParagraphs.clear();
			commands.clear();
			uploads.clear();
		}

		void AssignPacket(const SdDrawPacket& packet)
		{
			vertices.assign(packet.vertices.begin(), packet.vertices.end());
			indices.assign(packet.indices.begin(), packet.indices.end());
			batches.assign(packet.batches.begin(), packet.batches.end());
			referencedParagraphs.assign(packet.referencedParagraphs.begin(), packet.referencedParagraphs.end());
			commands.assign(packet.commands.begin(), packet.commands.end());
			uploads.assign(packet.resourceUpdates.begin(), packet.resourceUpdates.end());
		}

		SdDrawPacket BuildPacket(SdUInt32 packetVersion = 0) const noexcept
		{
			return {
				std::span<const SdDrawCommand>(commands.data(), commands.size()),
				std::span<const SdVertex>(vertices.data(), vertices.size()),
				std::span<const SdUInt32>(indices.data(), indices.size()),
				std::span<const SdRenderBatch>(batches.data(), batches.size()),
				std::span<const SdReferencedParagraphDraw>(referencedParagraphs.data(), referencedParagraphs.size()),
				std::span<const SdUploadRequest>(uploads.data(), uploads.size()),
				packetVersion
			};
		}
	};

	using SdRenderData = SdDrawData;
	using SdDrawPacketVersion = SdUInt32;
}
