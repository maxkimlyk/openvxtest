//@file ref_Disparity.c
//@brief Contains implementation of functions aimed to getting disparity map
//@author Max Kimlyk
//@date 17 April 2016

#include "../ref.h"
#include <memory.h>

uint8_t*  At(vx_image image, uint32_t x, uint32_t y);
bool      CheckImageSizes(const vx_image left, const vx_image right, const vx_image disparity);
uint32_t  SummaryAbsoluteDifference(vx_image source_image, vx_image matching_image, vx_rectangle_t source_block, vx_rectangle_t matching_block);
uint32_t  CalcDisparity(vx_image left_image, vx_image right_image, vx_rectangle_t left_block, uint32_t max_distance);
void      FillBlock(vx_image image, vx_rectangle_t rect, uint8_t value);

vx_status ref_DisparityMap(const vx_image left_image, const vx_image right_image, vx_image disparity_image,
	uint32_t block_size, uint32_t distance_threshold)
{
	if (!CheckImageSizes(left_image, right_image, disparity_image))
	{
		return VX_ERROR_INVALID_PARAMETERS;
	}
	
	const uint32_t width = left_image->width;
	const uint32_t height = left_image->height;

	const uint32_t max_disparity = 255;

	const uint8_t* left_data = left_image->data;
	const uint8_t* right_data = right_image->data;

	// TODO: размер блока не является делителем размеров изображения

	for (uint32_t y = 0; y < height - block_size; y += block_size)
	{
		for (uint32_t x = 0; x < width - block_size; x += block_size)
		{
			uint32_t       disparity;
			vx_rectangle_t left_block = { x, y, x + block_size, y + block_size };

			disparity = CalcDisparity(left_image, right_image, left_block, distance_threshold);

			if (disparity > max_disparity)
			{
				disparity = max_disparity;
			}

			uint8_t color = disparity / (float)distance_threshold * max_disparity;
			FillBlock(disparity_image, left_block, color);
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
	uint32_t min_difference_start_x;

	for (uint32_t offset = 0; offset <= max_distance; offset++)
	{
		int x = left_block.start_x - offset;

		if (x < 0)
			break;

		vx_rectangle_t right_block = { (uint32_t)x, left_block.start_y, x + block_size, left_block.end_y };
		uint32_t       difference = SummaryAbsoluteDifference(left_image, right_image, left_block, right_block);
		if (difference < min_difference)
		{
			min_difference = difference;
			min_difference_start_x = x;
		}
	}

	return (uint32_t) abs(left_block.start_x - min_difference_start_x);
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