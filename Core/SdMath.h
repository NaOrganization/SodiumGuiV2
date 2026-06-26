#pragma once

#include "Core/SdTypes.h"

#include <algorithm>
#include <cmath>

namespace Sodium
{
	inline constexpr float SdPIf = 3.14159265358979323846f;
	inline constexpr float SdPi = SdPIf;
	inline constexpr float SdTau = SdPIf * 2.0f;
	inline constexpr float SdHalfPi = SdPIf * 0.5f;
	inline constexpr float SdEpsilon = 0.000001f;

	template<typename T>
	constexpr T SdMin(T left, T right) noexcept
	{
		return left < right ? left : right;
	}

	template<typename T>
	constexpr T SdMax(T left, T right) noexcept
	{
		return left > right ? left : right;
	}

	template<typename T>
	constexpr T SdClamp(T value, T minimum, T maximum) noexcept
	{
		return value < minimum ? minimum : (value > maximum ? maximum : value);
	}

	constexpr float SdAbs(float value) noexcept
	{
		return value < 0.0f ? -value : value;
	}

	constexpr float SdSaturate(float value) noexcept
	{
		return SdClamp(value, 0.0f, 1.0f);
	}

	constexpr bool SdNearlyEqual(float left, float right, float epsilon = SdEpsilon) noexcept
	{
		return SdAbs(left - right) <= epsilon;
	}

	constexpr float SdLerp(float start, float end, float amount) noexcept
	{
		return start + (end - start) * amount;
	}

	constexpr float SdRadians(float degrees) noexcept
	{
		return degrees * (SdPIf / 180.0f);
	}

	constexpr float SdDegrees(float radians) noexcept
	{
		return radians * (180.0f / SdPIf);
	}

	struct SdVec2 final
	{
		float x = 0.0f;
		float y = 0.0f;

		constexpr SdVec2() = default;
		constexpr explicit SdVec2(float value) : x(value), y(value) {}
		constexpr SdVec2(float valueX, float valueY) : x(valueX), y(valueY) {}

		static constexpr SdVec2 Zero() noexcept { return {}; }
		static constexpr SdVec2 One() noexcept { return { 1.0f, 1.0f }; }
		static constexpr SdVec2 UnitX() noexcept { return { 1.0f, 0.0f }; }
		static constexpr SdVec2 UnitY() noexcept { return { 0.0f, 1.0f }; }

		constexpr SdVec2 operator+() const noexcept { return *this; }
		constexpr SdVec2 operator-() const noexcept { return { -x, -y }; }
		constexpr SdVec2 operator+(const SdVec2& other) const noexcept { return { x + other.x, y + other.y }; }
		constexpr SdVec2 operator-(const SdVec2& other) const noexcept { return { x - other.x, y - other.y }; }
		constexpr SdVec2 operator*(const SdVec2& other) const noexcept { return { x * other.x, y * other.y }; }
		constexpr SdVec2 operator/(const SdVec2& other) const noexcept { return { x / other.x, y / other.y }; }
		constexpr SdVec2 operator*(float scalar) const noexcept { return { x * scalar, y * scalar }; }
		constexpr SdVec2 operator/(float scalar) const noexcept { return { x / scalar, y / scalar }; }
		constexpr SdVec2& operator+=(const SdVec2& other) noexcept { x += other.x; y += other.y; return *this; }
		constexpr SdVec2& operator-=(const SdVec2& other) noexcept { x -= other.x; y -= other.y; return *this; }
		constexpr SdVec2& operator*=(float scalar) noexcept { x *= scalar; y *= scalar; return *this; }
		constexpr SdVec2& operator/=(float scalar) noexcept { x /= scalar; y /= scalar; return *this; }

		constexpr float Dot(const SdVec2& other) const noexcept { return x * other.x + y * other.y; }
		constexpr float Cross(const SdVec2& other) const noexcept { return x * other.y - y * other.x; }
		constexpr float LengthSquared() const noexcept { return Dot(*this); }
		float Length() const noexcept { return std::sqrt(LengthSquared()); }
		SdVec2 Normalized(float epsilon = SdEpsilon) const noexcept
		{
			const float length = Length();
			return length > epsilon ? *this / length : SdVec2{};
		}

		constexpr bool IsZero(float epsilon = SdEpsilon) const noexcept
		{
			return SdAbs(x) <= epsilon && SdAbs(y) <= epsilon;
		}

		constexpr SdVec2 Clamped(const SdVec2& minimum, const SdVec2& maximum) const noexcept
		{
			return { SdClamp(x, minimum.x, maximum.x), SdClamp(y, minimum.y, maximum.y) };
		}

