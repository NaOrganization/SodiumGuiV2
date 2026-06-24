#pragma once

#include "Rhi/SdRhi.h"
#include "Rhi/SdTransientTexturePool.hpp"

#include <functional>
#include <vector>

namespace Sodium::Rhi
{
	enum class SdResourceState : SdUInt16
	{
		Unknown,
		RenderTarget,
		ShaderRead,
		CopySource,
		CopyDest,
		DepthWrite,
		DepthRead,
		Present
	};

	struct SdRenderGraphTexture final
	{
		SdUInt32 id = 0;

		constexpr bool IsValid() const noexcept { return id != 0; }
		friend constexpr bool operator==(const SdRenderGraphTexture&, const SdRenderGraphTexture&) = default;
	};

	struct SdRenderGraphTextureDesc final
	{
		SdUInt32 width = 1;
		SdUInt32 height = 1;
		SdTextureFormat format = SdTextureFormat::Rgba8Unorm;
		SdTextureUsageFlags usage = SdTextureUsage::RenderTarget | SdTextureUsage::ShaderRead;
		SdUInt32 sampleCount = 1;
		bool transient = true;
		SdUtf8StringView debugName = {};
	};

	enum class SdRenderGraphPassType : SdUInt8
	{
		Raster,
		Fullscreen,
		Copy,
		Compute
	};

	struct SdRenderGraphPassHandle final
	{
		SdUInt32 id = 0;

		constexpr bool IsValid() const noexcept { return id != 0; }
		friend constexpr bool operator==(const SdRenderGraphPassHandle&, const SdRenderGraphPassHandle&) = default;
	};

	struct SdRenderGraphPassDesc final
	{
		SdUtf8StringView name = {};
		SdRenderGraphPassType type = SdRenderGraphPassType::Raster;
	};

	struct SdResourceTransition final
	{
		SdRenderGraphTexture texture = {};
		SdResourceState before = SdResourceState::Unknown;
		SdResourceState after = SdResourceState::Unknown;
	};

	struct SdCompiledRenderGraphPass final
	{
		SdRenderGraphPassHandle pass = {};
		std::vector<SdResourceTransition> transitions = {};
	};

	struct SdRenderGraphCompileResult final
	{
		std::vector<SdCompiledRenderGraphPass> passes = {};
		bool success = false;
		SdUtf8StringView errorMessage = {};
	};

	class SdRenderGraph final
	{
	public:
		using PassCallback = std::function<void(ISdCommandEncoder& encoder)>;

	private:
		struct TextureResource final
		{
			SdRenderGraphTextureDesc desc = {};
			SdTextureHandle importedTexture = {};
			SdTextureHandle physicalTexture = {};
			bool imported = false;
			bool written = false;
			SdResourceState state = SdResourceState::Unknown;
		};

		struct PassResource final
		{
			SdRenderGraphPassDesc desc = {};
			std::vector<SdRenderGraphTexture> reads = {};
			std::vector<SdRenderGraphTexture> writes = {};
			PassCallback callback = {};
		};

		SdTransientTexturePool* texturePool = nullptr;
		std::vector<TextureResource> textures = {};
		std::vector<PassResource> passes = {};
		SdRenderGraphCompileResult compileResult = {};

		TextureResource* TryGetTexture(SdRenderGraphTexture texture) noexcept
		{
			if (!texture.IsValid() || texture.id > textures.size())
				return nullptr;
			return &textures[texture.id - 1];
		}

		const TextureResource* TryGetTexture(SdRenderGraphTexture texture) const noexcept
		{
			if (!texture.IsValid() || texture.id > textures.size())
				return nullptr;
			return &textures[texture.id - 1];
		}

		PassResource* TryGetPass(SdRenderGraphPassHandle pass) noexcept
		{
			if (!pass.IsValid() || pass.id > passes.size())
				return nullptr;
			return &passes[pass.id - 1];
		}

		static bool ContainsTexture(const std::vector<SdRenderGraphTexture>& textures, SdRenderGraphTexture texture)
		{
			for (SdRenderGraphTexture current : textures)
			{
				if (current == texture)
					return true;
			}
			return false;
		}

		void AddTransition(std::vector<SdResourceTransition>& transitions, SdRenderGraphTexture texture, SdResourceState after)
		{
			TextureResource* resource = TryGetTexture(texture);
			if (!resource)
				return;
			if (resource->state == after)
				return;
			transitions.push_back({ texture, resource->state, after });
			resource->state = after;
		}

	public:
		SdRenderGraph() = default;
		explicit SdRenderGraph(SdTransientTexturePool& pool)
			: texturePool(&pool) {}

		void SetTransientTexturePool(SdTransientTexturePool* pool) noexcept
		{
			texturePool = pool;
		}

		void Reset()
		{
			for (TextureResource& texture : textures)
			{
				if (!texture.imported && texture.physicalTexture.IsValid() && texturePool)
					texturePool->Release(texture.physicalTexture);
			}
			textures.clear();
			passes.clear();
			compileResult = {};
		}

		SdRenderGraphTexture CreateTexture(const SdRenderGraphTextureDesc& desc)
		{
			TextureResource resource = {};
			resource.desc = desc;
			resource.desc.transient = true;
			textures.push_back(resource);
			return { static_cast<SdUInt32>(textures.size()) };
		}

