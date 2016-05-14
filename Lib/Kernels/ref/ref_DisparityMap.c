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
bool CheckImageSizes(const vx_image left_img, const vx_image right_img, const vx_image disp_img);

uint8_t GetPixel8(const vx_image image, uint32_t x, uint32_t y);
void    SetPixel8(vx_image image, uint32_t x, uint32_t y, uint8_t value);
int16_t GetPixel16(const vx_image image, uint32_t x, uint32_t y);
void    SetPixel16(vx_image image, uint32_t x, uint32_t y, int16_t value);
uint32_t GetPixel32(const vx_image image, uint32_t x, uint32_t y);
void     SetPixel32(vx_image image, uint32_t x, uint32_t y, uint32_t value);

vx_image CreateMatchingCostArrays(vx_image left_img, vx_image right_img, int16_t max_disparity);
void       FreeMatchingCostArrays(vx_image match_cost_images, int16_t max_disparity);

vx_image CreateBlockCostImages(vx_image pixel_cost_images, uint32_t block_halfsize, int16_t max_disparity);
vx_image CreateBlockCostImages_Slow(vx_image pixel_cost_images, uint32_t block_halfsize, int16_t max_disparity);
void       FreeBlockCostImage(vx_image block_cost_images, int16_t max_disparity);

uint32_t BlockCostEstimateFromLeft(vx_image pixel_cost_image, uint32_t x, uint32_t y, uint32_t left_cost, uint32_t block_halfsize);
uint32_t BlockCostEstimateFromTop(vx_image pixel_cost_image, uint32_t x, uint32_t y, uint32_t top_cost, uint32_t block_halfsize);

int16_t Disparity(vx_coordinates2d_t *pixel, vx_image block_cost_images, const int16_t max_disparity, const uint32_t block_halfsize);

int16_t FindDisparity(
	const vx_coordinates2d_t *pixel, const vx_image match_cost_images,
	const uint32_t block_halfsize, const int16_t max_disparity, const bool search_right);

uint32_t SummaryAbsoluteDifference(
	const vx_image source_img, const vx_image match_img,
	const vx_rectangle_t *template_block, vx_coordinates2d_t *match_block);

uint32_t SummOverBlock(const vx_image match_cost_image, const vx_rectangle_t *block);

int16_t SubPixelEstimation(int16_t disparity, int prev_cost, int current_cost, int next_cost);
///////////////////////////////////////////////////////////////////////////////

vx_status ref_DisparityMap(
	const vx_image left_img, const vx_image right_img, vx_image disp_img,
	const uint32_t block_size, const int16_t max_disparity, const bool search_right)
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

	vx_image match_cost_arrays = CreateMatchingCostArrays(template_img, match_img, max_disparity);
	vx_image block_cost_images = CreateBlockCostImages(match_cost_arrays, block_halfsize, max_disparity);

	FreeMatchingCostArrays(match_cost_arrays, max_disparity);

	vx_coordinates2d_t pixel;

	for (pixel.y = block_halfsize; pixel.y < height - block_halfsize; pixel.y++)
	{
		for (pixel.x = block_halfsize; pixel.x < width - block_halfsize; pixel.x++)
		{
			//int16_t disparity = FindDisparity(&pixel, match_cost_arrays, block_halfsize, max_disparity, search_right);
			int16_t disparity = Disparity(&pixel, block_cost_images, max_disparity, block_halfsize);
			SetPixel16(disp_img, pixel.x, pixel.y, disparity);
		}

#ifdef __DEBUG
		if ((pixel.y - block_halfsize) % 10 == 0)
		{
			uint32_t rows = height - 2 * block_halfsize;
			printf("Row %d / %d\n", (pixel.y - block_halfsize), rows);
		}
#endif
	}

	FreeBlockCostImage(block_cost_images, max_disparity);

	return VX_SUCCESS;
}

bool CheckImageSizes(const vx_image left_img, const vx_image right_img, const vx_image disp_img)
{
	return (left_img->width == right_img->width && left_img->width == disp_img->width &&
		left_img->height == right_img->height && left_img->height == disp_img->height);
}

