#pragma once

#include <cstdint>
#include "mv.hpp"

constexpr const char FILTER_NAME[] = "ME_your_surname";
constexpr const char FILTER_AUTHOR[] = "PUT YOUR NAME HERE";

class MotionEstimator {
public:
	/// Constructor
	MotionEstimator(int width, int height, uint8_t quality, bool use_half_pixel);

	/// Destructor
	~MotionEstimator();

	/// Copy constructor (deleted)
	MotionEstimator(const MotionEstimator&) = delete;

	/// Move constructor
	MotionEstimator(MotionEstimator&&) = default;

	/// Copy assignment (deleted)
	MotionEstimator& operator=(const MotionEstimator&) = delete;

	/// Move assignment
	MotionEstimator& operator=(MotionEstimator&&) = default;

	/**
	 * Estimate motion between two frames
	 *
	 * @param[in] cur_Y array of pixels of the current frame
	 * @param[in] prev_Y array of pixels of the previous frame
	 * @param[in] prev_Y_up array of pixels of the previous frame shifted half a pixel up,
	 *   only valid if use_half_pixel is true
	 * @param[in] prev_Y_left array of pixels of the previous frame shifted half a pixel left,
	 *   only valid if use_half_pixel is true
	 * @param[in] prev_Y_upleft array of pixels of the previous frame shifted half a pixel up left,
	 *   only valid if use_half_pixel is true
	 * @param[out] mvectors output array of motion vectors
	 */
	void Estimate(const uint8_t* cur_Y,
	              const uint8_t* prev_Y,
	              const uint8_t* prev_Y_up,
	              const uint8_t* prev_Y_left,
	              const uint8_t* prev_Y_upleft,
	              MV* mvectors);

	/**
	 * Size of the borders added to frames by the template, in pixels.
	 * This is the most pixels your motion vectors can extend past the image border.
	 */
	static constexpr int BORDER = 16;

	/// Size of a block covered by a motion vector. Do not change.
	static constexpr int BLOCK_SIZE = 16;

private:
	/// Frame width (not including borders)
	const int width;

	/// Frame height (not including borders)
	const int height;

	/// Quality
	const uint8_t quality;

	/// Whether to use half-pixel precision
	const bool use_half_pixel;

	/// Extended frame width (including borders)
	const int width_ext;

	/// Number of blocks per X-axis
	const int num_blocks_hor;

	/// Number of blocks per Y-axis
	const int num_blocks_vert;

	/// Position of the first pixel of the frame in the extended frame
	const int first_row_offset;
};
