#pragma once

#include "SdUiCore.h"

#include <array>
#include <cstring>
#include <functional>
#include <typeindex>
#include <type_traits>
#include <vector>

namespace Sodium
{
	struct SdTheme final
	{
		std::array<SdColor, static_cast<SdSize>(SdStyleToken::Count)> colors = {};
		std::array<float, static_cast<SdSize>(SdStyleToken::Count)> metrics = {};

		SdTheme()
		{
			colors[static_cast<SdSize>(SdStyleToken::ColorText)] = SdColorWhite;
			colors[static_cast<SdSize>(SdStyleToken::ColorWindowBg)] = { 24, 30, 39, 242 };
			colors[static_cast<SdSize>(SdStyleToken::ColorPanelBg)] = { 35, 42, 52, 235 };
			colors[static_cast<SdSize>(SdStyleToken::ColorButton)] = { 48, 72, 96, 255 };
			colors[static_cast<SdSize>(SdStyleToken::ColorButtonHovered)] = { 62, 100, 138, 255 };
			colors[static_cast<SdSize>(SdStyleToken::ColorButtonPressed)] = { 68, 118, 160, 255 };
			colors[static_cast<SdSize>(SdStyleToken::ColorAccent)] = { 82, 170, 128, 255 };
			colors[static_cast<SdSize>(SdStyleToken::ColorBorder)] = { 91, 109, 128, 255 };
			colors[static_cast<SdSize>(SdStyleToken::ColorBorderStrong)] = { 128, 154, 180, 255 };
			colors[static_cast<SdSize>(SdStyleToken::ColorDanger)] = { 164, 66, 66, 255 };
			colors[static_cast<SdSize>(SdStyleToken::ColorSelection)] = { 82, 170, 128, 96 };
			colors[static_cast<SdSize>(SdStyleToken::ColorBackground)] = { 24, 30, 39, 242 };
			metrics[static_cast<SdSize>(SdStyleToken::SpacingSmall)] = 6.0f;
			metrics[static_cast<SdSize>(SdStyleToken::SpacingMedium)] = 10.0f;
			metrics[static_cast<SdSize>(SdStyleToken::RadiusSmall)] = 5.0f;
			metrics[static_cast<SdSize>(SdStyleToken::DurationFast)] = 0.16f;
		}

		SdColor GetColor(SdStyleToken token) const noexcept
		{
			return colors[static_cast<SdSize>(token)];
		}

		float GetMetric(SdStyleToken token) const noexcept
		{
			return metrics[static_cast<SdSize>(token)];
		}
	};

	struct SdStyleRule final
	{
		SdStyleTokenTag targetTag = SdStyleTargetTags::Global;
		SdStyleInteractionState interactionState = SdStyleInteractionState::Normal;
		SdLayerPriority layerPriority = SdLayerPriority::Content;
		SdStyleClassId classId = 0;
		SdStyleScopeId scopeId = 0;
		bool matchLayer = false;
		bool matchClass = false;
		bool matchScope = false;
		std::vector<SdStyleDeclaration> declarations = {};

		void SetValue(SdStyleTokenTag propertyTag, SdStyleValue value)
		{
			for (SdStyleDeclaration& declaration : declarations)
			{
				if (declaration.propertyTag == propertyTag)
				{
					declaration.value = value;
					return;
				}
			}
			declarations.push_back({ propertyTag, value });
		}

		void SetColor(SdStyleTokenTag propertyTag, SdColor value)
		{
			SetValue(propertyTag, SdStyleValue::FromColor(value));
		}

		void SetFloat(SdStyleTokenTag propertyTag, float value)
		{
			SetValue(propertyTag, SdStyleValue::FromFloat(value));
		}

		void SetSpacing(SdStyleTokenTag propertyTag, SdSpacing value)
		{
			SetValue(propertyTag, SdStyleValue::FromSpacing(value));
		}

		void SetVec2(SdStyleTokenTag propertyTag, SdVec2 value)
		{
			SetValue(propertyTag, SdStyleValue::FromVec2(value));
		}

		void SetColorToken(SdStyleTokenTag propertyTag, SdStyleToken token)
		{
			SetValue(propertyTag, SdStyleValue::FromColorToken(token));
		}

		void SetMetricToken(SdStyleTokenTag propertyTag, SdStyleToken token)
		{
			SetValue(propertyTag, SdStyleValue::FromMetricToken(token));
		}
	};

