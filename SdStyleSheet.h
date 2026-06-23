#pragma once

#include "SdStyleProperty.h"

#include <typeindex>
#include <unordered_map>
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
		SdRootLayer rootLayer = SdRootLayer::Content;
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

	struct SdCompiledTransition final
	{
		std::type_index styleType = std::type_index(typeid(void));
		SdPropertyId propertyId = 0;
		SdTransition transition = {};
		bool important = false;
	};

	struct SdCompiledStyleRule final
	{
		SdCompiledSelector selector = {};
		SdCascadeLayer cascadeLayer = SdCascadeLayer::User;
		SdStyleSpecificity specificity = {};
		SdUInt32 sourceOrder = 0;
		std::vector<SdCompiledDeclaration> declarations = {};
		std::vector<SdCompiledTransition> transitions = {};
	};

	struct SdCompiledRuleBucketKey final
	{
		std::type_index styleType = std::type_index(typeid(void));
		SdStyleId targetTag = SdWidgetTargetIds::Global;
		SdStylePart part = SdStylePart::Root();
		bool matchPart = false;

		friend bool operator==(const SdCompiledRuleBucketKey& left, const SdCompiledRuleBucketKey& right) noexcept
		{
			return left.styleType == right.styleType
				&& left.targetTag == right.targetTag
				&& left.part == right.part
				&& left.matchPart == right.matchPart;
		}
	};

	struct SdCompiledRuleBucketKeyHash final
	{
		SdSize operator()(const SdCompiledRuleBucketKey& key) const noexcept
		{
			SdSize hash = key.styleType.hash_code();
			hash ^= static_cast<SdSize>(key.targetTag + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2));
			hash ^= static_cast<SdSize>(key.part.value + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2));
			hash ^= static_cast<SdSize>((key.matchPart ? 1u : 0u) + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2));
			return hash;
		}
	};

	struct SdCompiledRuleBucket final
	{
		SdCompiledRuleBucketKey key = {};
		std::vector<SdUInt32> ruleIndices = {};
	};

	class SdCompiledStyleSheet final
	{
	private:
		std::vector<SdCompiledStyleRule> rules = {};
		std::vector<SdCompiledRuleBucket> buckets = {};
		std::unordered_map<SdCompiledRuleBucketKey, SdSize, SdCompiledRuleBucketKeyHash> bucketIndexByKey = {};

		static SdCompiledRuleBucketKey MakeBucketKey(const SdCompiledSelector& selector) noexcept
		{
			return {
				selector.styleType,
				selector.targetTag,
				selector.matchPart ? selector.part : SdStylePart::Root(),
				selector.matchPart
			};
		}

		const SdCompiledRuleBucket* FindBucket(const SdCompiledRuleBucketKey& key) const noexcept
		{
			const auto it = bucketIndexByKey.find(key);
			if (it == bucketIndexByKey.end())
				return nullptr;
			return &buckets[it->second];
		}

		void AddRuleToBucket(SdUInt32 ruleIndex)
		{
			const SdCompiledRuleBucketKey key = MakeBucketKey(rules[ruleIndex].selector);
			auto it = bucketIndexByKey.find(key);
			if (it == bucketIndexByKey.end())
			{
				SdCompiledRuleBucket bucket = {};
				bucket.key = key;
				bucket.ruleIndices.push_back(ruleIndex);
				buckets.push_back(std::move(bucket));
				bucketIndexByKey.emplace(key, buckets.size() - 1);
				return;
			}
			buckets[it->second].ruleIndices.push_back(ruleIndex);
		}

	public:
		void Clear()
		{
			rules.clear();
			buckets.clear();
			bucketIndexByKey.clear();
		}

		void AddRule(const SdCompiledStyleRule& rule)
		{
			rules.push_back(rule);
			AddRuleToBucket(static_cast<SdUInt32>(rules.size() - 1));
		}

		const std::vector<SdCompiledStyleRule>& GetRules() const noexcept
		{
			return rules;
		}

		const std::vector<SdCompiledRuleBucket>& GetBuckets() const noexcept
		{
			return buckets;
		}

		template<class TCallback>
		void ForEachCandidateRule(
			std::type_index styleType,
			SdStyleId targetTag,
			SdStylePart part,
			TCallback&& callback) const
		{
			const std::vector<SdUInt32>* visitedBuckets[4] = {};
			SdSize visitedBucketCount = 0;

			const auto visitBucket = [&](SdStyleId bucketTargetTag, SdStylePart bucketPart, bool matchPart)
			{
				const SdCompiledRuleBucketKey key{ styleType, bucketTargetTag, bucketPart, matchPart };
				const SdCompiledRuleBucket* bucket = FindBucket(key);
				if (!bucket)
					return;
				for (SdSize index = 0; index < visitedBucketCount; ++index)
				{
					if (visitedBuckets[index] == &bucket->ruleIndices)
						return;
				}

				visitedBuckets[visitedBucketCount++] = &bucket->ruleIndices;
				for (SdUInt32 ruleIndex : bucket->ruleIndices)
					callback(rules[ruleIndex]);
			};

			visitBucket(targetTag, SdStylePart::Root(), false);
			visitBucket(targetTag, part, true);
			if (targetTag != SdWidgetTargetIds::Global)
			{
				visitBucket(SdWidgetTargetIds::Global, SdStylePart::Root(), false);
				visitBucket(SdWidgetTargetIds::Global, part, true);
			}
		}

		SdSize CountCandidateRules(std::type_index styleType, SdStyleId targetTag, SdStylePart part) const
		{
			SdSize count = 0;
			ForEachCandidateRule(styleType, targetTag, part, [&count](const SdCompiledStyleRule&)
			{
				++count;
			});
			return count;
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

		SdStyleSheetRuleBuilder& Layer(SdRootLayer rootLayer) noexcept
		{
			rule.selector.rootLayer = rootLayer;
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

		SdStyleSheetRuleBuilder& Scope(SdStyleScopeId scopeId) noexcept
		{
			rule.selector.scopeId = scopeId;
			rule.selector.matchScope = true;
			++rule.specificity.classes;
			return *this;
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
			for (SdCompiledTransition& transition : rule.transitions)
				transition.important = true;
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

		template<class TOwner, class TField>
		SdStyleSheetRuleBuilder& Transition(TField TOwner::* member, SdDuration duration, SdAnimationEasing easing)
		{
			return TransitionValue(member, { duration, easing }, false);
		}

		template<class TOwner, class TField>
		SdStyleSheetRuleBuilder& Transition(TField TOwner::* member, SdDuration duration, SdDuration delay, SdAnimationEasing easing)
		{
			return TransitionValue(member, { duration, easing, delay }, false);
		}

		template<class TOwner, class TField>
		SdStyleSheetRuleBuilder& Transition(TField TOwner::* member, SdDuration duration, SdDuration delay, SdAnimationEasing easing, SdTransitionBehavior behavior)
		{
			return TransitionValue(member, { duration, easing, delay, behavior }, false);
		}

		template<class TOwner, class TField>
		SdStyleSheetRuleBuilder& TransitionImportant(TField TOwner::* member, SdDuration duration, SdAnimationEasing easing)
		{
			return TransitionValue(member, { duration, easing }, true);
		}

		template<class TOwner, class TField>
		SdStyleSheetRuleBuilder& TransitionImportant(TField TOwner::* member, SdDuration duration, SdDuration delay, SdAnimationEasing easing)
		{
			return TransitionValue(member, { duration, easing, delay }, true);
		}

		template<class TOwner, class TField>
		SdStyleSheetRuleBuilder& TransitionImportant(TField TOwner::* member, SdDuration duration, SdDuration delay, SdAnimationEasing easing, SdTransitionBehavior behavior)
		{
			return TransitionValue(member, { duration, easing, delay, behavior }, true);
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

		template<class TOwner, class TField>
		SdStyleSheetRuleBuilder& TransitionValue(TField TOwner::* member, SdTransition transition, bool important)
		{
			SdCompiledTransition compiledTransition = {};
			compiledTransition.styleType = std::type_index(typeid(TStyle));
			compiledTransition.propertyId = Detail::SdStylePropertyId(member);
			compiledTransition.transition = transition;
			compiledTransition.important = important;
			rule.transitions.push_back(compiledTransition);
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
				rule.selector.targetTag = Detail::SdTypeHash<TWidget>();
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

		SdStyleSheetRuleBuilder<SdWidgetPartStyle> PartForTarget(SdStyleId targetTag, SdStylePart part)
		{
			struct TargetPartStyleWidget final
			{
				using Style = SdWidgetPartStyle;
			};

			SdCompiledStyleRule& rule = AddRule<TargetPartStyleWidget, SdWidgetPartStyle>(part);
			rule.selector.targetTag = targetTag;
			return SdStyleSheetRuleBuilder<SdWidgetPartStyle>(rule);
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
