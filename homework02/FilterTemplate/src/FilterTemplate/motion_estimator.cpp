#include <unordered_map>

#include "metric.hpp"
#include "motion_estimator.hpp"

MotionEstimator::MotionEstimator(int width, int height, uint8_t quality, bool use_half_pixel)
	: width(width)
	, height(height)
	, quality(quality)
	, use_half_pixel(use_half_pixel)
	, width_ext(width + 2 * BORDER)
	, num_blocks_hor((width + BLOCK_SIZE - 1) / BLOCK_SIZE)
	, num_blocks_vert((height + BLOCK_SIZE - 1) / BLOCK_SIZE)
	, first_row_offset(width_ext * BORDER + BORDER) {
	// PUT YOUR CODE HERE
}

MotionEstimator::~MotionEstimator() {
	// PUT YOUR CODE HERE
}

void MotionEstimator::Estimate(const uint8_t* cur_Y,
                               const uint8_t* prev_Y,
                               const uint8_t* prev_Y_up,
                               const uint8_t* prev_Y_left,
                               const uint8_t* prev_Y_upleft,
                               MV* mvectors) {
	std::unordered_map<ShiftDir, const uint8_t*> prev_map {
		{ ShiftDir::NONE, prev_Y }
	};

	if (use_half_pixel) {
		prev_map.emplace(ShiftDir::UP, prev_Y_up);
		prev_map.emplace(ShiftDir::LEFT, prev_Y_left);
		prev_map.emplace(ShiftDir::UPLEFT, prev_Y_upleft);
	}

	for (int i = 0; i < num_blocks_vert; ++i) {
		for (int j = 0; j < num_blocks_hor; ++j) {
			const auto block_id = i * num_blocks_hor + j;
			const auto hor_offset = j * BLOCK_SIZE;
			const auto vert_offset = first_row_offset + i * BLOCK_SIZE * width_ext;
			const auto cur = cur_Y + vert_offset + hor_offset;

			MV best_vector;
			best_vector.error = std::numeric_limits<long>::max();

			// PUT YOUR CODE HERE
			
			// Brute force
			for (const auto& prev_pair : prev_map) {
				const auto prev = prev_pair.second + vert_offset + hor_offset;

				for (int y = -BORDER; y <= BORDER; ++y) {
					for (int x = -BORDER; x <= BORDER; ++x) {
						const auto comp = prev + y * width_ext + x;
						const auto error = GetErrorSAD_16x16(cur, comp, width_ext);

						if (error < best_vector.error) {
							best_vector.x = x;
							best_vector.y = y;
							best_vector.shift_dir = prev_pair.first;
							best_vector.error = error;
						}
					}
				}
			}

			// Split into four subvectors if the error is too large
			if (best_vector.error > 1000) {
				best_vector.Split();

				for (int h = 0; h < 4; ++h) {
					auto& subvector = best_vector.SubVector(h);
					subvector.error = std::numeric_limits<long>::max();

					const auto hor_offset = j * BLOCK_SIZE + ((h & 1) ? BLOCK_SIZE / 2 : 0);
					const auto vert_offset = first_row_offset + (i * BLOCK_SIZE + ((h > 1) ? BLOCK_SIZE / 2 : 0)) * width_ext;
					const auto cur = cur_Y + vert_offset + hor_offset;

					for (const auto& prev_pair : prev_map) {
						const auto prev = prev_pair.second + vert_offset + hor_offset;

						for (int y = -BORDER; y <= BORDER; ++y) {
							for (int x = -BORDER; x <= BORDER; ++x) {
								const auto comp = prev + y * width_ext + x;
								const auto error = GetErrorSAD_8x8(cur, comp, width_ext);

								if (error < subvector.error) {
									subvector.x = x;
									subvector.y = y;
									subvector.shift_dir = prev_pair.first;
									subvector.error = error;
								}
							}
						}
					}
				}

				if (best_vector.SubVector(0).error
				    + best_vector.SubVector(1).error
				    + best_vector.SubVector(2).error
				    + best_vector.SubVector(3).error > best_vector.error * 0.7)
					best_vector.Unsplit();
			}

			mvectors[block_id] = best_vector;
		}
	}
}
