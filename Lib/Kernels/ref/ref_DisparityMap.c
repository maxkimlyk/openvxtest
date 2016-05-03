//@file ref_Disparity.c
//@brief Contains implementation of functions aimed to getting disparity map
//@author Max Kimlyk
//@date 17 April 2016

#include "../ref.h"
#include <memory.h>

// TODO: remove this
#include <stdio.h>

uint8_t*  At(vx_image image, uint32_t x, uint32_t y);
bool      CheckImageSizes(const vx_image left, const vx_image right, const vx_image disparity);
uint32_t  SummaryAbsoluteDifference(vx_image source_image, vx_image matching_image, vx_rectangle_t source_block, vx_rectangle_t matching_block);
uint32_t  CalcDisparity(vx_image left_image, vx_image right_image, vx_rectangle_t left_block, uint32_t max_distance);
void      FillBlock(vx_image image, vx_rectangle_t rect, uint8_t value);
uint32_t  SubPixelEstimation(vx_image left, vx_image right, vx_rectangle_t source_block, vx_rectangle_t min_block);

vx_status ref_DisparityMap(
	const vx_image left_image, const vx_image right_image, vx_image disparity_image,
	uint32_t block_size, uint32_t distance_threshold)
{
	if (!CheckImageSizes(left_image, right_image, disparity_image))
	{
		return VX_ERROR_INVALID_PARAMETERS;
	}
	
	const uint32_t width = left_image->width;
	const uint32_t height = left_image->height;

	uint32_t block_halfsize = block_size / 2;

	// TODO: diparity for each pixel

	for (uint32_t y = block_halfsize; y < height - block_halfsize; y++)
	{
		for (uint32_t x = distance_threshold; x < width - block_halfsize; x++)
		{
			uint32_t       disparity;
			vx_rectangle_t left_block;

			left_block.start_x = x - block_halfsize;
			left_block.start_y = y - block_halfsize;
			left_block.end_x   = x + block_halfsize;
			left_block.end_y   = y + block_halfsize;

			disparity = CalcDisparity(left_image, right_image, left_block, distance_threshold);

			// TODO: переместить подгонку цвета в демо
			const uint8_t max_color = 255;
			const uint8_t min_color = 10;
			uint8_t color = (uint8_t)(disparity / (float)distance_threshold * max_color);

			if (color < min_color)
			{
				color = min_color;
			}

			*At(disparity_image, x, y) = color;
		}

		// TODO: remove this
		if ((y - block_halfsize) % 10 == 0)
		{
			uint32_t working_size = height - 2 * block_halfsize;
			printf("row #%d (%2.f%%)\n", y, 100.0f * (float)(y - block_halfsize) / working_size);
		}
	}

	return VX_SUCCESS;
}

uint8_t* At(vx_image image, uint32_t x, uint32_t y)
{
	uint8_t* data = image->data;
	return &data[y * image->width + x];
}

bool CheckImageSizes(const vx_image left, const vx_image right, const vx_image disparity)
{
	if (left->width != right->width || left->width != disparity->width ||
		left->height != right->height || left->height != disparity->height)
		return false;
	return true;
}

uint32_t SummaryAbsoluteDifference(vx_image source_image, vx_image matching_image, 
	vx_rectangle_t source_block, vx_rectangle_t matching_block)
{
	const uint32_t width = source_block.end_x - source_block.start_x + 1;
	const uint32_t height = source_block.end_y - source_block.start_y + 1;

	uint32_t sum = 0;

	for (uint32_t y = 0; y < height; y++)
		for (uint32_t x = 0; x < width; x++)
		{
			uint8_t source_pixel = *At(source_image, source_block.start_x + x, source_block.end_y + y);
			uint8_t matching_pixel = *At(matching_image, matching_block.start_x + x, matching_block.end_y + y);

			sum += (uint32_t) abs(source_pixel - matching_pixel);
		}

	return sum;
}

uint32_t CalcDisparity(vx_image left_image, vx_image right_image, vx_rectangle_t left_block, uint32_t max_distance)
{
	uint32_t block_size = left_block.end_x - left_block.start_x;

	uint32_t min_difference = UINT_MAX;
	uint32_t min_difference_start_x = 0;

	for (uint32_t offset = 0; offset <= max_distance; offset++)
	{
		int x = left_block.start_x - offset;

		if (x < 0)
			break;

		vx_rectangle_t right_block;
		right_block.start_x = (uint32_t)x;
		right_block.start_y = left_block.start_y;
		right_block.end_x   = x + block_size;
		right_block.end_y   = left_block.end_y;

		uint32_t difference = SummaryAbsoluteDifference(left_image, right_image, left_block, right_block);
		if (difference < min_difference)
		{
			min_difference = difference;
			min_difference_start_x = x;
		}
	}

	vx_rectangle_t min_block;
	min_block.start_x = min_difference_start_x;
	min_block.start_y = left_block.start_y;
	min_block.end_x   = min_difference_start_x + block_size - 1;
	min_block.end_y   = left_block.end_y;

	//return (uint32_t) abs(left_block.start_x - min_difference_start_x);
	return SubPixelEstimation(left_image, right_image, left_block, min_block);
}

// TODO: it works still bad. solve the problem
uint32_t SubPixelEstimation(vx_image left_image, vx_image right_image, vx_rectangle_t template_block, vx_rectangle_t matched_block)
{
	uint32_t disparity = abs(template_block.start_x - matched_block.start_x);

	// if no neighbour blocks
	if (matched_block.start_x <= 0 || matched_block.end_x >= right_image->width - 1)
	{
		return disparity;
	}

	// TODO: refactor this

	matched_block.start_x --;
	matched_block.end_x --;

	int c1 = SummaryAbsoluteDifference(left_image, right_image, template_block, matched_block);
	
	matched_block.start_x ++;
	matched_block.end_x ++;

	int c2 = SummaryAbsoluteDifference(left_image, right_image, template_block, matched_block);

	matched_block.start_x ++;
	matched_block.end_x ++;

	int c3 = SummaryAbsoluteDifference(left_image, right_image, template_block, matched_block);

	// TODO: check the formula
	int denum = (c3 - 2*c2 + c1);

	if (denum == 0)
	{
		return disparity;
	}

	int num = c3 - c1;
	float correction = - (float)(num) / (float)(denum) * 0.5f;
	int   result = (int)(disparity + correction);

	return (uint32_t) (result);
}

void FillBlock(vx_image image, vx_rectangle_t rect, uint8_t value)
{
	for (uint32_t y = rect.start_y; y <= rect.end_y; y++)
		for (uint32_t x = rect.start_x; x <= rect.end_x; x++)
		{
			uint8_t *pixel = At(image, x, y);
			*pixel = value;
		}
}