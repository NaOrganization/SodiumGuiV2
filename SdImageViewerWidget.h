#pragma once

#include "SdBasicWidgets.h"

#include <algorithm>

namespace Sodium
{
	struct SdImageViewerOptions final
	{
		SdVec2 thumbnailSize = { 128.0f, 128.0f };
		SdVec2 previewSize = { 256.0f, 256.0f };
		SdVec2 previewOffset = { 18.0f, 18.0f };
		SdRect uvRect = { 0.0f, 0.0f, 1.0f, 1.0f };
		float initialZoom = 3.0f;
		float minZoom = 1.0f;
		float maxZoom = 16.0f;
		float zoomStep = 0.5f;
	};

	class SdImageViewer final : public SdWidgetTag
	{
	public:
		struct State
		{
			SdTextureHandle texture = {};
			SdImageViewerOptions options = {};
			SdVec2 mousePosition = {};
			SdVec2 localUv = {};
			SdRect zoomUvRect = {};
			float zoom = 3.0f;
			bool initialized = false;
			bool hovered = false;
		};

		void OnUpdate(SdUpdateContext& context, SdTextureHandle texture)
		{
			OnUpdate(context, texture, SdImageViewerOptions{});
		}

		void OnUpdate(SdUpdateContext& context, SdTextureHandle texture, SdVec2 thumbnailSize)
		{
			SdImageViewerOptions options = {};
			options.thumbnailSize = thumbnailSize;
			OnUpdate(context, texture, options);
		}

		void OnUpdate(SdUpdateContext& context, SdTextureHandle texture, const SdImageViewerOptions& options)
		{
			State& state = context.State<State>();
			const bool textureChanged = state.texture != texture;
			state.options = NormalizeOptions(options);
			context.widgetState.styleClass = SdStyleWidgetClass::ImageViewer;
			state.mousePosition = context.input.GetMousePosition();
			state.hovered = context.widgetState.inputEnabled
				&& context.IsHovered()
				&& texture.IsValid();
			context.widgetState.layerPriority = state.hovered ? SdLayerPriority::Floating : SdLayerPriority::Content;

			if (!state.initialized || textureChanged)
			{
				state.zoom = ClampZoom(state.options.initialZoom, state.options);
				state.initialized = true;
			}
			else
			{
				state.zoom = ClampZoom(state.zoom, state.options);
			}
			state.texture = texture;

			if (state.hovered)
			{
				const float wheel = context.input.GetMouseWheelDelta().y;
				if (wheel != 0.0f)
					state.zoom = ClampZoom(state.zoom + wheel * state.options.zoomStep, state.options);
			}

			state.localUv = BuildLocalUv(context.widgetState.lastRect, state.mousePosition);
			state.zoomUvRect = BuildZoomUvRect(state.options.uvRect, state.localUv, state.zoom);
			zoom = state.zoom;
		}

		void OnLayout(SdLayoutContext& context)
		{
			const State& state = context.State<State>();
			context.SetDesiredSize(state.options.thumbnailSize);
		}

		void OnPaint(SdPaintContext& context)
		{
			const State& state = context.State<State>();
			const SdColor frameFill = state.hovered ? SdColor{ 34, 45, 58, 255 } : SdColor{ 26, 34, 44, 255 };
			context.renderList.AddRectFilled(context.animatedRect, SdApplyOpacity(frameFill, context.opacity), context.clipRect, 4.0f);

			const SdRect imageRect = InsetRect(context.animatedRect, 2.0f);
			if (state.texture.IsValid())
				context.renderList.AddImage(state.texture, imageRect, state.options.uvRect, SdApplyOpacity(SdColorWhite, context.opacity), context.clipRect);

			context.renderList.AddRect(context.animatedRect, SdApplyOpacity({ 95, 115, 138, 255 }, context.opacity), context.clipRect, 1.0f, 4.0f);

			if (!state.hovered)
				return;

			const SdRect hoverSourceRect = BuildSourceMarkerRect(imageRect, state.options.uvRect, state.zoomUvRect);
			context.renderList.AddRect(hoverSourceRect, SdApplyOpacity({ 118, 196, 255, 255 }, context.opacity), context.clipRect, 1.0f);

			const SdRect previewRect = BuildPreviewRect(state.mousePosition, state.options.previewSize, state.options.previewOffset, context.instance.GetDisplaySize());
			const SdRect previewImageRect = InsetRect(previewRect, 4.0f);
			context.renderList.AddRectFilled(previewRect, SdApplyOpacity({ 12, 18, 25, 245 }, context.opacity), context.clipRect, 5.0f);
			context.renderList.AddImage(state.texture, previewImageRect, state.zoomUvRect, SdApplyOpacity(SdColorWhite, context.opacity), context.clipRect);
			context.renderList.AddRect(previewRect, SdApplyOpacity({ 118, 196, 255, 255 }, context.opacity), context.clipRect, 1.0f, 5.0f);
		}

		float GetZoom() const noexcept { return zoom; }

	private:
		float zoom = 3.0f;

