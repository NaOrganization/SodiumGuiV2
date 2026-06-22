#pragma once

#include "SdStyleCore.h"

#include <typeindex>
#include <unordered_map>
#include <vector>

namespace Sodium
{
	struct SdPropertyDescriptor final
	{
		SdPropertyId propertyId = 0;
		std::type_index styleType = std::type_index(typeid(void));
		SdStyleFieldImpact impact = SdStyleFieldImpact::Discrete;
		SdStyleInterpolation interpolation = SdStyleInterpolation::None;
		bool expensiveTransition = false;
		bool(*equals)(const void*, const void*) = nullptr;
		void(*copy)(void*, const void*) = nullptr;
		SdStyleValue(*readValue)(const void*) = nullptr;
		void(*writeValue)(void*, const SdStyleValue&) = nullptr;
	};

	namespace Detail
	{
		template<auto>
		struct SdMemberPointerTraits;

		template<class TStyle, class TField, TField TStyle::* Member>
		struct SdMemberPointerTraits<Member> final
		{
			using Style = TStyle;
			using Field = TField;
			static constexpr TField TStyle::* Value = Member;
		};

		template<class TStyle, class TField>
		SdPropertyId SdStylePropertyId(TField TStyle::* member) noexcept
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
		inline constexpr bool SdPropertyValueField = std::is_enum_v<T>;

		template<>
		inline constexpr bool SdPropertyValueField<SdColor> = true;

		template<>
		inline constexpr bool SdPropertyValueField<float> = true;

		template<>
		inline constexpr bool SdPropertyValueField<SdSpacing> = true;

		template<>
		inline constexpr bool SdPropertyValueField<SdVec2> = true;

		template<>
		inline constexpr bool SdPropertyValueField<SdLength> = true;

		template<>
		inline constexpr bool SdPropertyValueField<SdBoxEdges> = true;

		template<>
		inline constexpr bool SdPropertyValueField<SdBorder> = true;

		template<class T>
		SdStyleValue MakePropertyValue(T value) noexcept
		{
			if constexpr (std::is_same_v<T, SdColor>)
				return SdStyleValue::FromColor(value);
			else if constexpr (std::is_same_v<T, float>)
				return SdStyleValue::FromFloat(value);
			else if constexpr (std::is_same_v<T, SdSpacing>)
				return SdStyleValue::FromSpacing(value);
			else if constexpr (std::is_same_v<T, SdVec2>)
				return SdStyleValue::FromVec2(value);
			else if constexpr (std::is_same_v<T, SdLength>)
				return SdStyleValueFromLength(value);
			else if constexpr (std::is_same_v<T, SdBoxEdges>)
				return SdStyleValue::FromSpacing({
					value.left.value,
					value.top.value,
					value.right.value,
					value.bottom.value });
			else if constexpr (std::is_same_v<T, SdBorder>)
				return SdStyleValue::FromColor(value.left.color);
			else if constexpr (std::is_enum_v<T>)
				return SdStyleValueFromEnum(value);
			else
				return {};
		}

		template<class T>
		bool TryWritePropertyValue(T& destination, const SdStyleValue& value) noexcept
		{
			if constexpr (std::is_same_v<T, SdColor>)
			{
				if (value.kind != SdStyleValueKind::Color)
					return false;
				destination = value.color;
				return true;
			}
			else if constexpr (std::is_same_v<T, float>)
			{
				if (value.kind != SdStyleValueKind::Float)
					return false;
				destination = value.number;
				return true;
			}
			else if constexpr (std::is_same_v<T, SdSpacing>)
			{
				if (value.kind != SdStyleValueKind::Spacing)
					return false;
				destination = value.spacing;
				return true;
			}
			else if constexpr (std::is_same_v<T, SdVec2>)
			{
				if (value.kind != SdStyleValueKind::Vec2)
					return false;
				destination = value.vec2;
				return true;
			}
			else if constexpr (std::is_same_v<T, SdLength>)
			{
				destination = SdStyleValueToLength(value);
				return true;
			}
			else if constexpr (std::is_same_v<T, SdBoxEdges>)
			{
				if (value.kind != SdStyleValueKind::Spacing)
					return false;
				destination = SdBoxEdges::FromSpacing(value.spacing);
				return true;
			}
			else if constexpr (std::is_same_v<T, SdBorder>)
			{
				if (value.kind != SdStyleValueKind::Color)
					return false;
				destination = SdBorder::All(SdLength::Pixels(1.0f), value.color);
				return true;
			}
			else if constexpr (std::is_enum_v<T>)
			{
				return SdStyleValueToEnum(value, destination);
			}
			else
			{
				return false;
			}
		}

		template<auto Member>
		bool PropertyEquals(const void* left, const void* right)
		{
			using Traits = SdMemberPointerTraits<Member>;
			const typename Traits::Field& leftValue = static_cast<const typename Traits::Style*>(left)->*Member;
			const typename Traits::Field& rightValue = static_cast<const typename Traits::Style*>(right)->*Member;
			if constexpr (requires { leftValue == rightValue; })
				return leftValue == rightValue;
			else
				return false;
		}