	struct SdStyleContext final
	{
		const SdTheme& theme;
	};

	struct SdStyleFieldDescriptor final
	{
		SdUInt64 fieldId = 0;
		SdStyleFieldImpact impact = SdStyleFieldImpact::Discrete;
		SdStyleInterpolation interpolation = SdStyleInterpolation::None;
		bool expensiveTransition = false;
		std::function<bool(const void*, const void*)> equals = {};
		std::function<void(void*, const void*)> copy = {};
		std::function<SdStyleValue(const void*)> readValue = {};
		std::function<void(void*, const SdStyleValue&, const SdTheme&)> writeValue = {};
	};

	namespace Detail
	{
		template<class TStyle, class TField>
		SdUInt64 SdStyleFieldId(TField TStyle::* member) noexcept
		{
			SdUInt64 hash = 14695981039346656037ull;
			const unsigned char* bytes = reinterpret_cast<const unsigned char*>(&member);
			for (SdSize index = 0; index < sizeof(member); ++index)
			{
				hash ^= static_cast<SdUInt64>(bytes[index]);
				hash *= 1099511628211ull;
			}
			return hash == 0 ? 1 : hash;
		}

		template<class T>
		inline constexpr bool SdIsStyleValueField = false;

		template<>
		inline constexpr bool SdIsStyleValueField<SdColor> = true;

		template<>
		inline constexpr bool SdIsStyleValueField<float> = true;

		template<>
		inline constexpr bool SdIsStyleValueField<SdSpacing> = true;

		template<>
		inline constexpr bool SdIsStyleValueField<SdVec2> = true;

		inline SdStyleValue MakeStyleValue(SdColor value) noexcept
		{
			return SdStyleValue::FromColor(value);
		}

		inline SdStyleValue MakeStyleValue(float value) noexcept
		{
			return SdStyleValue::FromFloat(value);
		}

		inline SdStyleValue MakeStyleValue(SdSpacing value) noexcept
		{
			return SdStyleValue::FromSpacing(value);
		}

		inline SdStyleValue MakeStyleValue(SdVec2 value) noexcept
		{
			return SdStyleValue::FromVec2(value);
		}

		template<class TField>
		bool TryResolveStyleValue(const SdStyleValue& value, const SdTheme& theme, TField& outValue)
		{
			if constexpr (std::is_same_v<TField, SdColor>)
			{
				if (value.kind == SdStyleValueKind::Color)
				{
					outValue = value.color;
					return true;
				}
				if (value.kind == SdStyleValueKind::ColorToken)
				{
					outValue = theme.GetColor(value.token);
					return true;
				}
			}
			else if constexpr (std::is_same_v<TField, float>)
			{
				if (value.kind == SdStyleValueKind::Float)
				{
					outValue = value.number;
					return true;
				}
				if (value.kind == SdStyleValueKind::MetricToken)
				{
					outValue = theme.GetMetric(value.token);
					return true;
				}
			}
			else if constexpr (std::is_same_v<TField, SdSpacing>)
			{
				if (value.kind == SdStyleValueKind::Spacing)
				{
					outValue = value.spacing;
					return true;
				}
			}
			else if constexpr (std::is_same_v<TField, SdVec2>)
			{
				if (value.kind == SdStyleValueKind::Vec2)
				{
					outValue = value.vec2;
					return true;
				}
			}
			return false;
		}

		inline bool StyleValuesEqual(const SdStyleValue& left, const SdStyleValue& right) noexcept
		{
			if (left.kind != right.kind)
				return false;
			switch (left.kind)
			{
			case SdStyleValueKind::Color:
				return left.color == right.color;
			case SdStyleValueKind::Float:
				return left.number == right.number;
			case SdStyleValueKind::Spacing:
				return left.spacing.left == right.spacing.left
					&& left.spacing.top == right.spacing.top
					&& left.spacing.right == right.spacing.right
					&& left.spacing.bottom == right.spacing.bottom;
			case SdStyleValueKind::Vec2:
				return left.vec2.x == right.vec2.x && left.vec2.y == right.vec2.y;
			case SdStyleValueKind::ColorToken:
			case SdStyleValueKind::MetricToken:
				return left.token == right.token;
			case SdStyleValueKind::None:
			default:
				return true;
			}
		}

