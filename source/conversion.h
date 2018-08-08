#pragma once

void ImageMath_MatrixMultiply8888 (const void *src,
	size_t src_bytes_per_row,
	void *dst,
	size_t dst_bytes_per_row,
	unsigned long width,
	unsigned long height,
	const int16_t matrix[4 * 4],
	int32_t divisor,          // Applied after the matrix op
	const int16_t	*pre_bias,	// An array of 4 int16_t or NULL, added before matrix op
	const int32_t *post_bias,	// An array of 4 int32_t or NULL, added after matrix op
	bool bUseOMP);

//void ConvertBGRAtoRGBA (int width, int height, const unsigned char* a, unsigned char* b, bool bUseOMP);
//void FlipVerticallyInPlace (unsigned char* buffer, int stride, int height, bool bUseOMP);

void ConvertBGRAtoRGBA_flippedVertically (int width, int height, const unsigned char* a, unsigned char* b, bool bUseOMP);