		template<auto Member>
		void PropertyCopy(void* destination, const void* source)
		{
			using Traits = SdMemberPointerTraits<Member>;
			static_cast<typename Traits::Style*>(destination)->*Member = static_cast<const typename Traits::Style*>(source)->*Member;
		}

		template<auto Member>
		SdStyleValue PropertyReadValue(const void* style)
		{
			using Traits = SdMemberPointerTraits<Member>;
			return MakePropertyValue(static_cast<const typename Traits::Style*>(style)->*Member);
		}

		template<auto Member>
		void PropertyWriteValue(void* style, const SdStyleValue& value)
		{
			using Traits = SdMemberPointerTraits<Member>;
			TryWritePropertyValue(static_cast<typename Traits::Style*>(style)->*Member, value);
		}
	}

	class SdPropertyRegistry final
	{
	private:
		struct PropertyKey final
		{
			SdPropertyId propertyId = 0;
			std::type_index styleType = std::type_index(typeid(void));

			friend bool operator==(const PropertyKey& left, const PropertyKey& right) noexcept
			{
				return left.propertyId == right.propertyId && left.styleType == right.styleType;
			}
		};

		struct PropertyKeyHash final
		{
			SdSize operator()(const PropertyKey& key) const noexcept
			{
				SdSize hash = key.styleType.hash_code();
				hash ^= static_cast<SdSize>(key.propertyId + 0x9e3779b97f4a7c15ull + (hash << 6) + (hash >> 2));
				return hash;
			}
		};

		std::vector<SdPropertyDescriptor> properties = {};
		std::unordered_map<PropertyKey, SdSize, PropertyKeyHash> propertyIndexByKey = {};

		static bool CanUseBoxStyleProperty(std::type_index propertyStyleType, std::type_index requestedStyleType) noexcept
		{
			return propertyStyleType == std::type_index(typeid(SdBoxStyle))
				&& (requestedStyleType == std::type_index(typeid(SdWidgetRootStyle))
					|| requestedStyleType == std::type_index(typeid(SdWidgetPartStyle)));
		}

	public:
		template<auto Member>
		SdPropertyDescriptor& Register(
			SdStyleFieldImpact impact,
			SdStyleInterpolation interpolation,
			bool expensiveTransition = false)
		{
			using Traits = Detail::SdMemberPointerTraits<Member>;
			using TStyle = typename Traits::Style;
			using TField = typename Traits::Field;
			SdPropertyDescriptor descriptor = {};
			descriptor.propertyId = Detail::SdStylePropertyId(Member);
			descriptor.styleType = std::type_index(typeid(TStyle));
			descriptor.impact = impact;
			descriptor.interpolation = interpolation;
			descriptor.expensiveTransition = expensiveTransition;
			descriptor.equals = &Detail::PropertyEquals<Member>;
			descriptor.copy = &Detail::PropertyCopy<Member>;
			if constexpr (Detail::SdPropertyValueField<TField>)
			{
				descriptor.readValue = &Detail::PropertyReadValue<Member>;
				descriptor.writeValue = &Detail::PropertyWriteValue<Member>;
			}

			for (SdPropertyDescriptor& property : properties)
			{
				if (property.propertyId == descriptor.propertyId && property.styleType == descriptor.styleType)
				{
					property = descriptor;
					propertyIndexByKey[{ descriptor.propertyId, descriptor.styleType }] = static_cast<SdSize>(&property - properties.data());
					return property;
				}
			}

			properties.push_back(descriptor);
			propertyIndexByKey[{ descriptor.propertyId, descriptor.styleType }] = properties.size() - 1;
			return properties.back();
		}

		const SdPropertyDescriptor* Find(SdPropertyId propertyId, std::type_index styleType) const noexcept
		{
			const auto exactIt = propertyIndexByKey.find({ propertyId, styleType });
			if (exactIt != propertyIndexByKey.end() && exactIt->second < properties.size())
				return &properties[exactIt->second];

			if (styleType == std::type_index(typeid(SdWidgetRootStyle))
				|| styleType == std::type_index(typeid(SdWidgetPartStyle)))
			{
				const auto boxIt = propertyIndexByKey.find({ propertyId, std::type_index(typeid(SdBoxStyle)) });
				if (boxIt != propertyIndexByKey.end() && boxIt->second < properties.size())
					return &properties[boxIt->second];
			}

			for (const SdPropertyDescriptor& property : properties)
			{
				if (property.propertyId == propertyId && CanUseBoxStyleProperty(property.styleType, styleType))
					return &property;
			}
			return nullptr;
		}

		const std::vector<SdPropertyDescriptor>& GetProperties() const noexcept
		{
			return properties;
		}
	};
}