		inline SdStyleValue InterpolateStyleValue(
			const SdStyleValue& start,
			const SdStyleValue& target,
			SdStyleInterpolation interpolation,
			float amount) noexcept
		{
			const float t = std::clamp(amount, 0.0f, 1.0f);
			if (start.kind != target.kind)
				return target;

			switch (interpolation)
			{
			case SdStyleInterpolation::Float:
				if (start.kind == SdStyleValueKind::Float)
					return SdStyleValue::FromFloat(start.number + (target.number - start.number) * t);
				break;
			case SdStyleInterpolation::Color:
				if (start.kind == SdStyleValueKind::Color)
				{
					const auto lerpByte = [t](SdUInt8 left, SdUInt8 right) noexcept
					{
						const float value = static_cast<float>(left) + (static_cast<float>(right) - static_cast<float>(left)) * t;
						return static_cast<SdUInt8>(std::clamp(value, 0.0f, 255.0f));
					};
					return SdStyleValue::FromColor({
						lerpByte(start.color.r, target.color.r),
						lerpByte(start.color.g, target.color.g),
						lerpByte(start.color.b, target.color.b),
						lerpByte(start.color.a, target.color.a)
					});
				}
				break;
			case SdStyleInterpolation::Spacing:
				if (start.kind == SdStyleValueKind::Spacing)
				{
					return SdStyleValue::FromSpacing({
						start.spacing.left + (target.spacing.left - start.spacing.left) * t,
						start.spacing.top + (target.spacing.top - start.spacing.top) * t,
						start.spacing.right + (target.spacing.right - start.spacing.right) * t,
						start.spacing.bottom + (target.spacing.bottom - start.spacing.bottom) * t
					});
				}
				break;
			case SdStyleInterpolation::Vec2:
				if (start.kind == SdStyleValueKind::Vec2)
				{
					return SdStyleValue::FromVec2({
						start.vec2.x + (target.vec2.x - start.vec2.x) * t,
						start.vec2.y + (target.vec2.y - start.vec2.y) * t
					});
				}
				break;
			case SdStyleInterpolation::None:
			default:
				break;
			}
			return target;
		}
	}

	template<class TStyle>
	class SdStyleContract final
	{
	public:
		class FieldBuilder final
		{
		private:
			SdStyleContract& owner;
			SdSize index = 0;

		public:
			FieldBuilder(SdStyleContract& contract, SdSize descriptorIndex)
				: owner(contract), index(descriptorIndex) {}

			FieldBuilder& InterpolatesAsFloat() noexcept
			{
				owner.fields[index].interpolation = SdStyleInterpolation::Float;
				return *this;
			}

			FieldBuilder& InterpolatesAsColor() noexcept
			{
				owner.fields[index].interpolation = SdStyleInterpolation::Color;
				return *this;
			}

			FieldBuilder& InterpolatesAsSpacing() noexcept
			{
				owner.fields[index].interpolation = SdStyleInterpolation::Spacing;
				return *this;
			}

			FieldBuilder& InterpolatesAsVec2() noexcept
			{
				owner.fields[index].interpolation = SdStyleInterpolation::Vec2;
				return *this;
			}

			FieldBuilder& Discrete() noexcept
			{
				owner.fields[index].interpolation = SdStyleInterpolation::None;
				return *this;
			}

			FieldBuilder& ExpensiveTransition() noexcept
			{
				owner.fields[index].expensiveTransition = true;
				return *this;
			}
		};

	private:
		std::vector<SdStyleFieldDescriptor> fields = {};

		template<class TField>
		FieldBuilder AddField(TField TStyle::* member, SdStyleFieldImpact impact)
		{
			const SdUInt64 fieldId = Detail::SdStyleFieldId(member);
			const auto configureField = [member, impact, fieldId](SdStyleFieldDescriptor& descriptor)
			{
				descriptor.fieldId = fieldId;
				descriptor.impact = impact;
				descriptor.equals = [member](const void* left, const void* right)
				{
					const TField& leftValue = static_cast<const TStyle*>(left)->*member;
					const TField& rightValue = static_cast<const TStyle*>(right)->*member;
					if constexpr (requires { leftValue == rightValue; })
						return leftValue == rightValue;
					else if constexpr (std::is_trivially_copyable_v<TField>)
						return std::memcmp(&leftValue, &rightValue, sizeof(TField)) == 0;
					else
						return false;
				};
				descriptor.copy = [member](void* destination, const void* source)
				{
					static_cast<TStyle*>(destination)->*member = static_cast<const TStyle*>(source)->*member;
				};

				if constexpr (Detail::SdIsStyleValueField<TField>)
				{
					descriptor.readValue = [member](const void* style)
					{
						return Detail::MakeStyleValue(static_cast<const TStyle*>(style)->*member);
					};
					descriptor.writeValue = [member](void* style, const SdStyleValue& value, const SdTheme& theme)
					{
						TField resolvedValue = {};
						if (Detail::TryResolveStyleValue(value, theme, resolvedValue))
							static_cast<TStyle*>(style)->*member = resolvedValue;
					};
				}
				else
				{
					descriptor.readValue = {};
					descriptor.writeValue = {};
				}
			};

			for (SdSize index = 0; index < fields.size(); ++index)
			{
				if (fields[index].fieldId == fieldId)
				{
					configureField(fields[index]);
					return FieldBuilder(*this, index);
				}
			}
			fields.push_back({});
			configureField(fields.back());
			return FieldBuilder(*this, fields.size() - 1);
		}

