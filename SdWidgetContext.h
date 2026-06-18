#pragma once

#include "SdDrawList.h"
#include "SdInput.h"
#include "SdUiCore.h"

#include <concepts>

namespace Sodium
{
	class SdInstance;
	class SdUi;

	struct SdWidgetTag {};

	template<class T>
	concept SdDeclarableWidget =
		std::derived_from<T, SdWidgetTag>
		&& std::is_default_constructible_v<T>;

	enum class SdWidgetLifePhase : SdUInt8
	{
		Entering,
		Alive,
		Leaving,
		Dead
	};

	struct SdWidgetState final
	{
		SdWidgetId id = 0;
		SdWidgetLifePhase lifePhase = SdWidgetLifePhase::Entering;
		SdLayerPriority layerPriority = SdLayerPriority::Content;
		bool submittedThisFrame = false;
		bool inputEnabled = true;
		bool manualLayout = false;
		bool arrangeChildren = false;
		bool clipChildren = false;
		bool layoutDirty = true;
		bool styleDirty = true;
		bool animationActive = false;
		SdRect lastRect = {};
		SdRect targetRect = {};
		SdRect animatedRect = {};
		SdRect manualRect = {};
		SdRect childContentRect = {};
		SdRect computedClipRect = {};
		SdVec2 measuredSize = {};
		SdSpacing childPadding = {};
		float layoutWeight = 0.0f;
		float opacity = 0.0f;
		float childSpacing = 0.0f;
		SdFrameIndex lastSubmittedFrame = 0;
	};

	struct SdWidgetContextBase
	{
		SdInstance& instance;
		SdUi& ui;
		SdWidgetId id = 0;
		SdWidgetId parentId = 0;
		SdWidgetState& widgetState;
		SdComputedStyle& style;
		SdThemeView theme = {};

		template<class T>
		T& State();
	};

	struct SdCreateContext : SdWidgetContextBase
	{
	};

	struct SdUpdateContext : SdWidgetContextBase
	{
		const SdInputSnapshot& input;
		SdDuration deltaTime = {};
		SdFrameIndex frameIndex = 0;
	};

	struct SdLayoutContext : SdWidgetContextBase
	{
		SdLayoutConstraints constraints = {};
		SdLayoutResult& result;

		void SetDesiredSize(SdVec2 size)
		{
			result.desiredSize = size;
		}
	};

	struct SdPaintContext : SdWidgetContextBase
	{
		SdRenderList& renderList;
		SdRect rect = {};
		SdRect animatedRect = {};
		SdRect clipRect = {};
		float opacity = 1.0f;
		SdLayerId layer = 0;
	};
}