		SdRenderGraphTexture ImportTexture(SdTextureHandle texture, SdUtf8StringView debugName = {})
		{
			if (!texture.IsValid())
				return {};

			TextureResource resource = {};
			resource.importedTexture = texture;
			resource.physicalTexture = texture;
			resource.imported = true;
			resource.written = true;
			resource.desc.transient = false;
			resource.desc.debugName = debugName;
			textures.push_back(resource);
			return { static_cast<SdUInt32>(textures.size()) };
		}

		SdRenderGraphPassHandle AddPass(const SdRenderGraphPassDesc& desc)
		{
			PassResource pass = {};
			pass.desc = desc;
			passes.push_back(std::move(pass));
			return { static_cast<SdUInt32>(passes.size()) };
		}

		void ReadTexture(SdRenderGraphPassHandle pass, SdRenderGraphTexture texture)
		{
			if (PassResource* passResource = TryGetPass(pass))
			{
				if (!ContainsTexture(passResource->reads, texture))
					passResource->reads.push_back(texture);
			}
		}

		void WriteTexture(SdRenderGraphPassHandle pass, SdRenderGraphTexture texture)
		{
			if (PassResource* passResource = TryGetPass(pass))
			{
				if (!ContainsTexture(passResource->writes, texture))
					passResource->writes.push_back(texture);
			}
		}

		void SetPassCallback(SdRenderGraphPassHandle pass, PassCallback callback)
		{
			if (PassResource* passResource = TryGetPass(pass))
				passResource->callback = std::move(callback);
		}

		bool Compile(SdRenderGraphCompileResult& outResult)
		{
			compileResult = {};

			for (TextureResource& texture : textures)
			{
				texture.written = texture.imported;
				texture.state = SdResourceState::Unknown;
				if (!texture.imported && texture.desc.transient && !texture.physicalTexture.IsValid() && texturePool)
				{
					SdTextureDesc desc = {};
					desc.width = texture.desc.width;
					desc.height = texture.desc.height;
					desc.format = texture.desc.format;
					desc.usage = texture.desc.usage;
					desc.sampleCount = texture.desc.sampleCount;
					desc.isTransient = true;
					desc.debugName = texture.desc.debugName;
					texture.physicalTexture = texturePool->Acquire(desc);
				}
			}

			for (SdUInt32 passIndex = 0; passIndex < passes.size(); ++passIndex)
			{
				const PassResource& pass = passes[passIndex];
				SdCompiledRenderGraphPass compiledPass = {};
				compiledPass.pass = { passIndex + 1 };

				for (SdRenderGraphTexture texture : pass.reads)
				{
					TextureResource* resource = TryGetTexture(texture);
					if (!resource)
					{
						compileResult.errorMessage = "RenderGraph pass reads an invalid texture.";
						outResult = compileResult;
						return false;
					}
					if (ContainsTexture(pass.writes, texture))
					{
						compileResult.errorMessage = "RenderGraph pass reads and writes the same texture.";
						outResult = compileResult;
						return false;
					}
					if (!resource->written)
					{
						compileResult.errorMessage = "RenderGraph pass reads a texture before it is written.";
						outResult = compileResult;
						return false;
					}
					if (!SdHasFlag(resource->desc.usage, SdTextureUsage::ShaderRead) && !resource->imported)
					{
						compileResult.errorMessage = "RenderGraph pass reads a texture without ShaderRead usage.";
						outResult = compileResult;
						return false;
					}
					AddTransition(compiledPass.transitions, texture, SdResourceState::ShaderRead);
				}

				for (SdRenderGraphTexture texture : pass.writes)
				{
					TextureResource* resource = TryGetTexture(texture);
					if (!resource)
					{
						compileResult.errorMessage = "RenderGraph pass writes an invalid texture.";
						outResult = compileResult;
						return false;
					}
					if (!SdHasFlag(resource->desc.usage, SdTextureUsage::RenderTarget) && !resource->imported)
					{
						compileResult.errorMessage = "RenderGraph pass writes a texture without RenderTarget usage.";
						outResult = compileResult;
						return false;
					}
					AddTransition(compiledPass.transitions, texture, SdResourceState::RenderTarget);
					resource->written = true;
				}

				compileResult.passes.push_back(std::move(compiledPass));
			}

			compileResult.success = true;
			outResult = compileResult;
			return true;
		}

		bool Compile()
		{
			SdRenderGraphCompileResult ignored = {};
			return Compile(ignored);
		}

		void Execute(ISdCommandEncoder& encoder)
		{
			if (!compileResult.success)
			{
				if (!Compile())
					return;
			}

			for (const SdCompiledRenderGraphPass& compiledPass : compileResult.passes)
			{
				if (!compiledPass.pass.IsValid() || compiledPass.pass.id > passes.size())
					continue;
				PassResource& pass = passes[compiledPass.pass.id - 1];
				if (pass.callback)
					pass.callback(encoder);
			}
		}

		SdTextureHandle GetPhysicalTexture(SdRenderGraphTexture texture) const noexcept
		{
			const TextureResource* resource = TryGetTexture(texture);
			return resource ? resource->physicalTexture : SdTextureHandle{};
		}

		const SdRenderGraphCompileResult& GetCompileResult() const noexcept
		{
			return compileResult;
		}
	};
}