	public:
		template<class TField>
		FieldBuilder Layout(TField TStyle::* member)
		{
			return AddField(member, SdStyleFieldImpact::Layout);
		}

		template<class TField>
		FieldBuilder Paint(TField TStyle::* member)
		{
			return AddField(member, SdStyleFieldImpact::Paint);
		}

		template<class TField>
		FieldBuilder Composite(TField TStyle::* member)
		{
			return AddField(member, SdStyleFieldImpact::Composite);
		}

		template<class TField>
		FieldBuilder Discrete(TField TStyle::* member)
		{
			return AddField(member, SdStyleFieldImpact::Discrete).Discrete();
		}

		const std::vector<SdStyleFieldDescriptor>& GetFields() const noexcept
		{
			return fields;
		}

		const SdStyleFieldDescriptor* Find(SdUInt64 fieldId) const noexcept
		{
			for (const SdStyleFieldDescriptor& field : fields)
			{
				if (field.fieldId == fieldId)
					return &field;
			}
			return nullptr;
		}
	};

	struct SdTypedStyleDeclaration final
	{
		std::type_index styleType = std::type_index(typeid(void));
		SdUInt64 fieldId = 0;
		SdStyleValue value = {};
		std::function<void(void*, const SdStyleValue&, const SdTheme&)> apply = {};
	};

	struct SdTypedStyleTransition final
	{
		std::type_index styleType = std::type_index(typeid(void));
		SdUInt64 fieldId = 0;
		SdTransition transition = {};
	};

	struct SdTypedStyleRule final
	{
		std::type_index styleType = std::type_index(typeid(void));
		SdStyleTokenTag targetTag = SdStyleTargetTags::Global;
		SdStyleInteractionState interactionState = SdStyleInteractionState::Normal;
		SdLayerPriority layerPriority = SdLayerPriority::Content;
		SdStyleClassId classId = 0;
		SdStyleScopeId scopeId = 0;
		bool matchLayer = false;
		bool matchClass = false;
		bool matchScope = false;
		std::vector<SdTypedStyleDeclaration> declarations = {};
		std::vector<SdTypedStyleTransition> transitions = {};
	};

	template<class TWidget>
	class SdStyleRuleBuilder;

	class SdStyleSystem final
	{
	private:
		SdTheme theme = {};
		std::vector<SdStyleRule> rules = {};
		std::vector<SdTypedStyleRule> typedRules = {};
		SdUInt64 revision = 1;

	public:
		SdStyleSystem()
		{
			AddDefaultRules();
		}

		const SdTheme& GetTheme() const noexcept
		{
			return theme;
		}

		SdUInt64 GetRevision() const noexcept
		{
			return revision;
		}

		void SetColor(SdStyleToken token, SdColor color)
		{
			theme.colors[static_cast<SdSize>(token)] = color;
			Touch();
		}

		void SetMetric(SdStyleToken token, float value)
		{
			theme.metrics[static_cast<SdSize>(token)] = value;
			Touch();
		}

		void ClearRules()
		{
			rules.clear();
			typedRules.clear();
			Touch();
		}

		void AddRule(const SdStyleRule& rule)
		{
			rules.push_back(rule);
			Touch();
		}

		SdComputedStyle Resolve(
			SdStyleTokenTag targetTag,
			SdStyleInteractionState interactionState,
			SdLayerPriority layerPriority = SdLayerPriority::Content,
			SdSpan<const SdStyleClassId> styleClasses = {},
			SdStyleScopeId styleScope = 0) const
		{
			SdComputedStyle result = {};
			SetBaseValues(result);
			ApplyMatchingRules(result, targetTag, interactionState, layerPriority, styleClasses, styleScope, true);
			ApplyMatchingRules(result, targetTag, interactionState, layerPriority, styleClasses, styleScope, false);
			return result;
		}

