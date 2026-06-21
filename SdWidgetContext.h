#pragma once

#include "SdDrawList.h"
#include "SdInput.h"
#include "SdStyleCore.h"

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

	template<class T>
	concept SdStylableWidget =
		SdDeclarableWidget<T>
		&& requires { typename T::Style; };

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
		SdStyleId targetTypeId = SdWidgetTargetIds::Default;
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
		float layoutWeight = 0.0f;
		float opacity = 0.0f;
		SdFrameIndex lastSubmittedFrame = 0;
	};

	struct SdWidgetContextBase
	{
		SdInstance& instance;
		SdUi& ui;
		SdWidgetId id = 0;
		SdWidgetId parentId = 0;
		SdWidgetState& widgetState;
		SdThemeView theme = {};
		SdResolvedKey resolvedKey = 0;

		template<class T>
		T& State();

		template<class T>
		T& Model();

		const SdStyleNode& RootStyleNode() const;
		const SdStyleNode& Part(SdStylePart part) const;
		SdStyleNode& EnsurePart(SdStylePart part);

		template<class TWidget>
		const typename TWidget::Style& RootResolvedStyle();

		template<class TWidget>
		const typename TWidget::Style& RootPresentationStyle();

		bool HasModelKey() const noexcept;
		bool IsHovered() const noexcept;
		bool IsPressed() const noexcept;
		bool WasPressed() const noexcept;
		bool WasReleased() const noexcept;
		bool WasClicked() const noexcept;
		bool IsCaptured() const noexcept;
		bool IsFocused() const noexcept;
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
