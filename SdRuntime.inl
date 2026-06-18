#pragma once

#include <algorithm>

namespace Sodium
{
	inline SdUi::SdUi(SdInstance& owner)
		: instance(owner)
	{
		parentStack.reserve(32);
	}

	inline SdWidgetId SdUi::CurrentParentId() const noexcept
	{
		return parentStack.empty() ? 0 : parentStack.back();
	}

	inline void SdUi::BeginDeclarationFrame()
	{
		parentStack.clear();
		nextOrdinalByParent.clear();
	}

	inline SdWidgetId SdUi::ResolveWidgetId(SdUInt64 typeHash)
	{
		const SdWidgetId parentId = CurrentParentId();
		SdUInt32& ordinal = nextOrdinalByParent[parentId];
		++ordinal;

		SdUInt64 seed = 1469598103934665603ull;
		seed = Detail::SdHashCombine(seed, parentId);
		seed = Detail::SdHashCombine(seed, typeHash);
		seed = Detail::SdHashCombine(seed, ordinal);
		return seed == 0 ? 1 : seed;
	}

	inline SdInstance::SdInstance()
		: renderList(nullptr, &renderSharedData), uiObject(*this), ui(uiObject)
	{
	}

	inline void SdInstance::BeginInputFrame()
	{
		++frameIndex;
		input.BeginFrame(frameIndex);
	}

	inline void SdInstance::FinishInputAndBeginUiFrame(SdVec2 newDisplaySize)
	{
		if (newDisplaySize.x > 0.0f && newDisplaySize.y > 0.0f)
			displaySize = newDisplaySize;
		input.FinalizeFrame();
		renderList.SetSharedData(&renderSharedData);
		renderList.Reset();
		frameOrder.clear();
		nextOrder = 0;
		uiObject.BeginDeclarationFrame();
	}

	inline void SdInstance::BeginFrame(SdVec2 newDisplaySize)
	{
		BeginInputFrame();
		FinishInputAndBeginUiFrame(newDisplaySize);
	}

	inline void SdInstance::EndFrame()
	{
		FinishWidgetFrame();
	}

	inline void SdInstance::Shutdown()
	{
		widgets.clear();
		frameOrder.clear();
		renderList.Reset();
	}

	inline void SdInstance::SetFontBackend(ISdFontBackend* fontBackend) noexcept
	{
		renderSharedData.fontBackend = fontBackend;
		if (fontBackend)
			fontBackend->ConfigureRenderSharedData(renderSharedData);
	}

	inline SdInstance::SdWidgetRecord& SdInstance::GetOrCreateWidgetRecord(SdWidgetId id)
	{
		SdWidgetRecord& record = widgets[id];
		if (record.state.id == 0)
		{
			record.state.id = id;
			record.state.lifePhase = SdWidgetLifePhase::Entering;
			record.state.layoutWeight = 0.0f;
			record.state.opacity = 0.0f;
		}
		return record;
	}

	inline void SdInstance::MarkSubmitted(SdWidgetRecord& record, SdWidgetId id, SdWidgetId parentId)
	{
		record.parentId = parentId;
		record.order = nextOrder++;
		record.state.id = id;
		record.state.submittedThisFrame = true;
		record.state.lastSubmittedFrame = frameIndex;
		record.state.inputEnabled = true;
		record.state.manualLayout = false;
		record.state.arrangeChildren = false;
		record.state.clipChildren = false;
		record.state.manualRect = {};
		record.state.childContentRect = {};
		record.state.computedClipRect = {};
		record.state.childPadding = {};
		record.state.childSpacing = 0.0f;
		if (record.state.lifePhase == SdWidgetLifePhase::Leaving || record.state.lifePhase == SdWidgetLifePhase::Dead)
			record.state.lifePhase = SdWidgetLifePhase::Entering;
		frameOrder.push_back(id);
	}

	inline void SdInstance::FinishWidgetFrame()
	{
		for (auto& [id, record] : widgets)
		{
			if (!record.state.submittedThisFrame && record.state.lifePhase != SdWidgetLifePhase::Dead)
			{
				record.state.lifePhase = SdWidgetLifePhase::Leaving;
				record.state.inputEnabled = false;
			}
			UpdateWidgetAnimation(record);
		}

		SolveLayoutAndPaint();
		RemoveDeadWidgets();

		for (auto& [id, record] : widgets)
			record.state.submittedThisFrame = false;
	}

	inline void SdInstance::UpdateWidgetAnimation(SdWidgetRecord& record)
	{
		const bool leaving = record.state.lifePhase == SdWidgetLifePhase::Leaving;
		const float target = leaving ? 0.0f : 1.0f;
		const float seconds = std::max(0.001f, static_cast<float>(deltaTime.count()) / 1000000000.0f);
		const float step = seconds / 0.16f;

		record.state.layoutWeight = SdMoveToward(record.state.layoutWeight, target, step);
		record.state.opacity = SdMoveToward(record.state.opacity, target, step);
		record.state.animationActive = record.state.layoutWeight != target || record.state.opacity != target;

		if (!leaving && record.state.layoutWeight >= 1.0f && record.state.opacity >= 1.0f)
			record.state.lifePhase = SdWidgetLifePhase::Alive;
		if (leaving && record.state.layoutWeight <= 0.0f && record.state.opacity <= 0.0f)
			record.state.lifePhase = SdWidgetLifePhase::Dead;
	}