		template<class TWidget>
		SdStyleRuleBuilder<TWidget> Rule();

		template<class TWidget>
		typename TWidget::Style ResolveTargetStyle(
			SdStyleInteractionState interactionState,
			SdLayerPriority layerPriority = SdLayerPriority::Content,
			const typename TWidget::Style* inlineStyle = nullptr) const;

		template<class TWidget>
		typename TWidget::Style ResolveTargetStyle(
			SdStyleInteractionState interactionState,
			SdLayerPriority layerPriority,
			SdSpan<const SdStyleClassId> styleClasses,
			SdStyleScopeId styleScope,
			const typename TWidget::Style* inlineStyle = nullptr) const;

		template<class TWidget>
		bool TryResolveTransition(
			SdUInt64 fieldId,
			SdStyleInteractionState interactionState,
			SdLayerPriority layerPriority,
			SdSpan<const SdStyleClassId> styleClasses,
			SdStyleScopeId styleScope,
			SdTransition& transition) const;

		template<class TWidget>
		const SdStyleContract<typename TWidget::Style>& GetContract() const;

	private:
		template<class TWidget>
		static constexpr SdStyleTokenTag GetTargetTag() noexcept
		{
			if constexpr (requires { TWidget::Style::TokenTag; })
				return TWidget::Style::TokenTag;
			else
				return Detail::SdTypeHash<TWidget>();
		}

		template<class TWidget>
		typename TWidget::Style BuildDefaultStyle() const
		{
			using Style = typename TWidget::Style;
			const SdStyleContext styleContext{ theme };
			if constexpr (requires(const SdStyleContext& context) { { Style::Default(context) } -> std::same_as<Style>; })
				return Style::Default(styleContext);
			else
				return {};
		}

		SdTypedStyleRule& AddTypedRule(std::type_index styleType, SdStyleTokenTag targetTag)
		{
			SdTypedStyleRule rule = {};
			rule.styleType = styleType;
			rule.targetTag = targetTag;
			typedRules.push_back(std::move(rule));
			Touch();
			return typedRules.back();
		}

		template<class TStyle>
		void ApplyTypedRules(
			TStyle& style,
			SdStyleTokenTag targetTag,
			SdStyleInteractionState interactionState,
			SdLayerPriority layerPriority,
			SdSpan<const SdStyleClassId> styleClasses,
			SdStyleScopeId styleScope) const
		{
			for (const SdTypedStyleRule& rule : typedRules)
			{
				if (!MatchesTypedRule(rule, std::type_index(typeid(TStyle)), targetTag, interactionState, layerPriority, styleClasses, styleScope))
					continue;

				for (const SdTypedStyleDeclaration& declaration : rule.declarations)
				{
					if (declaration.apply)
						declaration.apply(&style, declaration.value, theme);
				}
			}
		}

		template<class TWidget>
		friend class SdStyleRuleBuilder;

		void Touch() noexcept
		{
			++revision;
			if (revision == 0)
				revision = 1;
		}

