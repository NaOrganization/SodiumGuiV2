#pragma once

#include "SdStyleProperty.h"

#include <typeindex>
#include <vector>

namespace Sodium
{
	enum class SdCascadeLayer : SdUInt8
	{
		UserAgent,
		Theme,
		User,
		Scoped,
		Inline
	};

	struct SdStyleSpecificity final
	{
		SdUInt16 ids = 0;
		SdUInt16 classes = 0;
		SdUInt16 types = 0;

		constexpr SdUInt32 Pack() const noexcept
		{
			return (static_cast<SdUInt32>(ids) << 20)
				| (static_cast<SdUInt32>(classes) << 10)
				| static_cast<SdUInt32>(types);
		}
	};

	struct SdCompiledSelector final
	{
		std::type_index styleType = std::type_index(typeid(void));
		SdStyleId targetTag = SdWidgetTargetIds::Global;
		SdStylePart part = SdStylePart::Root();
		SdPseudoState pseudoState = {};
		SdLayerPriority layerPriority = SdLayerPriority::Content;
		SdStyleClassId classId = 0;
		SdStyleScopeId scopeId = 0;
		bool matchPart = false;
		bool matchPseudo = false;
		bool matchLayer = false;
		bool matchClass = false;
		bool matchScope = false;
	};

	struct SdCompiledDeclaration final
	{
		std::type_index styleType = std::type_index(typeid(void));
		SdPropertyId propertyId = 0;
		SdStyleValue value = {};
		bool important = false;
	};

	struct SdCompiledStyleRule final
	{
		SdCompiledSelector selector = {};
		SdCascadeLayer cascadeLayer = SdCascadeLayer::User;
		SdStyleSpecificity specificity = {};
		SdUInt32 sourceOrder = 0;
		std::vector<SdCompiledDeclaration> declarations = {};
	};

	class SdCompiledStyleSheet final
	{
	private:
		std::vector<SdCompiledStyleRule> rules = {};

	public:
		void Clear()
		{
			rules.clear();
		}

		void AddRule(const SdCompiledStyleRule& rule)
		{
			rules.push_back(rule);
		}

		const std::vector<SdCompiledStyleRule>& GetRules() const noexcept
		{
			return rules;
		}
	};

	template<class TStyle>
	class SdStyleSheetRuleBuilder final
	{
	private:
		SdCompiledStyleRule& rule;

	public:
		explicit SdStyleSheetRuleBuilder(SdCompiledStyleRule& targetRule)
			: rule(targetRule) {}

		SdStyleSheetRuleBuilder& Cascade(SdCascadeLayer cascadeLayer) noexcept
		{
			rule.cascadeLayer = cascadeLayer;
			return *this;
		}

		SdStyleSheetRuleBuilder& Layer(SdLayerPriority layerPriority) noexcept
		{
			rule.selector.layerPriority = layerPriority;
			rule.selector.matchLayer = true;
			return *this;
		}

		SdStyleSheetRuleBuilder& Class(SdStyleClassId classId) noexcept
		{
			rule.selector.classId = classId;
			rule.selector.matchClass = true;
			++rule.specificity.classes;
			return *this;
		}

		SdStyleSheetRuleBuilder& Class(const char* className) noexcept
		{
			return Class(SdStyleClassLiteral(className));
		}

		SdStyleSheetRuleBuilder& Scope(SdStyleScopeId scopeId) noexcept
		{
			rule.selector.scopeId = scopeId;
			rule.selector.matchScope = true;
			++rule.specificity.classes;
			return *this;
		}

		SdStyleSheetRuleBuilder& Scope(const char* scopeName) noexcept
		{
			return Scope(SdStyleScopeLiteral(scopeName));
		}

		SdStyleSheetRuleBuilder& Pseudo(SdPseudoState pseudoState) noexcept
		{
			rule.selector.pseudoState = pseudoState;
			rule.selector.matchPseudo = pseudoState.bits != 0;
			if (rule.selector.matchPseudo)
				++rule.specificity.classes;
			return *this;
		}

		SdStyleSheetRuleBuilder& Pseudo(SdPseudoStateFlag pseudoState) noexcept
		{
			SdPseudoState state = {};
			state.Set(pseudoState);
			return Pseudo(state);
		}

