#pragma once

#include "SdStyleSheet.h"

namespace Sodium
{
	struct SdStyleResolveRequest final
	{
		SdStyleId targetTag = SdWidgetTargetIds::Default;
		SdStylePart part = SdStylePart::Root();
		SdPseudoState pseudoState = {};
		SdLayerPriority layerPriority = SdLayerPriority::Content;
		SdSpan<const SdStyleClassId> classes = {};
		SdStyleScopeId scope = 0;
	};

	struct SdStyleResolveResult final
	{
		SdBoxStyle specifiedStyle = {};
		SdBoxStyle resolvedStyle = {};
		SdBoxStyle presentationStyle = {};
	};

	class SdStyleResolver final
	{
	public:
		using ResolveValueFn = SdStyleValue(*)(const SdStyleValue&, const void*);

		static bool SelectorMatches(const SdCompiledSelector& selector, const SdStyleResolveRequest& request) noexcept
		{
			if (selector.targetTag != SdWidgetTargetIds::Global && selector.targetTag != request.targetTag)
				return false;
			if (selector.matchPart && selector.part != request.part)
				return false;
			if (selector.matchPseudo && (request.pseudoState.bits & selector.pseudoState.bits) != selector.pseudoState.bits)
				return false;
			if (selector.matchLayer && selector.layerPriority != request.layerPriority)
				return false;
			if (selector.matchClass)
			{
				bool found = false;
				for (SdStyleClassId styleClass : request.classes)
				{
					if (styleClass == selector.classId)
					{
						found = true;
						break;
					}
				}
				if (!found)
					return false;
			}
			if (selector.matchScope && selector.scopeId != request.scope)
				return false;
			return true;
		}

		static bool HasHigherCascadePriority(
			bool leftImportant,
			SdCascadeLayer leftLayer,
			SdStyleSpecificity leftSpecificity,
			SdUInt32 leftSourceOrder,
			bool rightImportant,
			SdCascadeLayer rightLayer,
			SdStyleSpecificity rightSpecificity,
			SdUInt32 rightSourceOrder) noexcept
		{
			if (leftImportant != rightImportant)
				return leftImportant;
			if (leftLayer != rightLayer)
				return static_cast<SdUInt8>(leftLayer) > static_cast<SdUInt8>(rightLayer);
			if (leftSpecificity.Pack() != rightSpecificity.Pack())
				return leftSpecificity.Pack() > rightSpecificity.Pack();
			return leftSourceOrder >= rightSourceOrder;
		}

		template<class TStyle>
		static TStyle ResolveStyle(
			const SdCompiledStyleSheet& sheet,
			const SdStyleResolveRequest& request,
			const SdPropertyRegistry& registry,
			TStyle baseStyle,
			ResolveValueFn resolveValue = nullptr,
			const void* resolveContext = nullptr)
		{
			struct AppliedDeclaration final
			{
				SdPropertyId propertyId = 0;
				SdStyleValue value = {};
				bool important = false;
				SdCascadeLayer layer = SdCascadeLayer::UserAgent;
				SdStyleSpecificity specificity = {};
				SdUInt32 sourceOrder = 0;
			};

			std::vector<AppliedDeclaration> applied = {};
			sheet.ForEachCandidateRule(std::type_index(typeid(TStyle)), request.targetTag, request.part, [&](const SdCompiledStyleRule& rule)
			{
				if (rule.selector.styleType != std::type_index(typeid(TStyle)))
					return;
				if (!SelectorMatches(rule.selector, request))
					return;

				for (const SdCompiledDeclaration& declaration : rule.declarations)
				{
					if (declaration.styleType != std::type_index(typeid(TStyle)))
						continue;

					const SdStyleValue resolvedValue = resolveValue ? resolveValue(declaration.value, resolveContext) : declaration.value;
					AppliedDeclaration candidate = {};
					candidate.propertyId = declaration.propertyId;
					candidate.value = resolvedValue;
					candidate.important = declaration.important;
					candidate.layer = rule.cascadeLayer;
					candidate.specificity = rule.specificity;
					candidate.sourceOrder = rule.sourceOrder;

					bool replaced = false;
					for (AppliedDeclaration& existing : applied)
					{
						if (existing.propertyId != candidate.propertyId)
							continue;
						if (HasHigherCascadePriority(
							candidate.important,
							candidate.layer,
							candidate.specificity,
							candidate.sourceOrder,
							existing.important,
							existing.layer,
							existing.specificity,
							existing.sourceOrder))
						{
							existing = candidate;
						}
						replaced = true;
						break;
					}

					if (!replaced)
						applied.push_back(candidate);
				}
			});

			for (const AppliedDeclaration& declaration : applied)
			{
				const SdPropertyDescriptor* property = registry.Find(declaration.propertyId, std::type_index(typeid(TStyle)));
				if (property && property->writeValue)
					property->writeValue(&baseStyle, declaration.value);
			}
			return baseStyle;
		}

		static void WriteNode(SdStyleNode& node, const SdStyleResolveResult& result)
		{
			node.specifiedStyle = result.specifiedStyle;
			node.resolvedStyle = result.resolvedStyle;
			node.presentationStyle = result.presentationStyle;
			++node.revision;
			if (node.revision == 0)
				node.revision = 1;
		}
	};
}