		void AddDefaultRules()
		{
			SdStyleRule global = {};
			global.targetTag = SdStyleTargetTags::Global;
			global.SetColorToken(SdStylePropertyTags::Color, SdStyleToken::ColorText);
			global.SetColorToken(SdStylePropertyTags::Border, SdStyleToken::ColorBorder);
			global.SetMetricToken(SdStylePropertyTags::Radius, SdStyleToken::RadiusSmall);
			global.SetFloat(SdStylePropertyTags::Opacity, 1.0f);
			AddRule(global);

			AddBackgroundRule(SdStyleTargetTags::Default, SdStyleInteractionState::Normal, SdLayerPriority::Content, SdStyleToken::ColorBackground);
			AddBackgroundRule(SdStyleTargetTags::Panel, SdStyleInteractionState::Normal, SdLayerPriority::Content, SdStyleToken::ColorPanelBg);
			AddBackgroundRule(SdStyleTargetTags::Button, SdStyleInteractionState::Normal, SdLayerPriority::Content, SdStyleToken::ColorButton);
			AddBackgroundRule(SdStyleTargetTags::Button, SdStyleInteractionState::Hovered, SdLayerPriority::Content, SdStyleToken::ColorButtonHovered);
			AddBackgroundRule(SdStyleTargetTags::Button, SdStyleInteractionState::Pressed, SdLayerPriority::Content, SdStyleToken::ColorButtonPressed);
			AddBackgroundRule(SdStyleTargetTags::CheckBox, SdStyleInteractionState::Normal, SdLayerPriority::Content, SdStyleToken::ColorPanelBg);
			AddBackgroundRule(SdStyleTargetTags::CheckBox, SdStyleInteractionState::Hovered, SdLayerPriority::Content, SdStyleToken::ColorButtonHovered);
			AddBackgroundRule(SdStyleTargetTags::Window, SdStyleInteractionState::Normal, SdLayerPriority::Floating, SdStyleToken::ColorWindowBg, true);
			AddBackgroundRule(SdStyleTargetTags::ImageViewer, SdStyleInteractionState::Normal, SdLayerPriority::Content, SdStyleToken::ColorPanelBg);
			AddBackgroundRule(SdStyleTargetTags::Slider, SdStyleInteractionState::Normal, SdLayerPriority::Content, SdStyleToken::ColorPanelBg);
			AddBackgroundRule(SdStyleTargetTags::Slider, SdStyleInteractionState::Hovered, SdLayerPriority::Content, SdStyleToken::ColorButtonHovered);
			AddBackgroundRule(SdStyleTargetTags::Slider, SdStyleInteractionState::Pressed, SdLayerPriority::Content, SdStyleToken::ColorButtonPressed);
			AddBackgroundRule(SdStyleTargetTags::TextInput, SdStyleInteractionState::Normal, SdLayerPriority::Content, SdStyleToken::ColorPanelBg);
			AddBackgroundRule(SdStyleTargetTags::TextInput, SdStyleInteractionState::Focused, SdLayerPriority::Content, SdStyleToken::ColorButton);
			AddBackgroundRule(SdStyleTargetTags::ScrollView, SdStyleInteractionState::Normal, SdLayerPriority::Content, SdStyleToken::ColorPanelBg);
			AddBackgroundRule(SdStyleTargetTags::Popup, SdStyleInteractionState::Normal, SdLayerPriority::Popup, SdStyleToken::ColorWindowBg, true);
			AddBackgroundRule(SdStyleTargetTags::ContextMenu, SdStyleInteractionState::Normal, SdLayerPriority::Popup, SdStyleToken::ColorWindowBg, true);
			AddBackgroundRule(SdStyleTargetTags::Tooltip, SdStyleInteractionState::Normal, SdLayerPriority::Overlay, SdStyleToken::ColorPanelBg, true);
		}

		void AddBackgroundRule(
			SdStyleTokenTag targetTag,
			SdStyleInteractionState interactionState,
			SdLayerPriority layerPriority,
			SdStyleToken backgroundToken,
			bool matchLayer = false)
		{
			SdStyleRule rule = {};
			rule.targetTag = targetTag;
			rule.interactionState = interactionState;
			rule.layerPriority = layerPriority;
			rule.matchLayer = matchLayer;
			rule.SetColorToken(SdStylePropertyTags::Background, backgroundToken);
			AddRule(rule);
		}

		void SetBaseValues(SdComputedStyle& result) const
		{
			result.SetValue(SdStylePropertyTags::Color, SdStyleValue::FromColor(theme.GetColor(SdStyleToken::ColorText)));
			result.SetValue(SdStylePropertyTags::Background, SdStyleValue::FromColor(theme.GetColor(SdStyleToken::ColorBackground)));
			result.SetValue(SdStylePropertyTags::Border, SdStyleValue::FromColor(theme.GetColor(SdStyleToken::ColorBorder)));
			result.SetValue(SdStylePropertyTags::Radius, SdStyleValue::FromFloat(theme.GetMetric(SdStyleToken::RadiusSmall)));
			result.SetValue(SdStylePropertyTags::Opacity, SdStyleValue::FromFloat(1.0f));
		}

		SdStyleValue ResolveValue(const SdStyleValue& value) const
		{
			switch (value.kind)
			{
			case SdStyleValueKind::ColorToken:
				return SdStyleValue::FromColor(theme.GetColor(value.token));
			case SdStyleValueKind::MetricToken:
				return SdStyleValue::FromFloat(theme.GetMetric(value.token));
			default:
				return value;
			}
		}