		SdStyleSheetRuleBuilder& Pseudo(SdStyleInteractionState interactionState) noexcept
		{
			return Pseudo(SdPseudoState::FromInteraction(interactionState));
		}

		SdStyleSheetRuleBuilder& Important() noexcept
		{
			for (SdCompiledDeclaration& declaration : rule.declarations)
				declaration.important = true;
			return *this;
		}

		template<class TOwner, class TField>
		SdStyleSheetRuleBuilder& Set(TField TOwner::* member, TField value)
		{
			return SetValue(member, Detail::MakePropertyValue(value), false);
		}

		template<class TOwner, class TField>
		SdStyleSheetRuleBuilder& Set(TField TOwner::* member, SdStyleValue value)
		{
			return SetValue(member, value, false);
		}

		template<class TOwner, class TField>
		SdStyleSheetRuleBuilder& SetImportant(TField TOwner::* member, TField value)
		{
			return SetValue(member, Detail::MakePropertyValue(value), true);
		}

		template<class TOwner, class TField>
		SdStyleSheetRuleBuilder& SetImportant(TField TOwner::* member, SdStyleValue value)
		{
			return SetValue(member, value, true);
		}

	private:
		template<class TOwner, class TField>
		SdStyleSheetRuleBuilder& SetValue(TField TOwner::* member, SdStyleValue value, bool important)
		{
			SdCompiledDeclaration declaration = {};
			declaration.styleType = std::type_index(typeid(TStyle));
			declaration.propertyId = Detail::SdStylePropertyId(member);
			declaration.value = value;
			declaration.important = important;
			rule.declarations.push_back(declaration);
			return *this;
		}
	};

	class SdStyleSheet final
	{
	private:
		std::vector<SdCompiledStyleRule> rules = {};
		SdUInt32 nextSourceOrder = 0;

		template<class TWidget, class TStyle>
		SdCompiledStyleRule& AddRule(SdStylePart part)
		{
			SdCompiledStyleRule rule = {};
			rule.selector.styleType = std::type_index(typeid(TStyle));
			if constexpr (requires { TWidget::TargetTypeId; })
				rule.selector.targetTag = TWidget::TargetTypeId;
			else
				rule.selector.targetTag = SdStyleIdLiteral(typeid(TWidget).name());
			rule.selector.part = part;
			rule.selector.matchPart = !part.IsRoot();
			rule.specificity.types = 1;
			if (rule.selector.matchPart)
				++rule.specificity.classes;
			rule.sourceOrder = nextSourceOrder++;
			rules.push_back(std::move(rule));
			return rules.back();
		}

	public:
		template<class TStyle>
		SdStyleSheetRuleBuilder<TStyle> RuleForTarget(SdStyleId targetTag)
		{
			struct TargetStyleWidget final
			{
				using Style = TStyle;
			};

			SdCompiledStyleRule& rule = AddRule<TargetStyleWidget, TStyle>(SdStylePart::Root());
			rule.selector.targetTag = targetTag;
			return SdStyleSheetRuleBuilder<TStyle>(rule);
		}

		template<class TWidget>
		SdStyleSheetRuleBuilder<typename TWidget::Style> Rule()
		{
			return SdStyleSheetRuleBuilder<typename TWidget::Style>(AddRule<TWidget, typename TWidget::Style>(SdStylePart::Root()));
		}

		template<class TWidget>
		SdStyleSheetRuleBuilder<SdWidgetPartStyle> Part(SdStylePart part)
		{
			return SdStyleSheetRuleBuilder<SdWidgetPartStyle>(AddRule<TWidget, SdWidgetPartStyle>(part));
		}

		SdCompiledStyleSheet Compile() const
		{
			SdCompiledStyleSheet compiled = {};
			for (const SdCompiledStyleRule& rule : rules)
				compiled.AddRule(rule);
			return compiled;
		}

		const std::vector<SdCompiledStyleRule>& GetRules() const noexcept
		{
			return rules;
		}

		void Clear()
		{
			rules.clear();
			nextSourceOrder = 0;
		}
	};
}