	inline void SdInstance::SolveLayoutAndPaint()
	{
		std::vector<SdWidgetId> liveIds;
		liveIds.reserve(widgets.size());
		for (const auto& [id, record] : widgets)
		{
			if (record.state.lifePhase != SdWidgetLifePhase::Dead)
				liveIds.push_back(id);
		}

		std::sort(liveIds.begin(), liveIds.end(), [this](SdWidgetId left, SdWidgetId right)
		{
			return widgets[left].order < widgets[right].order;
		});

		const SdRect clipRect = { 0.0f, 0.0f, displaySize.x, displaySize.y };

		auto intersectRect = [](const SdRect& left, const SdRect& right) -> SdRect
		{
			SdRect result = {
				std::max(left.min.x, right.min.x),
				std::max(left.min.y, right.min.y),
				std::min(left.max.x, right.max.x),
				std::min(left.max.y, right.max.y)
			};
			result.max.x = std::max(result.min.x, result.max.x);
			result.max.y = std::max(result.min.y, result.max.y);
			return result;
		};

		auto insetRect = [](const SdRect& rect, const SdSpacing& padding) -> SdRect
		{
			SdRect result = {
				rect.min.x + padding.left,
				rect.min.y + padding.top,
				rect.max.x - padding.right,
				rect.max.y - padding.bottom
			};
			result.max.x = std::max(result.min.x, result.max.x);
			result.max.y = std::max(result.min.y, result.max.y);
			return result;
		};

		for (SdWidgetId id : liveIds)
		{
			SdWidgetRecord& record = widgets[id];
			record.state.computedClipRect = clipRect;

			SdLayoutResult result = {};
			result.desiredSize = {
				record.style.width > 0.0f ? record.style.width : 160.0f,
				record.style.height > 0.0f ? record.style.height : 28.0f
			};

			SdLayoutContext layoutContext{
				*this,
				uiObject,
				id,
				record.parentId,
				record.state,
				record.style,
				theme,
				{ {}, displaySize },
				result
			};
			if (record.layoutCallback && record.widgetObject.value)
				record.layoutCallback(record.widgetObject.value, layoutContext);

			record.state.measuredSize = result.desiredSize;
		}

		auto parentArrangesChildren = [this](const SdWidgetRecord& record)
		{
			auto parent = widgets.find(record.parentId);
			return parent != widgets.end() && parent->second.state.arrangeChildren;
		};

		auto arrangeChildren = [&](auto&& self, SdWidgetId parentId) -> void
		{
			SdWidgetRecord& parentRecord = widgets[parentId];
			parentRecord.state.childContentRect = insetRect(parentRecord.state.targetRect, parentRecord.state.childPadding);
			const SdRect childClipRect = parentRecord.state.clipChildren
				? intersectRect(parentRecord.state.computedClipRect, parentRecord.state.childContentRect)
				: parentRecord.state.computedClipRect;

			float childY = parentRecord.state.childContentRect.min.y;
			for (SdWidgetId childId : liveIds)
			{
				SdWidgetRecord& childRecord = widgets[childId];
				if (childRecord.parentId != parentId)
					continue;

				const SdVec2 childSize = childRecord.state.measuredSize;
				if (childRecord.state.manualLayout)
				{
					childRecord.state.targetRect = childRecord.state.manualRect;
				}
				else
				{
					const float availableWidth = std::max(0.0f, parentRecord.state.childContentRect.Width());
					const float childWidth = std::min(childSize.x, availableWidth);
					childRecord.state.targetRect = {
						parentRecord.state.childContentRect.min.x,
						childY,
						parentRecord.state.childContentRect.min.x + childWidth,
						childY + childSize.y
					};
					childY += (childSize.y * childRecord.state.layoutWeight) + parentRecord.state.childSpacing;
				}

				childRecord.state.animatedRect = childRecord.state.targetRect;
				childRecord.state.lastRect = childRecord.state.animatedRect;
				childRecord.state.computedClipRect = childClipRect;
				if (static_cast<SdUInt8>(childRecord.state.layerPriority) < static_cast<SdUInt8>(parentRecord.state.layerPriority))
					childRecord.state.layerPriority = parentRecord.state.layerPriority;
				if (childRecord.state.arrangeChildren)
					self(self, childId);
			}
		};

		float y = 12.0f;
		for (SdWidgetId id : liveIds)
		{
			SdWidgetRecord& record = widgets[id];
			if (parentArrangesChildren(record))
				continue;

			if (record.state.manualLayout)
			{
				record.state.targetRect = record.state.manualRect;
			}
			else
			{
				const SdVec2 desiredSize = record.state.measuredSize;
				record.state.targetRect = {
					12.0f,
					y,
					12.0f + desiredSize.x,
					y + desiredSize.y
				};
				y += (desiredSize.y * record.state.layoutWeight) + 8.0f;
			}
			record.state.animatedRect = record.state.targetRect;
			record.state.lastRect = record.state.animatedRect;
			record.state.computedClipRect = clipRect;
			if (record.state.arrangeChildren)
				arrangeChildren(arrangeChildren, id);
		}

		std::vector<SdWidgetId> paintIds = liveIds;
		std::sort(paintIds.begin(), paintIds.end(), [this](SdWidgetId left, SdWidgetId right)
		{
			const SdWidgetRecord& leftRecord = widgets[left];
			const SdWidgetRecord& rightRecord = widgets[right];
			if (leftRecord.state.layerPriority != rightRecord.state.layerPriority)
				return static_cast<SdUInt8>(leftRecord.state.layerPriority) < static_cast<SdUInt8>(rightRecord.state.layerPriority);
			return leftRecord.order < rightRecord.order;
		});

		for (SdWidgetId id : paintIds)
		{
			SdWidgetRecord& record = widgets[id];
			if (record.state.opacity <= 0.0f)
				continue;

			SdPaintContext paintContext{
				*this,
				uiObject,
				id,
				record.parentId,
				record.state,
				record.style,
				theme,
				renderList,
				record.state.targetRect,
				record.state.animatedRect,
				record.state.computedClipRect,
				record.state.opacity,
				static_cast<SdLayerId>(record.state.layerPriority)
			};
			if (record.paintCallback && record.widgetObject.value)
				record.paintCallback(record.widgetObject.value, paintContext);
		}

		if (renderSharedData.fontBackend)
		{
			renderSharedData.fontBackend->ConfigureRenderSharedData(renderSharedData);
			renderSharedData.fontBackend->DrainPendingUploads(renderList.GetDrawData().uploads);
			renderSharedData.fontBackend->ConfigureRenderSharedData(renderSharedData);
		}
	}

