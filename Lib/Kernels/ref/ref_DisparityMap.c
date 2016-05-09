//@file ref_Disparity.c
//@brief Contains implementation of functions aimed to getting disparity map
//@author Max Kimlyk
//@date 17 April 2016

#include "../ref.h"
#include <memory.h>

// TODO: remove this
#ifdef _DEBUG
#include <stdio.h>
#endif

// Function prototypes
uint8_t GetPixel(const vx_image image, uint32_t x, uint32_t y);
void    SetPixel(vx_image image, uint32_t x, uint32_t y, uint8_t value);

bool CheckImageSizes(const vx_image left_img, const vx_image right_img, const vx_image disp_img);

int  FindDisparity(
	const vx_image source_img, const vx_image match_img, 
	const vx_coordinates2d_t *pixel, const uint32_t block_halfsize, 
	const uint32_t max_disparity, const bool search_right);

uint32_t SummaryAbsoluteDifference(
	const vx_image source_img, const vx_image match_img, 
	const vx_rectangle_t *template_block, vx_coordinates2d_t *match_block);

int  SubPixelEstimation(int disparity, int prev_cost, int current_cost, int next_cost);
///////////////////////////////////////////////////////////////////////////////

vx_status ref_DisparityMap(
	const vx_image left_img, const vx_image right_img, vx_image disp_img,
	const uint32_t block_size, const uint32_t max_disparity, const bool search_right)
{
	if (!CheckImageSizes(left_img, right_img, disp_img))
	{
		return VX_ERROR_INVALID_PARAMETERS;
	}

	const uint32_t width = left_img->width;
	const uint32_t height = left_img->height;
	const uint32_t block_halfsize = block_size / 2;

	const vx_image template_img = search_right ? right_img : left_img;
	const vx_image match_img = search_right ? left_img : right_img;

	vx_coordinates2d_t pixel;

	for (pixel.y = block_halfsize; pixel.y < height - block_halfsize; pixel.y++)
	{
		for (pixel.x = block_halfsize; pixel.x < width - block_halfsize; pixel.x++)
		{
			int disparity = FindDisparity(template_img, match_img, &pixel, block_halfsize, max_disparity, search_right);
#ifdef _DEBUG
			if (disparity < 0 || disparity > UINT8_MAX)
			{
				printf("Warning. Disparity out of UINT8 range! (x=%d, y=%d)\n", pixel.x, pixel.y);
			}
#endif	
			SetPixel(disp_img, pixel.x, pixel.y, (uint8_t)(disparity));
		}

#ifdef _DEBUG
		if ((pixel.y - block_halfsize) % 10 == 0)
		{
			uint32_t rows = height - 2 * block_halfsize;
			printf("Row %d / %d\n", (pixel.y - block_halfsize), rows);
		}
#endif
	}

	return VX_SUCCESS;
}

uint8_t GetPixel(const vx_image image, uint32_t x, uint32_t y)
{
	uint8_t *pixels = (uint8_t*)(image->data);
	return pixels[y * image->width + x];
}

void SetPixel(vx_image image, uint32_t x, uint32_t y, uint8_t value)
{
	uint8_t *pixels = (uint8_t*)(image->data);
	pixels[y * image->width + x] = value;
}

bool CheckImageSizes(const vx_image left_img, const vx_image right_img, const vx_image disp_img)
{
	return (left_img->width == right_img->width && left_img->width == disp_img->width &&
		left_img->height == right_img->height && left_img->height == disp_img->height);
}

int FindDisparity(
	const vx_image source_img, const vx_image match_img,
	const vx_coordinates2d_t *pixel, const uint32_t block_halfsize, 
	const uint32_t max_disparity, const bool search_right)
{
	vx_rectangle_t template_block;
	template_block.start_x = pixel->x - block_halfsize;
	template_block.start_y = pixel->y - block_halfsize;
	template_block.end_x = pixel->x + block_halfsize;
	template_block.end_y = pixel->y + block_halfsize;

	const uint32_t NOT_DEFINED = UINT32_MAX;
	uint32_t min_diff = NOT_DEFINED;
	uint32_t prev_diff = NOT_DEFINED;
	uint32_t next_diff = NOT_DEFINED;
	uint32_t best_match_x = pixel->x;

	vx_coordinates2d_t match_block;
	match_block.y = template_block.start_y;

	int search_dir_sign = search_right ? +1 : -1;

	for (int disp = 0; disp < (int)(max_disparity); disp++)
	{
		uint32_t match_x = pixel->x + disp * search_dir_sign;
		if((int)(match_x - block_halfsize) < 0 || match_x + block_halfsize >= source_img->width)
			break;

		match_block.x = match_x - block_halfsize;

		uint32_t diff = SummaryAbsoluteDifference(source_img, match_img, &template_block, &match_block);
		if (diff < min_diff)
		{
			best_match_x = match_x;
			prev_diff = min_diff;
			min_diff  = diff;
			next_diff = NOT_DEFINED;
		}
		if (best_match_x - match_x == 1)
		{
			next_diff = diff;
		}
	}

	int disparity = (best_match_x - pixel->x) * search_dir_sign;

	if (prev_diff == NOT_DEFINED || next_diff == NOT_DEFINED)
	{
		return disparity;
	}
	else
	{
		return SubPixelEstimation(disparity, prev_diff, min_diff, next_diff);
	}
}

uint32_t SummaryAbsoluteDifference(
	const vx_image source_img, const vx_image match_img,
	const vx_rectangle_t *template_block, vx_coordinates2d_t *match_block)
{
	uint32_t width = template_block->end_x - template_block->start_x + 1;
	uint32_t height = template_block->end_y - template_block->start_y + 1;

	uint32_t sum = 0;

	for (uint32_t y = 0; y < height; y++)
	{
		for (uint32_t x = 0; x < width; x++)
		{
			uint8_t source_pixel = GetPixel(source_img, template_block->start_x + x, template_block->start_y + y);
			uint8_t match_pixel = GetPixel(match_img, match_block->x + x, match_block->y + y);
			sum += abs((int)(source_pixel) - (int)(match_pixel));
		}
	}

	return sum;
}

int SubPixelEstimation(int disparity, int prev_cost, int current_cost, int next_cost)
{
	float denum = (float)(prev_cost - 2 * current_cost + next_cost);
	if (denum != 0)
		return (int)(disparity - 0.5f * (float)(next_cost - prev_cost) / denum);
	else
		return disparity;
}