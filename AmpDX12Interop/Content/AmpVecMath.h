//--------------------------------------------------------------------------------------
// Copyright (c) XU, Tianchen. All rights reserved.
//--------------------------------------------------------------------------------------

#pragma once

float dot(const Concurrency::graphics::unorm_2& v1, const Concurrency::graphics::unorm_2& v2) __GPU
{
	return v1.x * v2.x + v1.y * v2.y;
}

float dot(const Concurrency::graphics::unorm_3& v1, const Concurrency::graphics::unorm_3& v2) __GPU
{
	return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

float dot(const Concurrency::graphics::unorm_4& v1, const Concurrency::graphics::unorm_4& v2) __GPU
{
	return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z + v1.w * v2.w;
}

float dot(const Concurrency::graphics::float_2& v1, const Concurrency::graphics::float_2& v2) __GPU
{
	return v1.x * v2.x + v1.y * v2.y;
}

float dot(const Concurrency::graphics::float_3& v1, const Concurrency::graphics::float_3& v2) __GPU
{
	return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

float dot(const Concurrency::graphics::float_4& v1, const Concurrency::graphics::float_4& v2) __GPU
{
	return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z + v1.w * v2.w;
}

template<typename T>
T normalize(const T& v) __GPU
{
	return v / Concurrency::fast_math::sqrtf(dot(v, v));
}