		bool MatchesRule(
			const SdStyleRule& rule,
			SdStyleTokenTag targetTag,
			SdStyleInteractionState interactionState,
			SdLayerPriority layerPriority,
			SdSpan<const SdStyleClassId> styleClasses,
			SdStyleScopeId styleScope,
			bool globalPass) const noexcept
		{
			const bool isGlobalRule = rule.targetTag == SdStyleTargetTags::Global;
			if (isGlobalRule != globalPass)
				return false;
			if (!isGlobalRule && rule.targetTag != targetTag)
				return false;
			if (rule.interactionState != SdStyleInteractionState::Normal && rule.interactionState != interactionState)
				return false;
			if (rule.matchLayer && rule.layerPriority != layerPriority)
				return false;
			if (rule.matchClass && !HasStyleClass(styleClasses, rule.classId))
				return false;
			if (rule.matchScope && rule.scopeId != styleScope)
				return false;
			return true;
		}

		bool MatchesTypedRule(
			const SdTypedStyleRule& rule,
			std::type_index styleType,
			SdStyleTokenTag targetTag,
			SdStyleInteractionState interactionState,
			SdLayerPriority layerPriority,
			SdSpan<const SdStyleClassId> styleClasses,
			SdStyleScopeId styleScope) const noexcept
		{
			if (rule.styleType != styleType)
				return false;
			if (rule.targetTag != SdStyleTargetTags::Global && rule.targetTag != targetTag)
				return false;
			if (rule.interactionState != SdStyleInteractionState::Normal && rule.interactionState != interactionState)
				return false;
			if (rule.matchLayer && rule.layerPriority != layerPriority)
				return false;
			if (rule.matchClass && !HasStyleClass(styleClasses, rule.classId))
				return false;
			if (rule.matchScope && rule.scopeId != styleScope)
				return false;
			return true;
		}

		static bool HasStyleClass(SdSpan<const SdStyleClassId> styleClasses, SdStyleClassId classId) noexcept
		{
			for (SdStyleClassId styleClass : styleClasses)
			{
				if (styleClass == classId)
					return true;
			}
			return false;
		}

		void ApplyMatchingRules(
			SdComputedStyle& result,
			SdStyleTokenTag targetTag,
			SdStyleInteractionState interactionState,
			SdLayerPriority layerPriority,
			SdSpan<const SdStyleClassId> styleClasses,
			SdStyleScopeId styleScope,
			bool globalPass) const
		{
			for (const SdStyleRule& rule : rules)
			{
				if (!MatchesRule(rule, targetTag, interactionState, layerPriority, styleClasses, styleScope, globalPass))
					continue;

				for (const SdStyleDeclaration& declaration : rule.declarations)
					result.SetValue(declaration.propertyTag, ResolveValue(declaration.value));
			}
		}
	};

	template<class TWidget>
	class SdStyleRuleBuilder final
	{
	private:
		using Style = typename TWidget::Style;

		SdStyleSystem& system;
		SdTypedStyleRule& rule;

	public:
		SdStyleRuleBuilder(SdStyleSystem& owner, SdTypedStyleRule& styleRule)
			: system(owner), rule(styleRule) {}

		SdStyleRuleBuilder& Pseudo(SdStyleInteractionState interactionState) noexcept
		{
			rule.interactionState = interactionState;
			return *this;
		}

		SdStyleRuleBuilder& Layer(SdLayerPriority layerPriority) noexcept
		{
			rule.layerPriority = layerPriority;
			rule.matchLayer = true;
			return *this;
		}

		SdStyleRuleBuilder& Class(SdStyleClassId classId) noexcept
		{
			rule.classId = classId;
			rule.matchClass = true;
			return *this;
		}

		SdStyleRuleBuilder& Class(const char* className) noexcept
		{
			return Class(SdStyleClassLiteral(className));
		}

		SdStyleRuleBuilder& Scope(SdStyleScopeId scopeId) noexcept
		{
			rule.scopeId = scopeId;
			rule.matchScope = true;
			return *this;
		}

		SdStyleRuleBuilder& Scope(const char* scopeName) noexcept
		{
			return Scope(SdStyleScopeLiteral(scopeName));
		}

		template<class TField>
		SdStyleRuleBuilder& Set(TField Style::* member, TField value)
		{
			return SetValue(member, Detail::MakeStyleValue(value));
		}