		static SdImageViewerOptions NormalizeOptions(SdImageViewerOptions options)
		{
			options.thumbnailSize.x = std::max(1.0f, options.thumbnailSize.x);
			options.thumbnailSize.y = std::max(1.0f, options.thumbnailSize.y);
			options.previewSize.x = std::max(16.0f, options.previewSize.x);
			options.previewSize.y = std::max(16.0f, options.previewSize.y);
			options.minZoom = std::max(1.0f, options.minZoom);
			options.maxZoom = std::max(options.minZoom, options.maxZoom);
			options.zoomStep = std::max(0.01f, options.zoomStep);
			return options;
		}

		static float ClampZoom(float value, const SdImageViewerOptions& options)
		{
			return std::clamp(value, options.minZoom, options.maxZoom);
		}

		static SdVec2 BuildLocalUv(const SdRect& rect, const SdVec2& mousePosition)
		{
			const float width = std::max(1.0f, rect.Width());
			const float height = std::max(1.0f, rect.Height());
			return {
				std::clamp((mousePosition.x - rect.min.x) / width, 0.0f, 1.0f),
				std::clamp((mousePosition.y - rect.min.y) / height, 0.0f, 1.0f)
			};
		}

		static SdRect BuildZoomUvRect(const SdRect& sourceUvRect, const SdVec2& localUv, float zoom)
		{
			const SdVec2 uvSize = {
				std::max(0.0001f, sourceUvRect.Width()),
				std::max(0.0001f, sourceUvRect.Height())
			};
			const SdVec2 uvCenter = {
				sourceUvRect.min.x + uvSize.x * localUv.x,
				sourceUvRect.min.y + uvSize.y * localUv.y
			};
			const SdVec2 halfUvSize = {
				uvSize.x / std::max(1.0f, zoom) * 0.5f,
				uvSize.y / std::max(1.0f, zoom) * 0.5f
			};

			SdRect result = {
				uvCenter.x - halfUvSize.x,
				uvCenter.y - halfUvSize.y,
				uvCenter.x + halfUvSize.x,
				uvCenter.y + halfUvSize.y
			};

			if (result.min.x < sourceUvRect.min.x)
			{
				result.max.x += sourceUvRect.min.x - result.min.x;
				result.min.x = sourceUvRect.min.x;
			}
			if (result.max.x > sourceUvRect.max.x)
			{
				result.min.x -= result.max.x - sourceUvRect.max.x;
				result.max.x = sourceUvRect.max.x;
			}
			if (result.min.y < sourceUvRect.min.y)
			{
				result.max.y += sourceUvRect.min.y - result.min.y;
				result.min.y = sourceUvRect.min.y;
			}
			if (result.max.y > sourceUvRect.max.y)
			{
				result.min.y -= result.max.y - sourceUvRect.max.y;
				result.max.y = sourceUvRect.max.y;
			}

			result.min.x = std::clamp(result.min.x, sourceUvRect.min.x, sourceUvRect.max.x);
			result.max.x = std::clamp(result.max.x, sourceUvRect.min.x, sourceUvRect.max.x);
			result.min.y = std::clamp(result.min.y, sourceUvRect.min.y, sourceUvRect.max.y);
			result.max.y = std::clamp(result.max.y, sourceUvRect.min.y, sourceUvRect.max.y);
			return result;
		}

		static SdRect BuildPreviewRect(const SdVec2& mousePosition, const SdVec2& size, const SdVec2& offset, const SdVec2& displaySize)
		{
			SdVec2 min = mousePosition + offset;
			if (min.x + size.x > displaySize.x)
				min.x = mousePosition.x - offset.x - size.x;
			if (min.y + size.y > displaySize.y)
				min.y = mousePosition.y - offset.y - size.y;
			min.x = std::clamp(min.x, 0.0f, std::max(0.0f, displaySize.x - size.x));
			min.y = std::clamp(min.y, 0.0f, std::max(0.0f, displaySize.y - size.y));
			return { min, min + size };
		}

		static SdRect BuildSourceMarkerRect(const SdRect& imageRect, const SdRect& sourceUvRect, const SdRect& zoomUvRect)
		{
			const SdVec2 uvSize = {
				std::max(0.0001f, sourceUvRect.Width()),
				std::max(0.0001f, sourceUvRect.Height())
			};
			const float left = (zoomUvRect.min.x - sourceUvRect.min.x) / uvSize.x;
			const float top = (zoomUvRect.min.y - sourceUvRect.min.y) / uvSize.y;
			const float right = (zoomUvRect.max.x - sourceUvRect.min.x) / uvSize.x;
			const float bottom = (zoomUvRect.max.y - sourceUvRect.min.y) / uvSize.y;
			return {
				imageRect.min.x + imageRect.Width() * std::clamp(left, 0.0f, 1.0f),
				imageRect.min.y + imageRect.Height() * std::clamp(top, 0.0f, 1.0f),
				imageRect.min.x + imageRect.Width() * std::clamp(right, 0.0f, 1.0f),
				imageRect.min.y + imageRect.Height() * std::clamp(bottom, 0.0f, 1.0f)
			};
		}

		static SdRect InsetRect(const SdRect& rect, float inset)
		{
			return {
				rect.min.x + inset,
				rect.min.y + inset,
				rect.max.x - inset,
				rect.max.y - inset
			};
		}
	};
}