		friend constexpr bool operator==(const SdVec2&, const SdVec2&) = default;
		friend constexpr SdVec2 operator*(float scalar, const SdVec2& value) noexcept { return value * scalar; }
	};

	struct SdVec3 final
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;

		constexpr SdVec3() = default;
		constexpr explicit SdVec3(float value) : x(value), y(value), z(value) {}
		constexpr SdVec3(float valueX, float valueY, float valueZ) : x(valueX), y(valueY), z(valueZ) {}
		constexpr SdVec3(const SdVec2& xy, float valueZ) : x(xy.x), y(xy.y), z(valueZ) {}

		static constexpr SdVec3 Zero() noexcept { return {}; }
		static constexpr SdVec3 One() noexcept { return { 1.0f, 1.0f, 1.0f }; }
		static constexpr SdVec3 UnitX() noexcept { return { 1.0f, 0.0f, 0.0f }; }
		static constexpr SdVec3 UnitY() noexcept { return { 0.0f, 1.0f, 0.0f }; }
		static constexpr SdVec3 UnitZ() noexcept { return { 0.0f, 0.0f, 1.0f }; }

		constexpr SdVec3 operator+() const noexcept { return *this; }
		constexpr SdVec3 operator-() const noexcept { return { -x, -y, -z }; }
		constexpr SdVec3 operator+(const SdVec3& other) const noexcept { return { x + other.x, y + other.y, z + other.z }; }
		constexpr SdVec3 operator-(const SdVec3& other) const noexcept { return { x - other.x, y - other.y, z - other.z }; }
		constexpr SdVec3 operator*(const SdVec3& other) const noexcept { return { x * other.x, y * other.y, z * other.z }; }
		constexpr SdVec3 operator/(const SdVec3& other) const noexcept { return { x / other.x, y / other.y, z / other.z }; }
		constexpr SdVec3 operator*(float scalar) const noexcept { return { x * scalar, y * scalar, z * scalar }; }
		constexpr SdVec3 operator/(float scalar) const noexcept { return { x / scalar, y / scalar, z / scalar }; }
		constexpr SdVec3& operator+=(const SdVec3& other) noexcept { x += other.x; y += other.y; z += other.z; return *this; }
		constexpr SdVec3& operator-=(const SdVec3& other) noexcept { x -= other.x; y -= other.y; z -= other.z; return *this; }
		constexpr SdVec3& operator*=(float scalar) noexcept { x *= scalar; y *= scalar; z *= scalar; return *this; }
		constexpr SdVec3& operator/=(float scalar) noexcept { x /= scalar; y /= scalar; z /= scalar; return *this; }

		constexpr float Dot(const SdVec3& other) const noexcept { return x * other.x + y * other.y + z * other.z; }
		constexpr SdVec3 Cross(const SdVec3& other) const noexcept
		{
			return {
				y * other.z - z * other.y,
				z * other.x - x * other.z,
				x * other.y - y * other.x
			};
		}
		constexpr float LengthSquared() const noexcept { return Dot(*this); }
		float Length() const noexcept { return std::sqrt(LengthSquared()); }
		SdVec3 Normalized(float epsilon = SdEpsilon) const noexcept
		{
			const float length = Length();
			return length > epsilon ? *this / length : SdVec3{};
		}

