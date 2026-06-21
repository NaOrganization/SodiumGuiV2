#pragma once

#include "SdStyleResolver.h"

#include <cstring>
#include <functional>
#include <unordered_map>
#include <typeindex>
#include <type_traits>
#include <vector>

namespace Sodium
{
	struct SdTheme final
	{
		std::unordered_map<SdThemeVariableId, SdColor> colorVariables = {};
		std::unordered_map<SdThemeVariableId, float> metricVariables = {};

		SdTheme()
		{
			SetDefaultVariables();
		}

		void SetColorVariable(SdThemeVariableId variableId, SdColor color)
		{
			colorVariables[variableId] = color;
		}

		void SetMetricVariable(SdThemeVariableId variableId, float value)
		{
			metricVariables[variableId] = value;
		}

		SdColor GetColorVariable(SdThemeVariableId variableId, SdColor fallback = SdColorTransparent) const noexcept
		{
			auto it = colorVariables.find(variableId);
			return it == colorVariables.end() ? fallback : it->second;
		}

		float GetMetricVariable(SdThemeVariableId variableId, float fallback = 0.0f) const noexcept
		{
			auto it = metricVariables.find(variableId);
			return it == metricVariables.end() ? fallback : it->second;
		}

	private:
		void SetDefaultVariables()
		{
			colorVariables[SdThemeVariableLiteral("text")] = SdColorWhite;
			colorVariables[SdThemeVariableLiteral("background")] = { 24, 30, 39, 242 };
			colorVariables[SdThemeVariableLiteral("window.bg")] = { 24, 30, 39, 242 };
			colorVariables[SdThemeVariableLiteral("panel.bg")] = { 35, 42, 52, 235 };
			colorVariables[SdThemeVariableLiteral("button.bg")] = { 48, 72, 96, 255 };
			colorVariables[SdThemeVariableLiteral("button.bg.hover")] = { 62, 100, 138, 255 };
			colorVariables[SdThemeVariableLiteral("button.bg.pressed")] = { 68, 118, 160, 255 };
			colorVariables[SdThemeVariableLiteral("button.text")] = SdColorWhite;
			colorVariables[SdThemeVariableLiteral("accent")] = { 82, 170, 128, 255 };
			colorVariables[SdThemeVariableLiteral("border")] = { 91, 109, 128, 255 };
			colorVariables[SdThemeVariableLiteral("border.strong")] = { 128, 154, 180, 255 };
			colorVariables[SdThemeVariableLiteral("danger")] = { 164, 66, 66, 255 };
			colorVariables[SdThemeVariableLiteral("selection")] = { 82, 170, 128, 96 };
			metricVariables[SdThemeVariableLiteral("spacing.small")] = 6.0f;
			metricVariables[SdThemeVariableLiteral("spacing.medium")] = 10.0f;
			metricVariables[SdThemeVariableLiteral("font.button")] = 16.0f;
			metricVariables[SdThemeVariableLiteral("radius.small")] = 5.0f;
			metricVariables[SdThemeVariableLiteral("duration.fast")] = 0.16f;
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
				if (value.kind == SdStyleValueKind::ColorVariable)
				{
					outValue = theme.GetColorVariable(value.variableId);
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
				if (value.kind == SdStyleValueKind::MetricVariable)
				{
					outValue = theme.GetMetricVariable(value.variableId);
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
			case SdStyleValueKind::ColorVariable:
			case SdStyleValueKind::MetricVariable:
				return left.variableId == right.variableId;
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
		SdStyleId targetTag = SdWidgetTargetIds::Global;
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
	public:
		template<class TStyle>
		class SdStyleSystemSheetRuleBuilder final
		{
		private:
			SdStyleSystem& system;
			SdStyleSheetRuleBuilder<TStyle> builder;

			void TouchCompiledSheet()
			{
				system.compiledStyleSheet = system.typedStyleSheet.Compile();
				system.Touch();
			}

		public:
			SdStyleSystemSheetRuleBuilder(SdStyleSystem& owner, SdStyleSheetRuleBuilder<TStyle> ruleBuilder)
				: system(owner), builder(ruleBuilder) {}

			SdStyleSystemSheetRuleBuilder& Cascade(SdCascadeLayer cascadeLayer) noexcept
			{
				builder.Cascade(cascadeLayer);
				TouchCompiledSheet();
				return *this;
			}

			SdStyleSystemSheetRuleBuilder& Layer(SdLayerPriority layerPriority) noexcept
			{
				builder.Layer(layerPriority);
				TouchCompiledSheet();
				return *this;
			}

			SdStyleSystemSheetRuleBuilder& Class(SdStyleClassId classId) noexcept
			{
				builder.Class(classId);
				TouchCompiledSheet();
				return *this;
			}

			SdStyleSystemSheetRuleBuilder& Class(const char* className) noexcept
			{
				return Class(SdStyleClassLiteral(className));
			}

			SdStyleSystemSheetRuleBuilder& Scope(SdStyleScopeId scopeId) noexcept
			{
				builder.Scope(scopeId);
				TouchCompiledSheet();
				return *this;
			}

			SdStyleSystemSheetRuleBuilder& Scope(const char* scopeName) noexcept
			{
				return Scope(SdStyleScopeLiteral(scopeName));
			}

			SdStyleSystemSheetRuleBuilder& Pseudo(SdPseudoState pseudoState) noexcept
			{
				builder.Pseudo(pseudoState);
				TouchCompiledSheet();
				return *this;
			}

			SdStyleSystemSheetRuleBuilder& Pseudo(SdPseudoStateFlag pseudoState) noexcept
			{
				builder.Pseudo(pseudoState);
				TouchCompiledSheet();
				return *this;
			}

			SdStyleSystemSheetRuleBuilder& Pseudo(SdStyleInteractionState interactionState) noexcept
			{
				builder.Pseudo(interactionState);
				TouchCompiledSheet();
				return *this;
			}

			SdStyleSystemSheetRuleBuilder& Important() noexcept
			{
				builder.Important();
				TouchCompiledSheet();
				return *this;
			}

			template<class TField>
			SdStyleSystemSheetRuleBuilder& Set(TField TStyle::* member, TField value)
			{
				builder.Set(member, value);
				TouchCompiledSheet();
				return *this;
			}

			template<class TField>
			SdStyleSystemSheetRuleBuilder& Set(TField TStyle::* member, SdStyleValue value)
			{
				builder.Set(member, value);
				TouchCompiledSheet();
				return *this;
			}

			template<class TField>
			SdStyleSystemSheetRuleBuilder& SetImportant(TField TStyle::* member, TField value)
			{
				builder.SetImportant(member, value);
				TouchCompiledSheet();
				return *this;
			}

			template<class TField>
			SdStyleSystemSheetRuleBuilder& SetImportant(TField TStyle::* member, SdStyleValue value)
			{
				builder.SetImportant(member, value);
				TouchCompiledSheet();
				return *this;
			}

		};

	private:
		SdTheme theme = {};
		std::vector<SdTypedStyleRule> typedRules = {};
		SdStyleSheet typedStyleSheet = {};
		SdCompiledStyleSheet compiledStyleSheet = {};
		SdPropertyRegistry propertyRegistry = {};
		SdUInt64 revision = 1;

	public:
		SdStyleSystem()
		{
			RegisterRootProperties();
			InstallDefaultUserAgentStyleSheet();
		}

		const SdTheme& GetTheme() const noexcept
		{
			return theme;
		}

		SdUInt64 GetRevision() const noexcept
		{
			return revision;
		}

		void SetColorVariable(SdThemeVariableId variableId, SdColor color)
		{
			theme.SetColorVariable(variableId, color);
			Touch();
		}

		void SetColorVariable(const char* name, SdColor color)
		{
			SetColorVariable(SdThemeVariableLiteral(name), color);
		}

		void SetMetricVariable(SdThemeVariableId variableId, float value)
		{
			theme.SetMetricVariable(variableId, value);
			Touch();
		}

		void SetMetricVariable(const char* name, float value)
		{
			SetMetricVariable(SdThemeVariableLiteral(name), value);
		}

		void ClearRules()
		{
			typedRules.clear();
			typedStyleSheet.Clear();
			compiledStyleSheet.Clear();
			InstallDefaultUserAgentStyleSheet(false);
			Touch();
		}

		template<class TWidget>
		SdStyleRuleBuilder<TWidget> Rule();

		SdStyleSystemSheetRuleBuilder<SdWidgetRootStyle> RootRule(SdStyleId targetTypeId)
		{
			return SdStyleSystemSheetRuleBuilder<SdWidgetRootStyle>(*this, typedStyleSheet.RuleForTarget<SdWidgetRootStyle>(targetTypeId));
		}

		template<class TWidget>
		SdStyleSystemSheetRuleBuilder<SdWidgetPartStyle> Part(SdStylePart part)
		{
			return SdStyleSystemSheetRuleBuilder<SdWidgetPartStyle>(*this, typedStyleSheet.Part<TWidget>(part));
		}

		SdStyleSystemSheetRuleBuilder<SdWidgetPartStyle> PartRule(SdStyleId targetTypeId, SdStylePart part)
		{
			return SdStyleSystemSheetRuleBuilder<SdWidgetPartStyle>(*this, typedStyleSheet.PartForTarget(targetTypeId, part));
		}

		SdWidgetRootStyle ResolveRootStyle(
			SdStyleId targetTypeId,
			SdStyleInteractionState interactionState,
			SdLayerPriority layerPriority = SdLayerPriority::Content,
			SdSpan<const SdStyleClassId> styleClasses = {},
			SdStyleScopeId styleScope = 0) const
		{
			SdWidgetRootStyle result = BuildDefaultRootStyle();
			return ApplyCompiledTypedRules(
				result,
				targetTypeId,
				SdStylePart::Root(),
				interactionState,
				layerPriority,
				styleClasses,
				styleScope);
		}

		SdWidgetPartStyle ResolvePartStyle(
			SdStyleId targetTypeId,
			SdStylePart part,
			const SdWidgetRootStyle& rootStyle,
			SdStyleInteractionState interactionState,
			SdLayerPriority layerPriority = SdLayerPriority::Content,
			SdSpan<const SdStyleClassId> styleClasses = {},
			SdStyleScopeId styleScope = 0) const
		{
			SdWidgetPartStyle result = {};
			if (result.inheritText)
			{
				result.color = rootStyle.color;
				result.opacity = rootStyle.opacity;
			}
			return ApplyCompiledTypedRules(
				result,
				targetTypeId,
				part,
				interactionState,
				layerPriority,
				styleClasses,
				styleScope);
		}

		template<class TWidget>
		typename TWidget::Style ResolveTypedStyle(
			SdStyleInteractionState interactionState,
			SdLayerPriority layerPriority = SdLayerPriority::Content,
			const typename TWidget::Style* inlineStyle = nullptr) const;

		template<class TWidget>
		typename TWidget::Style ResolveTypedStyle(
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

		const SdCompiledStyleSheet& GetCompiledStyleSheet() const noexcept
		{
			return compiledStyleSheet;
		}

		const SdPropertyRegistry& GetPropertyRegistry() const noexcept
		{
			return propertyRegistry;
		}

	private:
		template<class TWidget>
		static constexpr SdStyleId GetTargetTag() noexcept
		{
			if constexpr (requires { TWidget::TargetTypeId; })
				return TWidget::TargetTypeId;
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

		SdWidgetRootStyle BuildDefaultRootStyle() const
		{
			SdWidgetRootStyle style = {};
			style.color = theme.GetColorVariable(SdThemeVariableLiteral("text"));
			style.backgroundColor = theme.GetColorVariable(SdThemeVariableLiteral("background"));
			style.border = SdBorder::All(SdLength::Pixels(1.0f), theme.GetColorVariable(SdThemeVariableLiteral("border")));
			style.radius = SdLength::Pixels(theme.GetMetricVariable(SdThemeVariableLiteral("radius.small")));
			style.opacity = 1.0f;
			return style;
		}

		SdTypedStyleRule& AddTypedRule(std::type_index styleType, SdStyleId targetTag)
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
			SdStyleId targetTag,
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

		template<class TStyle, class TField>
		void RegisterTypedProperty(TField TStyle::* member)
		{
			(void)member;
		}

		template<class TStyle>
		const SdStyleContract<TStyle>& GetStyleContract() const
		{
			static const SdStyleContract<TStyle> contract = []
			{
				SdStyleContract<TStyle> result = {};
				if constexpr (requires(SdStyleContract<TStyle>& contractRef) { TStyle::Describe(contractRef); })
					TStyle::Describe(result);
				return result;
			}();
			return contract;
		}

		template<class TStyle>
		TStyle ApplyCompiledTypedRules(
			TStyle style,
			SdStyleId targetTag,
			SdStylePart part,
			SdStyleInteractionState interactionState,
			SdLayerPriority layerPriority,
			SdSpan<const SdStyleClassId> styleClasses,
			SdStyleScopeId styleScope) const
		{
			SdStyleResolveRequest request = {};
			request.targetTag = targetTag;
			request.part = part;
			request.pseudoState = SdPseudoState::FromInteraction(interactionState);
			request.layerPriority = layerPriority;
			request.classes = styleClasses;
			request.scope = styleScope;
			return SdStyleResolver::ResolveStyle<TStyle>(
				compiledStyleSheet,
				request,
				propertyRegistry,
				style,
				&SdStyleSystem::ResolveCompiledValue,
				this);
		}

		static SdStyleValue ResolveCompiledValue(const SdStyleValue& value, const void* owner)
		{
			return static_cast<const SdStyleSystem*>(owner)->ResolveValue(value);
		}

		template<class TWidget>
		friend class SdStyleRuleBuilder;

		void Touch() noexcept
		{
			++revision;
			if (revision == 0)
				revision = 1;
		}

		void RegisterRootProperties()
		{
			propertyRegistry.Register<&SdBoxStyle::width>(
				SdStyleFieldImpact::Layout,
				SdStyleInterpolation::Float,
				true);
			propertyRegistry.Register<&SdBoxStyle::height>(
				SdStyleFieldImpact::Layout,
				SdStyleInterpolation::Float,
				true);
			propertyRegistry.Register<&SdBoxStyle::minWidth>(
				SdStyleFieldImpact::Layout,
				SdStyleInterpolation::Float,
				true);
			propertyRegistry.Register<&SdBoxStyle::minHeight>(
				SdStyleFieldImpact::Layout,
				SdStyleInterpolation::Float,
				true);
			propertyRegistry.Register<&SdBoxStyle::padding>(
				SdStyleFieldImpact::Layout,
				SdStyleInterpolation::Float,
				true);
			propertyRegistry.Register<&SdBoxStyle::color>(
				SdStyleFieldImpact::Paint,
				SdStyleInterpolation::Color);
			propertyRegistry.Register<&SdBoxStyle::backgroundColor>(
				SdStyleFieldImpact::Paint,
				SdStyleInterpolation::Color);
			propertyRegistry.Register<&SdBoxStyle::border>(
				SdStyleFieldImpact::Paint,
				SdStyleInterpolation::Color);
			propertyRegistry.Register<&SdBoxStyle::radius>(
				SdStyleFieldImpact::Paint,
				SdStyleInterpolation::Float);
			propertyRegistry.Register<&SdBoxStyle::opacity>(
				SdStyleFieldImpact::Composite,
				SdStyleInterpolation::Float);
			propertyRegistry.Register<&SdBoxStyle::fontSize>(
				SdStyleFieldImpact::Layout,
				SdStyleInterpolation::Float);
			propertyRegistry.Register<&SdBoxStyle::lineHeight>(
				SdStyleFieldImpact::Layout,
				SdStyleInterpolation::Float);
			propertyRegistry.Register<&SdBoxStyle::gap>(
				SdStyleFieldImpact::Layout,
				SdStyleInterpolation::Float,
				true);
		}

		void InstallDefaultUserAgentStyleSheet(bool touchRevision = true)
		{
			auto global = typedStyleSheet.RuleForTarget<SdWidgetRootStyle>(SdWidgetTargetIds::Global);
			global.Cascade(SdCascadeLayer::UserAgent)
				.Set(&SdBoxStyle::color, ThemeColor("text"))
				.Set(&SdBoxStyle::border, ThemeColor("border"))
				.Set(&SdBoxStyle::radius, ThemeMetric("radius.small"))
				.Set(&SdBoxStyle::opacity, 1.0f)
				.Set(&SdBoxStyle::fontSize, ThemeMetric("font.button"))
				.Set(&SdBoxStyle::lineHeight, 0.0f);
			const float mediumSpacing = theme.GetMetricVariable(SdThemeVariableLiteral("spacing.medium"));
			typedStyleSheet.RuleForTarget<SdWidgetRootStyle>(SdWidgetTargetIds::Panel)
				.Cascade(SdCascadeLayer::UserAgent)
				.Set(&SdBoxStyle::width, SdLength::Pixels(240.0f))
				.Set(&SdBoxStyle::height, SdLength::Pixels(120.0f))
				.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ mediumSpacing, mediumSpacing, mediumSpacing, mediumSpacing }))
				.Set(&SdBoxStyle::gap, SdLength::Pixels(theme.GetMetricVariable(SdThemeVariableLiteral("spacing.small"))));
			typedStyleSheet.RuleForTarget<SdWidgetRootStyle>(SdWidgetTargetIds::Window)
				.Cascade(SdCascadeLayer::UserAgent)
				.Set(&SdBoxStyle::width, SdLength::Pixels(420.0f))
				.Set(&SdBoxStyle::height, SdLength::Pixels(260.0f))
				.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ mediumSpacing, 40.0f, mediumSpacing, mediumSpacing }))
				.Set(&SdBoxStyle::fontSize, ThemeMetric("font.button"))
				.Set(&SdBoxStyle::lineHeight, 0.0f)
				.Set(&SdBoxStyle::radius, ThemeMetric("radius.small"))
				.Set(&SdBoxStyle::gap, SdLength::Pixels(theme.GetMetricVariable(SdThemeVariableLiteral("spacing.small"))))
				.Set(&SdBoxStyle::opacity, 1.0f);
			typedStyleSheet.PartForTarget(SdWidgetTargetIds::Window, SdStylePart::Make("Sodium.Window.Part.Titlebar"))
				.Cascade(SdCascadeLayer::UserAgent)
				.Set(&SdBoxStyle::backgroundColor, ThemeColor("button.bg"))
				.Set(&SdBoxStyle::radius, ThemeMetric("radius.small"));
			const float smallSpacing = theme.GetMetricVariable(SdThemeVariableLiteral("spacing.small"));
			typedStyleSheet.RuleForTarget<SdWidgetRootStyle>(SdWidgetTargetIds::Button)
				.Cascade(SdCascadeLayer::UserAgent)
				.Set(&SdBoxStyle::minWidth, SdLength::Pixels(82.0f))
				.Set(&SdBoxStyle::minHeight, SdLength::Pixels(30.0f))
				.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ mediumSpacing, smallSpacing, mediumSpacing, smallSpacing }))
				.Set(&SdBoxStyle::fontSize, ThemeMetric("font.button"))
				.Set(&SdBoxStyle::lineHeight, 0.0f);
			typedStyleSheet.RuleForTarget<SdWidgetRootStyle>(SdWidgetTargetIds::CheckBox)
				.Cascade(SdCascadeLayer::UserAgent)
				.Set(&SdBoxStyle::minHeight, SdLength::Pixels(28.0f))
				.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ smallSpacing, smallSpacing, smallSpacing, smallSpacing }))
				.Set(&SdBoxStyle::fontSize, ThemeMetric("font.button"))
				.Set(&SdBoxStyle::lineHeight, 0.0f)
				.Set(&SdBoxStyle::gap, SdLength::Pixels(smallSpacing))
				.Set(&SdBoxStyle::radius, SdLength::Pixels(std::max(2.0f, theme.GetMetricVariable(SdThemeVariableLiteral("radius.small")) - 1.0f)));
			typedStyleSheet.PartForTarget(SdWidgetTargetIds::CheckBox, SdStylePart::Make("Sodium.CheckBox.Part.Box"))
				.Cascade(SdCascadeLayer::UserAgent)
				.Set(&SdBoxStyle::backgroundColor, ThemeColor("panel.bg"))
				.Set(&SdBoxStyle::border, ThemeColor("border"))
				.Set(&SdBoxStyle::radius, SdLength::Pixels(std::max(2.0f, theme.GetMetricVariable(SdThemeVariableLiteral("radius.small")) - 1.0f)));
			typedStyleSheet.PartForTarget(SdWidgetTargetIds::CheckBox, SdStylePart::Make("Sodium.CheckBox.Part.Box"))
				.Cascade(SdCascadeLayer::UserAgent)
				.Pseudo(SdStyleInteractionState::Hovered)
				.Set(&SdBoxStyle::backgroundColor, ThemeColor("button.bg.hover"));
			typedStyleSheet.PartForTarget(SdWidgetTargetIds::CheckBox, SdStylePart::Make("Sodium.CheckBox.Part.Indicator"))
				.Cascade(SdCascadeLayer::UserAgent)
				.Set(&SdBoxStyle::backgroundColor, ThemeColor("accent"));
			typedStyleSheet.RuleForTarget<SdWidgetRootStyle>(SdWidgetTargetIds::Slider)
				.Cascade(SdCascadeLayer::UserAgent)
				.Set(&SdBoxStyle::width, SdLength::Pixels(180.0f))
				.Set(&SdBoxStyle::height, SdLength::Pixels(30.0f))
				.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ smallSpacing, smallSpacing, smallSpacing, smallSpacing }))
				.Set(&SdBoxStyle::fontSize, ThemeMetric("font.button"))
				.Set(&SdBoxStyle::lineHeight, 0.0f)
				.Set(&SdBoxStyle::gap, SdLength::Pixels(smallSpacing));
			typedStyleSheet.PartForTarget(SdWidgetTargetIds::Slider, SdStylePart::Make("Sodium.Slider.Part.Track"))
				.Cascade(SdCascadeLayer::UserAgent)
				.Set(&SdBoxStyle::backgroundColor, ThemeColor("panel.bg"))
				.Set(&SdBoxStyle::border, ThemeColor("border"))
				.Set(&SdBoxStyle::radius, ThemeMetric("radius.small"));
			typedStyleSheet.PartForTarget(SdWidgetTargetIds::Slider, SdStylePart::Make("Sodium.Slider.Part.Track"))
				.Cascade(SdCascadeLayer::UserAgent)
				.Pseudo(SdStyleInteractionState::Hovered)
				.Set(&SdBoxStyle::backgroundColor, ThemeColor("button.bg.hover"));
			typedStyleSheet.PartForTarget(SdWidgetTargetIds::Slider, SdStylePart::Make("Sodium.Slider.Part.Track"))
				.Cascade(SdCascadeLayer::UserAgent)
				.Pseudo(SdStyleInteractionState::Pressed)
				.Set(&SdBoxStyle::backgroundColor, ThemeColor("button.bg.pressed"));
			typedStyleSheet.PartForTarget(SdWidgetTargetIds::Slider, SdStylePart::Make("Sodium.Slider.Part.Fill"))
				.Cascade(SdCascadeLayer::UserAgent)
				.Set(&SdBoxStyle::backgroundColor, ThemeColor("accent"));
			typedStyleSheet.PartForTarget(SdWidgetTargetIds::Slider, SdStylePart::Make("Sodium.Slider.Part.Thumb"))
				.Cascade(SdCascadeLayer::UserAgent)
				.Set(&SdBoxStyle::backgroundColor, ThemeColor("accent"))
				.Set(&SdBoxStyle::border, ThemeColor("border"));
			typedStyleSheet.RuleForTarget<SdWidgetRootStyle>(SdWidgetTargetIds::TextInput)
				.Cascade(SdCascadeLayer::UserAgent)
				.Set(&SdBoxStyle::width, SdLength::Pixels(220.0f))
				.Set(&SdBoxStyle::minHeight, SdLength::Pixels(32.0f))
				.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ mediumSpacing, smallSpacing, mediumSpacing, smallSpacing }))
				.Set(&SdBoxStyle::fontSize, ThemeMetric("font.button"))
				.Set(&SdBoxStyle::lineHeight, 0.0f);
			typedStyleSheet.PartForTarget(SdWidgetTargetIds::TextInput, SdStylePart::Make("Sodium.TextInput.Part.Placeholder"))
				.Cascade(SdCascadeLayer::UserAgent)
				.Set(&SdBoxStyle::opacity, 0.52f);
			typedStyleSheet.RuleForTarget<SdWidgetRootStyle>(SdWidgetTargetIds::ImageViewer)
				.Cascade(SdCascadeLayer::UserAgent)
				.Set(&SdBoxStyle::width, SdLength::Pixels(160.0f))
				.Set(&SdBoxStyle::height, SdLength::Pixels(120.0f));
			typedStyleSheet.RuleForTarget<SdWidgetRootStyle>(SdWidgetTargetIds::ScrollView)
				.Cascade(SdCascadeLayer::UserAgent)
				.Set(&SdBoxStyle::width, SdLength::Pixels(240.0f))
				.Set(&SdBoxStyle::height, SdLength::Pixels(160.0f))
				.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ smallSpacing, smallSpacing, smallSpacing, smallSpacing }))
				.Set(&SdBoxStyle::gap, SdLength::Pixels(smallSpacing));
			typedStyleSheet.PartForTarget(SdWidgetTargetIds::ScrollView, SdStylePart::Make("Sodium.ScrollView.Part.Thumb"))
				.Cascade(SdCascadeLayer::UserAgent)
				.Set(&SdBoxStyle::backgroundColor, ThemeColor("accent"));
			typedStyleSheet.RuleForTarget<SdWidgetRootStyle>(SdWidgetTargetIds::Popup)
				.Cascade(SdCascadeLayer::UserAgent)
				.Set(&SdBoxStyle::width, SdLength::Pixels(220.0f))
				.Set(&SdBoxStyle::height, SdLength::Pixels(140.0f))
				.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ smallSpacing, smallSpacing, smallSpacing, smallSpacing }))
				.Set(&SdBoxStyle::gap, SdLength::Pixels(smallSpacing));
			typedStyleSheet.RuleForTarget<SdWidgetRootStyle>(SdWidgetTargetIds::ContextMenu)
				.Cascade(SdCascadeLayer::UserAgent)
				.Set(&SdBoxStyle::width, SdLength::Pixels(220.0f))
				.Set(&SdBoxStyle::height, SdLength::Pixels(140.0f))
				.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ smallSpacing, smallSpacing, smallSpacing, smallSpacing }))
				.Set(&SdBoxStyle::gap, SdLength::Pixels(smallSpacing));
			typedStyleSheet.RuleForTarget<SdWidgetRootStyle>(SdWidgetTargetIds::Tooltip)
				.Cascade(SdCascadeLayer::UserAgent)
				.Set(&SdBoxStyle::padding, SdStyleValue::FromSpacing({ smallSpacing, smallSpacing, smallSpacing, smallSpacing }))
				.Set(&SdBoxStyle::fontSize, ThemeMetric("font.button"))
				.Set(&SdBoxStyle::lineHeight, 0.0f);

			AddDefaultRootBackgroundRule(SdWidgetTargetIds::Default, SdStyleInteractionState::Normal, SdLayerPriority::Content, "background");
			AddDefaultRootBackgroundRule(SdWidgetTargetIds::Panel, SdStyleInteractionState::Normal, SdLayerPriority::Content, "panel.bg");
			AddDefaultRootBackgroundRule(SdWidgetTargetIds::Button, SdStyleInteractionState::Normal, SdLayerPriority::Content, "button.bg");
			AddDefaultRootBackgroundRule(SdWidgetTargetIds::Button, SdStyleInteractionState::Hovered, SdLayerPriority::Content, "button.bg.hover");
			AddDefaultRootBackgroundRule(SdWidgetTargetIds::Button, SdStyleInteractionState::Pressed, SdLayerPriority::Content, "button.bg.pressed");
			AddDefaultRootBackgroundRule(SdWidgetTargetIds::CheckBox, SdStyleInteractionState::Normal, SdLayerPriority::Content, "panel.bg");
			AddDefaultRootBackgroundRule(SdWidgetTargetIds::CheckBox, SdStyleInteractionState::Hovered, SdLayerPriority::Content, "button.bg.hover");
			AddDefaultRootBackgroundRule(SdWidgetTargetIds::Window, SdStyleInteractionState::Normal, SdLayerPriority::Floating, "window.bg", true);
			AddDefaultRootBackgroundRule(SdWidgetTargetIds::ImageViewer, SdStyleInteractionState::Normal, SdLayerPriority::Content, "panel.bg");
			AddDefaultRootBackgroundRule(SdWidgetTargetIds::Slider, SdStyleInteractionState::Normal, SdLayerPriority::Content, "panel.bg");
			AddDefaultRootBackgroundRule(SdWidgetTargetIds::Slider, SdStyleInteractionState::Hovered, SdLayerPriority::Content, "button.bg.hover");
			AddDefaultRootBackgroundRule(SdWidgetTargetIds::Slider, SdStyleInteractionState::Pressed, SdLayerPriority::Content, "button.bg.pressed");
			AddDefaultRootBackgroundRule(SdWidgetTargetIds::TextInput, SdStyleInteractionState::Normal, SdLayerPriority::Content, "panel.bg");
			AddDefaultRootBackgroundRule(SdWidgetTargetIds::TextInput, SdStyleInteractionState::Focused, SdLayerPriority::Content, "button.bg");
			AddDefaultRootBackgroundRule(SdWidgetTargetIds::ScrollView, SdStyleInteractionState::Normal, SdLayerPriority::Content, "panel.bg");
			AddDefaultRootBackgroundRule(SdWidgetTargetIds::Popup, SdStyleInteractionState::Normal, SdLayerPriority::Popup, "window.bg", true);
			AddDefaultRootBackgroundRule(SdWidgetTargetIds::ContextMenu, SdStyleInteractionState::Normal, SdLayerPriority::Popup, "window.bg", true);
			AddDefaultRootBackgroundRule(SdWidgetTargetIds::Tooltip, SdStyleInteractionState::Normal, SdLayerPriority::Overlay, "panel.bg", true);

			compiledStyleSheet = typedStyleSheet.Compile();
			if (touchRevision)
				Touch();
		}

		void AddDefaultRootBackgroundRule(
			SdStyleId targetTag,
			SdStyleInteractionState interactionState,
			SdLayerPriority layerPriority,
			const char* backgroundVariable,
			bool matchLayer = false)
		{
			auto rule = typedStyleSheet.RuleForTarget<SdWidgetRootStyle>(targetTag);
			rule.Cascade(SdCascadeLayer::UserAgent)
				.Pseudo(interactionState)
				.Set(&SdBoxStyle::backgroundColor, ThemeColor(backgroundVariable));
			if (matchLayer)
				rule.Layer(layerPriority);
		}

			SdStyleValue ResolveValue(const SdStyleValue& value) const
		{
			switch (value.kind)
			{
			case SdStyleValueKind::ColorVariable:
				return SdStyleValue::FromColor(theme.GetColorVariable(value.variableId));
			case SdStyleValueKind::MetricVariable:
				return SdStyleValue::FromFloat(theme.GetMetricVariable(value.variableId));
			default:
				return value;
			}
		}

		bool MatchesTypedRule(
			const SdTypedStyleRule& rule,
			std::type_index styleType,
			SdStyleId targetTag,
			SdStyleInteractionState interactionState,
			SdLayerPriority layerPriority,
			SdSpan<const SdStyleClassId> styleClasses,
			SdStyleScopeId styleScope) const noexcept
		{
			if (rule.styleType != styleType)
				return false;
			if (rule.targetTag != SdWidgetTargetIds::Global && rule.targetTag != targetTag)
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
		SdStyleRuleBuilder& Set(TField Style::* member, SdStyleValue value)
		{
			return SetValue(member, value);
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
				system.template RegisterTypedProperty<Style>(member);
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
				system.typedStyleSheet.Rule<TWidget>().Set(member, value);
				system.compiledStyleSheet = system.typedStyleSheet.Compile();
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
	typename TWidget::Style SdStyleSystem::ResolveTypedStyle(
		SdStyleInteractionState interactionState,
		SdLayerPriority layerPriority,
		const typename TWidget::Style* inlineStyle) const
	{
		return ResolveTypedStyle<TWidget>(interactionState, layerPriority, {}, 0, inlineStyle);
	}

	template<class TWidget>
	typename TWidget::Style SdStyleSystem::ResolveTypedStyle(
		SdStyleInteractionState interactionState,
		SdLayerPriority layerPriority,
		SdSpan<const SdStyleClassId> styleClasses,
		SdStyleScopeId styleScope,
		const typename TWidget::Style* inlineStyle) const
	{
		using Style = typename TWidget::Style;
		Style result = BuildDefaultStyle<TWidget>();
		result = ApplyCompiledTypedRules(
			result,
			GetTargetTag<TWidget>(),
			SdStylePart::Root(),
			interactionState,
			layerPriority,
			styleClasses,
			styleScope);
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
		return GetStyleContract<Style>();
	}
}
