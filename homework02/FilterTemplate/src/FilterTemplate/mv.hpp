#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

enum class ShiftDir
{
	NONE,
	UP,
	LEFT,
	UPLEFT
};

class MV
{
public:
	/// Constructor
	MV(int x = 0,
	   int y = 0,
	   ShiftDir shift_dir = ShiftDir::NONE,
	   long error = std::numeric_limits<long>::max())
		: x(x)
		, y(y)
		, shift_dir(shift_dir)
		, error(error)
		, subvectors(nullptr)
	{}

	/// Copy constructor
	MV(const MV& other)
		: x(other.x)
		, y(other.y)
		, shift_dir(other.shift_dir)
		, error(other.error)
	{
		if (other.subvectors)
			subvectors = std::make_unique<SubVectors>(*other.subvectors);
	}

	/// Move constructor
	MV(MV&& other)
		: MV()
	{
		swap(other);
	}

	/// Copy + move assignment operator
	MV& operator=(MV other)
	{
		swap(other);
		return *this;
	}

	/// Split into 4 subvectors
	inline void Split()
	{
		subvectors = std::make_unique<SubVectors>();
	}

	/// Unsplit
	inline void Unsplit()
	{
		subvectors.reset();
	}

	/// Check if the motion vector is split
	inline bool IsSplit() const
	{
		return (subvectors != nullptr);
	}

	/// Get a subvector
	inline MV& SubVector(int id)
	{
		assert(subvectors && id >= 0 && id < 4);
		return (*subvectors)[id];
	}

	int x;
	int y;
	ShiftDir shift_dir;
	long error;

private:
	using SubVectors = std::array<MV, 4>;
	std::unique_ptr<SubVectors> subvectors;

	void swap(MV& other)
	{
		std::swap(x, other.x);
		std::swap(y, other.y);
		std::swap(shift_dir, other.shift_dir);
		std::swap(error, other.error);
		std::swap(subvectors, other.subvectors);
	}
};