		friend constexpr bool operator==(const SdVec3&, const SdVec3&) = default;
		friend constexpr SdVec3 operator*(float scalar, const SdVec3& value) noexcept { return value * scalar; }
	};

	struct SdVec4 final
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
		float w = 0.0f;

		constexpr SdVec4() = default;
		constexpr explicit SdVec4(float value) : x(value), y(value), z(value), w(value) {}
		constexpr SdVec4(float valueX, float valueY, float valueZ, float valueW)
			: x(valueX), y(valueY), z(valueZ), w(valueW) {}
		constexpr SdVec4(const SdVec2& xy, const SdVec2& zw)
			: x(xy.x), y(xy.y), z(zw.x), w(zw.y) {}
		constexpr SdVec4(const SdVec3& xyz, float valueW)
			: x(xyz.x), y(xyz.y), z(xyz.z), w(valueW) {}

		static constexpr SdVec4 Zero() noexcept { return {}; }
		static constexpr SdVec4 One() noexcept { return { 1.0f, 1.0f, 1.0f, 1.0f }; }

		constexpr SdVec4 operator+(const SdVec4& other) const noexcept { return { x + other.x, y + other.y, z + other.z, w + other.w }; }
		constexpr SdVec4 operator-(const SdVec4& other) const noexcept { return { x - other.x, y - other.y, z - other.z, w - other.w }; }
		constexpr SdVec4 operator*(float scalar) const noexcept { return { x * scalar, y * scalar, z * scalar, w * scalar }; }
		constexpr SdVec4 operator/(float scalar) const noexcept { return { x / scalar, y / scalar, z / scalar, w / scalar }; }
		constexpr float Dot(const SdVec4& other) const noexcept { return x * other.x + y * other.y + z * other.z + w * other.w; }
		friend constexpr bool operator==(const SdVec4&, const SdVec4&) = default;
		friend constexpr SdVec4 operator*(float scalar, const SdVec4& value) noexcept { return value * scalar; }
	};

	struct SdSize2 final
	{
		float width = 0.0f;
		float height = 0.0f;

		constexpr SdSize2() = default;
		constexpr SdSize2(float valueWidth, float valueHeight) : width(valueWidth), height(valueHeight) {}
		constexpr explicit SdSize2(const SdVec2& value) : width(value.x), height(value.y) {}

		constexpr SdVec2 ToVec2() const noexcept { return { width, height }; }
		constexpr bool IsEmpty() const noexcept { return width <= 0.0f || height <= 0.0f; }

		friend constexpr bool operator==(const SdSize2&, const SdSize2&) = default;
	};

	// Corner order: top-left, top-right, bottom-right, bottom-left.
	struct SdCornerRadii final
	{
		float topLeft = 0.0f;
		float topRight = 0.0f;
		float bottomRight = 0.0f;
		float bottomLeft = 0.0f;

		constexpr SdCornerRadii() = default;
		constexpr explicit SdCornerRadii(float radius)
			: topLeft(radius), topRight(radius), bottomRight(radius), bottomLeft(radius) {}
		constexpr SdCornerRadii(float valueTopLeft, float valueTopRight, float valueBottomRight, float valueBottomLeft)
			: topLeft(valueTopLeft), topRight(valueTopRight), bottomRight(valueBottomRight), bottomLeft(valueBottomLeft) {}

		static constexpr SdCornerRadii All(float radius) noexcept { return SdCornerRadii(radius); }
		constexpr bool IsZero(float epsilon = SdEpsilon) const noexcept
		{
			return SdAbs(topLeft) <= epsilon
				&& SdAbs(topRight) <= epsilon
				&& SdAbs(bottomRight) <= epsilon
				&& SdAbs(bottomLeft) <= epsilon;
		}

		constexpr SdCornerRadii Clamped(float minimum, float maximum) const noexcept
		{
			return {
				SdClamp(topLeft, minimum, maximum),
				SdClamp(topRight, minimum, maximum),
				SdClamp(bottomRight, minimum, maximum),
				SdClamp(bottomLeft, minimum, maximum)
			};
		}

		friend constexpr bool operator==(const SdCornerRadii&, const SdCornerRadii&) = default;
	};

	struct SdMat3 final
	{
		float m[3][3] =
		{
			{ 1.0f, 0.0f, 0.0f },
			{ 0.0f, 1.0f, 0.0f },
			{ 0.0f, 0.0f, 1.0f }
		};

		constexpr SdMat3() = default;
		constexpr SdMat3(
			float m00, float m01, float m02,
			float m10, float m11, float m12,
			float m20, float m21, float m22)
			: m{
				{ m00, m01, m02 },
				{ m10, m11, m12 },
				{ m20, m21, m22 }
			} {}

		static constexpr SdMat3 Identity() noexcept
		{
			return {};
		}

		static constexpr SdMat3 Translation(const SdVec2& value) noexcept
		{
			return {
				1.0f, 0.0f, 0.0f,
				0.0f, 1.0f, 0.0f,
				value.x, value.y, 1.0f
			};
		}

		static constexpr SdMat3 Scale(const SdVec2& value) noexcept
		{
			return {
				value.x, 0.0f, 0.0f,
				0.0f, value.y, 0.0f,
				0.0f, 0.0f, 1.0f
			};
		}

		static SdMat3 Rotation(float radians) noexcept
		{
			const float cosine = std::cos(radians);
			const float sine = std::sin(radians);
			return {
				cosine, sine, 0.0f,
				-sine, cosine, 0.0f,
				0.0f, 0.0f, 1.0f
			};
		}

		constexpr SdMat3 operator*(const SdMat3& other) const noexcept
		{
			SdMat3 result = {
				0.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 0.0f,
				0.0f, 0.0f, 0.0f
			};
			for (SdSize row = 0; row < 3; ++row)
			{
				for (SdSize column = 0; column < 3; ++column)
				{
					for (SdSize index = 0; index < 3; ++index)
						result.m[row][column] += m[row][index] * other.m[index][column];
				}
			}
			return result;
		}

		constexpr SdVec2 TransformPoint(const SdVec2& point) const noexcept
		{
			return {
				point.x * m[0][0] + point.y * m[1][0] + m[2][0],
				point.x * m[0][1] + point.y * m[1][1] + m[2][1]
			};
		}

		constexpr SdVec2 TransformVector(const SdVec2& vector) const noexcept
		{
			return {
				vector.x * m[0][0] + vector.y * m[1][0],
				vector.x * m[0][1] + vector.y * m[1][1]
			};
		}
	};

	struct SdMat4 final
	{
		float m[4][4] =
		{
			{ 1.0f, 0.0f, 0.0f, 0.0f },
			{ 0.0f, 1.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 1.0f, 0.0f },
			{ 0.0f, 0.0f, 0.0f, 1.0f }
		};

		constexpr SdMat4() = default;
		constexpr SdMat4(
			float m00, float m01, float m02, float m03,
			float m10, float m11, float m12, float m13,
			float m20, float m21, float m22, float m23,
			float m30, float m31, float m32, float m33)
			: m{
				{ m00, m01, m02, m03 },
				{ m10, m11, m12, m13 },
				{ m20, m21, m22, m23 },
				{ m30, m31, m32, m33 }
			} {}

		static constexpr SdMat4 Identity() noexcept
		{
			return {};
		}

		static constexpr SdMat4 Orthographic(float left, float right, float bottom, float top, float nearZ, float farZ) noexcept
		{
			return {
				2.0f / (right - left), 0.0f, 0.0f, 0.0f,
				0.0f, 2.0f / (top - bottom), 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f / (farZ - nearZ), 0.0f,
				-(right + left) / (right - left), -(top + bottom) / (top - bottom), -nearZ / (farZ - nearZ), 1.0f
			};
		}
	};

	struct SdRect final
	{
		SdVec2 min = {};
		SdVec2 max = {};

		constexpr SdRect() = default;
		constexpr SdRect(const SdVec2& minimum, const SdVec2& maximum) : min(minimum), max(maximum) {}
		constexpr SdRect(float left, float top, float right, float bottom) : min(left, top), max(right, bottom) {}

		static constexpr SdRect FromPositionSize(const SdVec2& position, const SdVec2& size) noexcept
		{
			return { position, position + size };
		}

		static constexpr SdRect FromCenterSize(const SdVec2& center, const SdVec2& size) noexcept
		{
			const SdVec2 halfSize = size * 0.5f;
			return { center - halfSize, center + halfSize };
		}

		constexpr float Width() const noexcept { return max.x - min.x; }
		constexpr float Height() const noexcept { return max.y - min.y; }
		constexpr SdVec2 Position() const noexcept { return min; }
		constexpr SdVec2 Size() const noexcept { return { Width(), Height() }; }
		constexpr SdVec2 Center() const noexcept { return (min + max) * 0.5f; }
		constexpr float Area() const noexcept { return Width() * Height(); }
		constexpr bool IsEmpty() const noexcept { return Width() <= 0.0f || Height() <= 0.0f; }
		constexpr bool HasArea() const noexcept { return !IsEmpty(); }
		constexpr bool Contains(const SdVec2& point) const noexcept { return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y; }
		constexpr bool Contains(const SdRect& rect) const noexcept { return Contains(rect.min) && Contains(rect.max); }
		constexpr bool Intersects(const SdRect& other) const noexcept { return !(other.min.x > max.x || other.max.x < min.x || other.min.y > max.y || other.max.y < min.y); }

		constexpr SdRect Translated(const SdVec2& offset) const noexcept
		{
			return { min + offset, max + offset };
		}

		constexpr SdRect Inflated(float amount) const noexcept
		{
			return { min.x - amount, min.y - amount, max.x + amount, max.y + amount };
		}

		constexpr SdRect Inset(float amount) const noexcept
		{
			return Inflated(-amount);
		}

		constexpr SdVec2 ClampPoint(const SdVec2& point) const noexcept
		{
			return {
				SdClamp(point.x, min.x, max.x),
				SdClamp(point.y, min.y, max.y)
			};
		}

		constexpr SdRect Intersection(const SdRect& other) const noexcept
		{
			const SdRect result = {
				SdMax(min.x, other.min.x),
				SdMax(min.y, other.min.y),
				SdMin(max.x, other.max.x),
				SdMin(max.y, other.max.y)
			};
			return result.IsEmpty() ? SdRect{} : result;
		}

		constexpr SdRect Union(const SdRect& other) const noexcept
		{
			if (IsEmpty())
				return other;
			if (other.IsEmpty())
				return *this;
			return {
				SdMin(min.x, other.min.x),
				SdMin(min.y, other.min.y),
				SdMax(max.x, other.max.x),
				SdMax(max.y, other.max.y)
			};
		}

		friend constexpr bool operator==(const SdRect&, const SdRect&) = default;
	};

	struct SdLineSegment2 final
	{
		SdVec2 a = {};
		SdVec2 b = {};

		constexpr SdLineSegment2() = default;
		constexpr SdLineSegment2(const SdVec2& pointA, const SdVec2& pointB) : a(pointA), b(pointB) {}

		constexpr SdVec2 Direction() const noexcept { return b - a; }
		constexpr float LengthSquared() const noexcept { return Direction().LengthSquared(); }
		float Length() const noexcept { return Direction().Length(); }
		constexpr SdRect Bounds() const noexcept
		{
			return {
				SdMin(a.x, b.x),
				SdMin(a.y, b.y),
				SdMax(a.x, b.x),
				SdMax(a.y, b.y)
			};
		}
	};

	struct SdCircle final
	{
		SdVec2 center = {};
		float radius = 0.0f;

		constexpr SdCircle() = default;
		constexpr SdCircle(const SdVec2& valueCenter, float valueRadius) : center(valueCenter), radius(valueRadius) {}

		constexpr float Diameter() const noexcept { return radius * 2.0f; }
		constexpr bool Contains(const SdVec2& point) const noexcept { return (point - center).LengthSquared() <= radius * radius; }
		constexpr SdRect Bounds() const noexcept { return SdRect::FromCenterSize(center, { Diameter(), Diameter() }); }
	};

	constexpr float SdDot(const SdVec2& left, const SdVec2& right) noexcept { return left.Dot(right); }
	constexpr float SdCross(const SdVec2& left, const SdVec2& right) noexcept { return left.Cross(right); }
	constexpr float SdLengthSquared(const SdVec2& value) noexcept { return value.LengthSquared(); }
	inline float SdVecLength(const SdVec2& value) noexcept { return value.Length(); }
	inline SdVec2 SdNormalize(const SdVec2& value, float epsilon = SdEpsilon) noexcept { return value.Normalized(epsilon); }
	constexpr SdVec2 SdLerp(const SdVec2& start, const SdVec2& end, float amount) noexcept { return start + (end - start) * amount; }
	constexpr SdVec2 SdMin(const SdVec2& left, const SdVec2& right) noexcept { return { SdMin(left.x, right.x), SdMin(left.y, right.y) }; }
	constexpr SdVec2 SdMax(const SdVec2& left, const SdVec2& right) noexcept { return { SdMax(left.x, right.x), SdMax(left.y, right.y) }; }
	constexpr SdVec2 SdClamp(const SdVec2& value, const SdVec2& minimum, const SdVec2& maximum) noexcept { return value.Clamped(minimum, maximum); }

	constexpr float SdDot(const SdVec3& left, const SdVec3& right) noexcept { return left.Dot(right); }
	constexpr SdVec3 SdCross(const SdVec3& left, const SdVec3& right) noexcept { return left.Cross(right); }
	constexpr float SdLengthSquared(const SdVec3& value) noexcept { return value.LengthSquared(); }
	inline float SdVecLength(const SdVec3& value) noexcept { return value.Length(); }
	inline SdVec3 SdNormalize(const SdVec3& value, float epsilon = SdEpsilon) noexcept { return value.Normalized(epsilon); }
	constexpr SdVec3 SdLerp(const SdVec3& start, const SdVec3& end, float amount) noexcept { return start + (end - start) * amount; }

	constexpr SdRect SdMakeRect(const SdVec2& position, const SdVec2& size) noexcept { return SdRect::FromPositionSize(position, size); }
	constexpr SdRect SdOffsetRect(const SdRect& rect, const SdVec2& offset) noexcept { return rect.Translated(offset); }
	constexpr SdRect SdInflateRect(const SdRect& rect, float amount) noexcept { return rect.Inflated(amount); }
	constexpr SdRect SdInsetRect(const SdRect& rect, float amount) noexcept { return rect.Inset(amount); }
	constexpr SdRect SdIntersectRect(const SdRect& left, const SdRect& right) noexcept { return left.Intersection(right); }
	constexpr SdRect SdUnionRect(const SdRect& left, const SdRect& right) noexcept { return left.Union(right); }
}