uint8_t GetPixel8(const vx_image image, uint32_t x, uint32_t y)
{
	uint8_t *pixels = (uint8_t*)(image->data);
	return pixels[y * image->width + x];
}

void SetPixel8(vx_image image, uint32_t x, uint32_t y, uint8_t value)
{
	uint8_t *pixels = (uint8_t*)(image->data);
	pixels[y * image->width + x] = value;
}

int16_t GetPixel16(const vx_image image, uint32_t x, uint32_t y)
{
	int16_t *pixels = (int16_t*)(image->data);
	return pixels[y * image->width + x];
}

void SetPixel16(vx_image image, uint32_t x, uint32_t y, int16_t value)
{
	int16_t *pixels = (int16_t*)(image->data);
	pixels[y * image->width + x] = value;
}

uint32_t GetPixel32(const vx_image image, uint32_t x, uint32_t y)
{
	uint32_t *pixels = (uint32_t*)(image->data);
	return pixels[y * image->width + x];
}

void SetPixel32(vx_image image, uint32_t x, uint32_t y, uint32_t value)
{
	uint32_t *pixels = (uint32_t*)(image->data);
	pixels[y * image->width + x] = value;
}

vx_image CreateMatchingCostArrays(vx_image left_img, vx_image right_img, int16_t max_disparity)
{
	uint32_t width = left_img->width;
	uint32_t height = right_img->height;
	uint32_t num_pixels = width * height;
	uint32_t num_disparities = max_disparity + 1;

	vx_image match_cost_images = (vx_image)calloc(num_disparities, sizeof(struct _vx_image));

	for (uint32_t i = 0; i <= max_disparity; i++)
	{
		uint8_t *image_data = (uint8_t*)calloc(width*height, sizeof(uint8_t));
		match_cost_images[i].data = image_data;
		match_cost_images[i].width = width;
		match_cost_images[i].height = height;
		match_cost_images[i].image_type = VX_DF_IMAGE_U8;
		match_cost_images[i].color_space = VX_COLOR_SPACE_DEFAULT;
	}

	for (uint32_t y = 0; y < height; y++)
	{
		for (uint32_t x = 0; x < width; x++)
		{
			uint8_t left_pixel = GetPixel8(left_img, x, y);

			for (uint32_t offset = 0; offset <= max_disparity; offset++)
			{
				if ((int)x - (int)offset < 0)
					break;

				uint8_t right_pixel = GetPixel8(right_img, x - offset, y);
				uint8_t matching_cost = (uint8_t)abs((int)(left_pixel)-(int)(right_pixel));
				SetPixel8(&match_cost_images[offset], x, y, matching_cost);
			}
		}
	}

	return match_cost_images;
}

vx_image CreateBlockCostImages_Slow(vx_image pixel_cost_images, uint32_t block_halfsize, int16_t max_disparity)
{
	uint32_t width = pixel_cost_images[0].width;
	uint32_t height = pixel_cost_images[0].height;
	uint32_t num_pixels = width * height;
	uint32_t num_disparities = max_disparity + 1;

	vx_image block_cost_images = (vx_image)calloc(num_disparities, sizeof(struct _vx_image));

	for (uint32_t i = 0; i <= max_disparity; i++)
	{
		uint32_t *image_data = (uint32_t*)calloc(width*height, sizeof(uint32_t));
		block_cost_images[i].data = image_data;
		block_cost_images[i].width = width;
		block_cost_images[i].height = height;
		block_cost_images[i].image_type = VX_DF_IMAGE_S32;
		block_cost_images[i].color_space = VX_COLOR_SPACE_DEFAULT;
	}

	for (uint32_t offset = 0; offset <= max_disparity; offset++)
	{
		for (uint32_t y = block_halfsize; y < height - block_halfsize; y++)
		{
			for (uint32_t x = block_halfsize; x < width - block_halfsize; x++)
			{
				vx_rectangle_t block;
				block.start_x = x - block_halfsize;
				block.start_y = y - block_halfsize;
				block.end_x = x + block_halfsize;
				block.end_y = y + block_halfsize;

				uint32_t cost = SummOverBlock(&pixel_cost_images[offset], &block);
				SetPixel32(&block_cost_images[offset], x, y, cost);
			}
		}
	}

	return block_cost_images;
}

