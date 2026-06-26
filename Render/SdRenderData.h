#pragma once

#include <Effects/SdEffectTypes.hpp>
#include <Core/SdCore.h>
#include <Core/SdText.h>
#include <Rhi/SdRhi.h>

#include <cmath>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace Sodium
{
	class SdEffectRegistry;

	namespace Detail
	{
		constexpr SdUInt32 AlignUp(SdUInt32 value, SdUInt32 alignment) noexcept
		{
			return (value + alignment - 1u) & ~(alignment - 1u);
		}

		template<std::size_t N>
		constexpr std::array<SdVec2, N> CalcFastArcSamples()
		{
			std::array<SdVec2, N> samples = {};
			for (std::size_t i = 0; i < N; ++i)
			{
				const float angle = (static_cast<float>(i) / static_cast<float>(samples.size())) * SdPi * 2.0f;
				samples[i] = { std::cos(angle), std::sin(angle) };
			}
			return samples;
		}
	}

	struct SdVertex final
	{
		SdVec2 position = {};
		SdVec2 uv = {};
		SdUInt32 color = SdColorWhite.Pack();
	};

	enum class SdRenderCommandKind : SdUInt8
	{
		DrawBatch,

		PushClipRect,
		PopClip,

		BeginRenderLayer,
		EndRenderLayer,
		ApplyEffect,

		None
	};

	enum class SdClipKind : SdUInt8
	{
		Rect,
		RoundedRect,
		Mask
	};

	enum class SdRenderLayerUsage : SdUInt8
	{
		Generic,
		OpacityGroup,
		EffectInput,
		BackdropCapture,
		Mask
	};

	inline constexpr SdRenderLayerId SdRootRenderLayerId = 0;
	inline constexpr SdRenderLayerId SdInvalidRenderLayerId = SdInvalidIndex<SdRenderLayerId>;

	struct SdRenderCommand final
	{
		SdRenderCommandKind kind = SdRenderCommandKind::None;
		SdUInt32 payloadOffset = 0;
		SdUInt32 payloadSize = 0;
	};

	class SdRenderCommandBuffer final
	{
	public:
		template<class TPayload>
		void Push(SdRenderCommandKind kind, const TPayload& payload)
		{
			static_assert(std::is_trivially_copyable_v<TPayload>);

			const SdUInt32 alignment = static_cast<SdUInt32>(alignof(TPayload));
			const SdUInt32 offset = Detail::AlignUp(static_cast<SdUInt32>(payloads.size()), alignment);

			if (payloads.size() < offset)
				payloads.resize(offset);

			const SdUInt32 size = static_cast<SdUInt32>(sizeof(TPayload));
			payloads.resize(offset + size);
			std::memcpy(payloads.data() + offset, &payload, size);

			commands.push_back({
				.kind = kind,
				.payloadOffset = offset,
				.payloadSize = size
				});
		}
		void Push(SdRenderCommandKind kind)
		{
			commands.push_back({
				.kind = kind,
				.payloadOffset = 0,
				.payloadSize = 0
				});
		}

		template<class TPayload>
		[[nodiscard]] TPayload ReadPayload(const SdRenderCommand& command) const
		{
			static_assert(std::is_trivially_copyable_v<TPayload>, SODIUM_STRING("Render command payloads must be trivially copyable."));
			assert(command.payloadSize == sizeof(TPayload));
			assert(command.payloadOffset + command.payloadSize <= payloads.size());
			TPayload payload = {};
			if (command.payloadSize == sizeof(TPayload) && command.payloadOffset <= payloads.size() && command.payloadSize <= payloads.size() - command.payloadOffset)
				std::memcpy(&payload, payloads.data() + command.payloadOffset, sizeof(TPayload));
			return payload;
		}

		std::span<const SdRenderCommand> GetCommands() const noexcept { return commands; }
		std::span<const std::byte> GetPayloads() const noexcept { return payloads; }

		void Clear() 
		{
			commands.clear();
			payloads.clear();
		}
	private:
		std::vector<SdRenderCommand> commands;
		std::vector<std::byte> payloads;
	};

	struct SdDrawBatchPayload final
	{
		SdUInt32 batchIndex = std::numeric_limits<SdUInt32>::max();
	};

	struct SdPushClipPayload final
	{
		SdClipKind kind = SdClipKind::Rect;
		SdRect rect = {};
		SdCornerRadii cornerRadii = {};
		SdUInt32 maskResourceId = 0;
	};

	struct SdPopClipPayload final
	{
	};

	struct SdBeginRenderLayerPayload final
	{
		SdRenderLayerId layerId = SdInvalidRenderLayerId;
		SdRenderLayerUsage usage = SdRenderLayerUsage::Generic;
		SdRect bounds = {};
		Rhi::SdTextureFormat format = Rhi::SdTextureFormat::Unknown;
		SdColorLinear clearColor = {};
		bool clear = true;
	};

	struct SdEndRenderLayerPayload final
	{
		SdRenderLayerId layerId = SdInvalidRenderLayerId;
	};

	struct SdEffectParameterRange final
	{
		SdUInt32 offset = 0;
		SdUInt32 size = 0;
	};

	struct SdApplyEffectPayload final
	{
		SdEffectHandle effect = {};
		SdRenderLayerId sourceLayer = SdInvalidRenderLayerId;
		SdRenderLayerId targetLayer = SdRootRenderLayerId;
		SdRenderLayerId backdropLayer = SdInvalidRenderLayerId;
		SdRenderLayerId maskLayer = SdInvalidRenderLayerId;
		SdRect sourceBounds = {};
		SdRect expandedBounds = {};
		SdRect clipRect = {};
		SdUInt32 parameterOffset = 0;
		SdUInt32 parameterSize = 0;
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

	struct SdUploadRequest final
	{
		SdTextureHandle texture = {};
		SdUInt32 width = 0;
		SdUInt32 height = 0;
		std::vector<std::byte> pixels = {};
	};

	struct SdRenderPacket final
	{
		const SdRenderCommandBuffer& commandBuffer;
		std::span<const SdRenderCommand> commands = {};
		std::span<const SdVertex> vertices = {};
		std::span<const SdUInt32> indices = {};
		std::span<const SdRenderBatch> batches = {};
		std::span<const std::byte> effectParameters = {};
		std::span<const SdUploadRequest> resourceUpdates = {};
		const SdEffectRegistry* effectRegistry = nullptr;
		SdUInt32 packetVersion = 0;

		std::span<const std::byte> ReadEffectParameterBytes(const SdApplyEffectPayload& payload) const noexcept
		{
			if (payload.parameterSize == 0)
				return {};
			if (payload.parameterOffset > effectParameters.size())
				return {};
			if (payload.parameterSize > effectParameters.size() - payload.parameterOffset)
				return {};
			return effectParameters.subspan(payload.parameterOffset, payload.parameterSize);
		}

		template<class TParameters>
		const TParameters* TryReadEffectParameters(const SdApplyEffectPayload& payload) const noexcept
		{
			static_assert(std::is_trivially_copyable_v<TParameters>, SODIUM_STRING("Effect command parameters must be trivially copyable."));
			if (payload.parameterSize != sizeof(TParameters))
				return nullptr;
			const std::span<const std::byte> bytes = ReadEffectParameterBytes(payload);
			if (bytes.size_bytes() != sizeof(TParameters))
				return nullptr;
			return std::launder(reinterpret_cast<const TParameters*>(bytes.data()));
		}
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
		SdBuiltInEffectHandles builtInEffects = {};
		SdUInt32 flags = UseAntiAliasing;

		SdRenderSharedData() 
		{
			fastArcSamples = Detail::CalcFastArcSamples<32>();
		}
	};

	struct SdRenderData final
	{
		SdRenderCommandBuffer commandBuffer = {};

		std::vector<SdUploadRequest> uploadRequests = {};
		std::vector<std::byte> effectParameters = {};

		std::vector<SdVertex> vertices = {};
		std::vector<SdUInt32> indices = {};
		std::vector<SdRenderBatch> batches = {};

		void Clear()
		{
			commandBuffer.Clear();
			vertices.clear();
			indices.clear();
			batches.clear();
			effectParameters.clear();
			uploadRequests.clear();
		}

		void AssignPacket(const SdRenderPacket& packet)
		{
			vertices.assign(packet.vertices.begin(), packet.vertices.end());
			indices.assign(packet.indices.begin(), packet.indices.end());
			batches.assign(packet.batches.begin(), packet.batches.end());
			effectParameters.assign(packet.effectParameters.begin(), packet.effectParameters.end());
			uploadRequests.assign(packet.resourceUpdates.begin(), packet.resourceUpdates.end());
		}

		template<class TParameters>
		SdEffectParameterRange PushEffectParameters(const TParameters& parameters)
		{
			static_assert(std::is_trivially_copyable_v<TParameters>, SODIUM_STRING("Effect command parameters must be trivially copyable."));

			const SdUInt32 alignment = static_cast<SdUInt32>(alignof(TParameters));
			const SdUInt32 offset = Detail::AlignUp(static_cast<SdUInt32>(effectParameters.size()), alignment);
			if (effectParameters.size() < offset)
				effectParameters.resize(offset);

			const SdUInt32 size = static_cast<SdUInt32>(sizeof(TParameters));
			effectParameters.resize(offset + size);
			std::memcpy(effectParameters.data() + offset, &parameters, size);
			return { offset, size };
		}

		SdEffectParameterRange PushEffectParameterBytes(std::span<const std::byte> parameters, SdUInt32 alignment = static_cast<SdUInt32>(alignof(std::max_align_t)))
		{
			if (parameters.empty())
				return {};

			const SdUInt32 offset = Detail::AlignUp(static_cast<SdUInt32>(effectParameters.size()), alignment);
			if (effectParameters.size() < offset)
				effectParameters.resize(offset);

			const SdUInt32 size = static_cast<SdUInt32>(parameters.size_bytes());
			effectParameters.resize(offset + size);
			std::memcpy(effectParameters.data() + offset, parameters.data(), size);
			return { offset, size };
		}

		SdRenderPacket BuildPacket(SdUInt32 packetVersion = 0, const SdEffectRegistry* effectRegistry = nullptr) const noexcept
		{
			return {
				commandBuffer,
				commandBuffer.GetCommands(),
				std::span<const SdVertex>(vertices.data(), vertices.size()),
				std::span<const SdUInt32>(indices.data(), indices.size()),
				std::span<const SdRenderBatch>(batches.data(), batches.size()),
				std::span<const std::byte>(effectParameters.data(), effectParameters.size()),
				std::span<const SdUploadRequest>(uploadRequests.data(), uploadRequests.size()),
				effectRegistry,
				packetVersion
			};
		}
	};

	using SdRenderPacketVersion = SdUInt32;
	using SdDrawPacket = SdRenderPacket;
	using SdDrawData = SdRenderData;
}