	inline void SdInstance::RemoveDeadWidgets()
	{
		for (auto it = widgets.begin(); it != widgets.end();)
		{
			if (it->second.state.lifePhase == SdWidgetLifePhase::Dead)
				it = widgets.erase(it);
			else
				++it;
		}
	}

	template<class T>
	T& SdInstance::GetOrCreateUserState(SdWidgetId widgetId)
	{
		SdWidgetRecord& record = GetOrCreateWidgetRecord(widgetId);
		Detail::SdAnyObject& object = record.userStates[std::type_index(typeid(T))];
		if (!object.value)
			return object.Create<T>();
		if (T* state = object.Get<T>())
			return *state;
		return object.Create<T>();
	}

	template<class T>
	T& SdWidgetContextBase::State()
	{
		return instance.GetOrCreateUserState<T>(id);
	}

	template<SdDeclarableWidget T, class... TArgs>
	T& SdUi::Declare(TArgs&&... args)
	{
		const SdWidgetId parentId = CurrentParentId();
		const SdWidgetId id = ResolveWidgetId(Detail::SdTypeHash<T>());
		SdInstance::SdWidgetRecord& record = instance.GetOrCreateWidgetRecord(id);
		const bool created = record.widgetType != std::type_index(typeid(T)) || !record.widgetObject.value;

		if (created)
		{
			record.widgetType = std::type_index(typeid(T));
			record.widgetObject.Create<T>();
			record.layoutCallback = &SdInstance::LayoutThunk<T>;
			record.paintCallback = &SdInstance::PaintThunk<T>;
		}

		instance.MarkSubmitted(record, id, parentId);
		T& widget = *record.widgetObject.Get<T>();

		SdCreateContext createContext{
			instance,
			*this,
			id,
			parentId,
			record.state,
			record.style,
			instance.theme
		};
		if (created)
		{
			if constexpr (requires(T& value, SdCreateContext& context, TArgs&&... values) { value.OnCreate(context, std::forward<TArgs>(values)...); })
				widget.OnCreate(createContext, std::forward<TArgs>(args)...);
			else if constexpr (requires(T& value, SdCreateContext& context) { value.OnCreate(context); })
				widget.OnCreate(createContext);
		}

		SdUpdateContext updateContext{
			instance,
			*this,
			id,
			parentId,
			record.state,
			record.style,
			instance.theme,
			instance.input.GetSnapshot(),
			instance.deltaTime,
			instance.frameIndex
		};

		parentStack.push_back(id);
		if constexpr (requires(T& value, SdUpdateContext& context, TArgs&&... values) { value.OnUpdate(context, std::forward<TArgs>(values)...); })
			widget.OnUpdate(updateContext, std::forward<TArgs>(args)...);
		else if constexpr (requires(T& value, SdUpdateContext& context) { value.OnUpdate(context); })
			widget.OnUpdate(updateContext);
		parentStack.pop_back();

		return widget;
	}
}