vx_image CreateBlockCostImages(vx_image pixel_cost_images, uint32_t block_halfsize, int16_t max_disparity)
{
	uint32_t width = pixel_cost_images[0].width;
	uint32_t height = pixel_cost_images[0].height;
	uint32_t num_pixels = width * height;
	uint32_t num_disparities = max_disparity + 1;

	vx_image block_cost_images = (vx_image)calloc(num_disparities, sizeof(struct _vx_image));

	for (uint32_t i = 0; i <= max_disparity; i++)
	{
		uint32_t *image_data = (uint32_t*)calloc(width*height, sizeof(uint32_t));
		block_cost_images[i].data = image_data;
		block_cost_images[i].width = width;
		block_cost_images[i].height = height;
		block_cost_images[i].image_type = VX_DF_IMAGE_S32;
		block_cost_images[i].color_space = VX_COLOR_SPACE_DEFAULT;
	}

	for (uint32_t offset = 0; offset <= max_disparity; offset++)
	{
		uint32_t x = offset + block_halfsize;
		uint32_t y = block_halfsize;

		vx_rectangle_t block;
		block.start_x = x - block_halfsize;
		block.start_y = y - block_halfsize;
		block.end_x = x + block_halfsize;
		block.end_y = y + block_halfsize;

		uint32_t cost = SummOverBlock(&pixel_cost_images[offset], &block);
		SetPixel32(&block_cost_images[offset], x, y, cost);

		for (x++; x < width - block_halfsize; x++)
		{
			cost = BlockCostEstimateFromLeft(&pixel_cost_images[offset], x, y, cost, block_halfsize);
			SetPixel32(&block_cost_images[offset], x, y, cost);
		}

		for (y++; y < height - block_halfsize; y++)
		{
			for (uint32_t x = block_halfsize + offset; x < width - block_halfsize; x++)
			{
				cost = GetPixel32(&block_cost_images[offset], x, y - 1);
				cost = BlockCostEstimateFromTop(&pixel_cost_images[offset], x, y, cost, block_halfsize);
				SetPixel32(&block_cost_images[offset], x, y, cost);
			}
		}
	}

	return block_cost_images;
}

uint32_t BlockCostEstimateFromLeft(vx_image pixel_cost_image, uint32_t x, uint32_t y, uint32_t left_cost, uint32_t block_halfsize)
{
	vx_rectangle_t temp_block;
	temp_block.start_x = x - 1 - block_halfsize;
	temp_block.end_x = x - 1 - block_halfsize;
	temp_block.start_y = y - block_halfsize;
	temp_block.end_y = y + block_halfsize;

	uint32_t take = SummOverBlock(pixel_cost_image, &temp_block);

	temp_block.start_x = x + block_halfsize;
	temp_block.end_x = x + block_halfsize;

	uint32_t add = SummOverBlock(pixel_cost_image, &temp_block);

	return left_cost - take + add;
}

uint32_t BlockCostEstimateFromTop(vx_image pixel_cost_image, uint32_t x, uint32_t y, uint32_t top_cost, uint32_t block_halfsize)
{
	vx_rectangle_t temp_block;
	temp_block.start_x = x - block_halfsize;
	temp_block.end_x = x + block_halfsize;
	temp_block.start_y = y - 1 - block_halfsize;
	temp_block.end_y = y - 1 - block_halfsize;

	uint32_t take = SummOverBlock(pixel_cost_image, &temp_block);

	temp_block.start_y = y + block_halfsize;
	temp_block.end_y = y + block_halfsize;

	uint32_t add = SummOverBlock(pixel_cost_image, &temp_block);

	return top_cost - take + add;
}