		template<class TField>
		SdStyleRuleBuilder& SetColorToken(TField Style::* member, SdStyleToken token)
		{
			static_assert(std::is_same_v<TField, SdColor>, "SetColorToken requires an SdColor style field.");
			return SetValue(member, SdStyleValue::FromColorToken(token));
		}

		template<class TField>
		SdStyleRuleBuilder& SetMetricToken(TField Style::* member, SdStyleToken token)
		{
			static_assert(std::is_same_v<TField, float>, "SetMetricToken requires a float style field.");
			return SetValue(member, SdStyleValue::FromMetricToken(token));
		}

		template<class TField>
		SdStyleRuleBuilder& Transition(TField Style::* member, SdDuration duration, SdAnimationEasing easing)
		{
			SdTypedStyleTransition transition = {};
			transition.styleType = std::type_index(typeid(Style));
			transition.fieldId = Detail::SdStyleFieldId(member);
			transition.transition = { duration, easing };
			rule.transitions.push_back(transition);
			system.Touch();
			return *this;
		}

	private:
		template<class TField>
		SdStyleRuleBuilder& SetValue(TField Style::* member, SdStyleValue value)
		{
			SdTypedStyleDeclaration declaration = {};
			declaration.styleType = std::type_index(typeid(Style));
			declaration.fieldId = Detail::SdStyleFieldId(member);
			declaration.value = value;
			declaration.apply = [member](void* style, const SdStyleValue& styleValue, const SdTheme& theme)
			{
				TField resolvedValue = {};
				if (Detail::TryResolveStyleValue(styleValue, theme, resolvedValue))
					static_cast<Style*>(style)->*member = resolvedValue;
			};
			rule.declarations.push_back(std::move(declaration));
			system.Touch();
			return *this;
		}
	};

	template<class TWidget>
	SdStyleRuleBuilder<TWidget> SdStyleSystem::Rule()
	{
		using Style = typename TWidget::Style;
		SdTypedStyleRule& rule = AddTypedRule(std::type_index(typeid(Style)), GetTargetTag<TWidget>());
		return SdStyleRuleBuilder<TWidget>(*this, rule);
	}

	template<class TWidget>
	typename TWidget::Style SdStyleSystem::ResolveTargetStyle(
		SdStyleInteractionState interactionState,
		SdLayerPriority layerPriority,
		const typename TWidget::Style* inlineStyle) const
	{
		return ResolveTargetStyle<TWidget>(interactionState, layerPriority, {}, 0, inlineStyle);
	}

	template<class TWidget>
	typename TWidget::Style SdStyleSystem::ResolveTargetStyle(
		SdStyleInteractionState interactionState,
		SdLayerPriority layerPriority,
		SdSpan<const SdStyleClassId> styleClasses,
		SdStyleScopeId styleScope,
		const typename TWidget::Style* inlineStyle) const
	{
		using Style = typename TWidget::Style;
		Style result = BuildDefaultStyle<TWidget>();
		ApplyTypedRules(result, GetTargetTag<TWidget>(), interactionState, layerPriority, styleClasses, styleScope);
		if (inlineStyle)
			result = *inlineStyle;
		return result;
	}

	template<class TWidget>
	bool SdStyleSystem::TryResolveTransition(
		SdUInt64 fieldId,
		SdStyleInteractionState interactionState,
		SdLayerPriority layerPriority,
		SdSpan<const SdStyleClassId> styleClasses,
		SdStyleScopeId styleScope,
		SdTransition& transition) const
	{
		using Style = typename TWidget::Style;
		bool found = false;
		const std::type_index styleType = std::type_index(typeid(Style));
		for (const SdTypedStyleRule& rule : typedRules)
		{
			if (!MatchesTypedRule(rule, styleType, GetTargetTag<TWidget>(), interactionState, layerPriority, styleClasses, styleScope))
				continue;

			for (const SdTypedStyleTransition& candidate : rule.transitions)
			{
				if (candidate.styleType == styleType && candidate.fieldId == fieldId)
				{
					transition = candidate.transition;
					found = true;
				}
			}
		}
		return found;
	}

	template<class TWidget>
	const SdStyleContract<typename TWidget::Style>& SdStyleSystem::GetContract() const
	{
		using Style = typename TWidget::Style;
		static const SdStyleContract<Style> contract = []
		{
			SdStyleContract<Style> result = {};
			if constexpr (requires(SdStyleContract<Style>& contractRef) { Style::Describe(contractRef); })
				Style::Describe(result);
			return result;
		}();
		return contract;
	}
}