void FreeMatchingCostArrays(vx_image match_cost_images, int16_t max_disparity)
{
	for (uint32_t i = 0; i < max_disparity; i++)
	{
		free(match_cost_images[i].data);
	}
	free(match_cost_images);
}

void FreeBlockCostImage(vx_image block_cost_images, int16_t max_disparity)
{
	for (uint32_t i = 0; i < max_disparity; i++)
	{
		free(block_cost_images[i].data);
	}
	free(block_cost_images);
}

void Dbg_WriteMatchingCostImageToFile(vx_image match_cost_image)
{
	FILE *file = fopen("mc_data.txt", "w");

	uint32_t width = match_cost_image->width;
	uint32_t height = match_cost_image->height;

	for (uint32_t y = 0; y < height; y++)
	{
		for (uint32_t x = 0; x < width; x++)
		{
			fprintf(file, "%-3d ", GetPixel8(match_cost_image, x, y));
		}
		fprintf(file, "\n");
	}
}

int16_t Disparity(vx_coordinates2d_t *pixel, vx_image block_cost_images, const int16_t max_disparity, const uint32_t block_halfsize)
{
	const uint32_t NOT_DEFINED = UINT32_MAX;
	uint32_t min_diff = NOT_DEFINED;
	//uint32_t prev_diff = NOT_DEFINED;
	//uint32_t next_diff = NOT_DEFINED;
	uint32_t best_disp = 0;

	// write this shit good enough
	int16_t limit_disp = (int)(pixel->x) - max_disparity - (int)(block_halfsize) >= 0 ? max_disparity : pixel->x - block_halfsize;

	for (int16_t disp = 0; disp <= limit_disp; disp++)
	{
		uint32_t diff = GetPixel32(&block_cost_images[disp], pixel->x, pixel->y);
		if (diff < min_diff)
		{
			min_diff = diff;
			best_disp = disp;
		}
	}

	return best_disp;
}

int16_t FindDisparity(
	const vx_coordinates2d_t *pixel, const vx_image match_cost_images,
	const uint32_t block_halfsize, const int16_t max_disparity, const bool search_right)
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
	uint32_t best_disp = 0;

	int search_dir_sign = search_right ? +1 : -1;

	for (int16_t disp = 0; disp <= max_disparity; disp++)
	{
		uint32_t match_x = pixel->x + disp * search_dir_sign;
		if ((int)(match_x - block_halfsize) < 0 || match_x + block_halfsize >= match_cost_images[disp].width)
			break;

		const vx_image match_cost_image = (const vx_image)&match_cost_images[disp];
		uint32_t diff = SummOverBlock(match_cost_image, &template_block);
		if (diff < min_diff)
		{
			best_disp = disp;
			prev_diff = min_diff;
			min_diff = diff;
			next_diff = NOT_DEFINED;
		}
		if (disp - best_disp == 1)
		{
			next_diff = diff;
		}
	}

	int16_t disparity = best_disp;

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
			uint8_t source_pixel = GetPixel8(source_img, template_block->start_x + x, template_block->start_y + y);
			uint8_t match_pixel = GetPixel8(match_img, match_block->x + x, match_block->y + y);
			sum += abs((int)(source_pixel)-(int)(match_pixel));
		}
	}

	return sum;
}

uint32_t SummOverBlock(const vx_image match_cost_image, const vx_rectangle_t *block)
{
	uint32_t sum = 0;

	for (uint32_t y = block->start_y; y <= block->end_y; y++)
	{
		for (uint32_t x = block->start_x; x <= block->end_x; x++)
		{
			sum += GetPixel8(match_cost_image, x, y);
		}
	}

	return sum;
}

int16_t SubPixelEstimation(int16_t disparity, int prev_cost, int current_cost, int next_cost)
{
	float denum = (float)(prev_cost - 2 * current_cost + next_cost);
	if (denum != 0)
		return (int16_t)(disparity - 0.5f * (float)(next_cost - prev_cost) / denum);
	else
		return disparity;
}