//@file ref_Disparity.c
//@brief Contains implementation of functions aimed to getting disparity map
//@author Max Kimlyk
//@date 17 April 2016

#include "../ref.h"
#include <memory.h>

// Function prototypes
bool CheckImageSizes(const vx_image left_img, const vx_image right_img, const vx_image disp_img);
bool CheckImageFormats(const vx_image left_img, const vx_image right_img, const vx_image disp_img);

uint8_t GetPixel8U(const vx_image image, uint32_t x, uint32_t y);
void    SetPixel8U(vx_image image, uint32_t x, uint32_t y, uint8_t value);
int16_t GetPixel16S(const vx_image image, uint32_t x, uint32_t y);
void    SetPixel16S(vx_image image, uint32_t x, uint32_t y, int16_t value);
uint32_t GetPixel32U(const vx_image image, uint32_t x, uint32_t y);
void     SetPixel32U(vx_image image, uint32_t x, uint32_t y, uint32_t value);

vx_image SobelFilter(vx_image src);

vx_image CreatePixelCostImages(vx_image left_img, vx_image right_img, int16_t max_disparity);
vx_image CreateBlockCostImages(vx_image pixel_cost_images, uint32_t block_halfsize, int16_t max_disparity);
void     FillBlockCostImage(vx_image block_cost_image, vx_image pixel_cost_image, int16_t offset, uint32_t block_halfsize);

void FreeImage(vx_image image);
void FreeImageArray(vx_image images, int32_t size);

int16_t Disparity(
	vx_coordinates2d_t *pixel, vx_image block_cost_images,
	const int16_t max_disparity, const uint32_t block_halfsize, const uint32_t uniqueness_threshold);

int16_t SubPixelEstimation(int16_t disparity, int prev_cost, int current_cost, int next_cost);

uint32_t SummOverBlock(const vx_image match_cost_image, const vx_rectangle_t *block);

void    InterpolateBadPixels(vx_image image);
int16_t Interpolate(vx_image image, vx_coordinates2d_t *pixel);
///////////////////////////////////////////////////////////////////////////////

// GLOBAL CONSTANTS
#define UNRELIABLE -1
///////////////////////////////////////////////////////////////////////////////

vx_status ref_DisparityMap(
	const vx_image left_img, const vx_image right_img, vx_image disp_img,
	const uint32_t block_size, const int16_t max_disparity, const uint32_t uniqueness_threshold)
{
	if (!CheckImageSizes(left_img, right_img, disp_img) || !CheckImageFormats(left_img, right_img, disp_img))
	{
		return VX_ERROR_INVALID_PARAMETERS;
	}

	// TODO: check image formats

	const uint32_t width = left_img->width;
	const uint32_t height = left_img->height;
	const uint32_t block_halfsize = block_size / 2;

	vx_image left_filtered = SobelFilter(left_img);
	vx_image right_filtered = SobelFilter(right_img);

	struct _vx_image *pixel_cost_images = CreatePixelCostImages(left_filtered, right_filtered, max_disparity);
	struct _vx_image *block_cost_images = CreateBlockCostImages(pixel_cost_images, block_halfsize, max_disparity);

	FreeImageArray(pixel_cost_images, (size_t)(max_disparity + 1));
	FreeImage(left_filtered);
	FreeImage(right_filtered);

	vx_coordinates2d_t pixel;

	for (pixel.y = block_halfsize; pixel.y < height - block_halfsize; pixel.y++)
	{
		for (pixel.x = (uint32_t)(max_disparity); pixel.x < width - block_halfsize; pixel.x++)
		{
			int16_t disparity = Disparity(&pixel, block_cost_images, max_disparity, block_halfsize, uniqueness_threshold);
			SetPixel16S(disp_img, pixel.x, pixel.y, disparity);
		}
	}

	FreeImageArray(block_cost_images, (size_t)(max_disparity + 1));

	return VX_SUCCESS;
}

bool CheckImageSizes(const vx_image left_img, const vx_image right_img, const vx_image disp_img)
{
	return (left_img->width == right_img->width && left_img->width == disp_img->width &&
		left_img->height == right_img->height && left_img->height == disp_img->height);
}

bool CheckImageFormats(const vx_image left_img, const vx_image right_img, const vx_image disp_img)
{
	return (left_img->image_type == VX_DF_IMAGE_U8 && right_img->image_type == VX_DF_IMAGE_U8 && disp_img->image_type == VX_DF_IMAGE_S16);
}

uint8_t GetPixel8U(const vx_image image, uint32_t x, uint32_t y)
{
	uint8_t *pixels = (uint8_t*)(image->data);
	return pixels[y * image->width + x];
}

void SetPixel8U(vx_image image, uint32_t x, uint32_t y, uint8_t value)
{
	uint8_t *pixels = (uint8_t*)(image->data);
	pixels[y * image->width + x] = value;
}

int16_t GetPixel16S(const vx_image image, uint32_t x, uint32_t y)
{
	int16_t *pixels = (int16_t*)(image->data);
	return pixels[y * image->width + x];
}

void SetPixel16S(vx_image image, uint32_t x, uint32_t y, int16_t value)
{
	int16_t *pixels = (int16_t*)(image->data);
	pixels[y * image->width + x] = value;
}

uint32_t GetPixel32U(const vx_image image, uint32_t x, uint32_t y)
{
	uint32_t *pixels = (uint32_t*)(image->data);
	return pixels[y * image->width + x];
}

void SetPixel32U(vx_image image, uint32_t x, uint32_t y, uint32_t value)
{
	uint32_t *pixels = (uint32_t*)(image->data);
	pixels[y * image->width + x] = value;
}

vx_image SobelFilter(vx_image src)
{
	const uint32_t width = src->width;
	const uint32_t height = src->height;

	vx_image dest = (vx_image)malloc(sizeof(struct _vx_image));
	dest->data = calloc(width * height, sizeof(int16_t));
	dest->width = width;
	dest->height = height;
	dest->image_type = VX_DF_IMAGE_S16;
	dest->color_space = VX_COLOR_SPACE_DEFAULT;

	const int kernel[] = {
		-1, 0, 1,
		-2, 0, 2,
		-1, 0, 1
	};

	const uint32_t kernel_halfsize = 1;
	const uint32_t kernel_size = 3;

	for (uint32_t y = kernel_halfsize; y < height - kernel_halfsize; y++)
	{
		for (uint32_t x = kernel_halfsize; x < width - kernel_halfsize; x++)
		{
			int sum = 0;
			for (uint32_t i = 0; i < kernel_size; i++)
			{
				for (uint32_t j = 0; j < kernel_size; j++)
				{
					uint8_t pixel = GetPixel8U(src, x - kernel_halfsize + j, y - kernel_halfsize + i);
					sum += kernel[i * kernel_size + j] * pixel;
				}
			}
			// TODO: deside: abs or not abs
			SetPixel16S(dest, x, y, (int16_t)(sum));
		}
	}

	return dest;
}

vx_image CreatePixelCostImages(vx_image left_img, vx_image right_img, int16_t max_disparity)
{
	uint32_t width = left_img->width;
	uint32_t height = right_img->height;
	uint32_t num_disparities = max_disparity + 1;

	vx_image match_cost_images = (vx_image)calloc(num_disparities, sizeof(struct _vx_image));

	for (int16_t i = 0; i <= max_disparity; i++)
	{
		match_cost_images[i].data = calloc(width*height, sizeof(int16_t));
		match_cost_images[i].width = width;
		match_cost_images[i].height = height;
		match_cost_images[i].image_type = VX_DF_IMAGE_S16;
		match_cost_images[i].color_space = VX_COLOR_SPACE_DEFAULT;
	}

	for (uint32_t y = 0; y < height; y++)
	{
		for (uint32_t x = 0; x < width; x++)
		{
			int16_t left_pixel = GetPixel16S(left_img, x, y);

			for (int16_t offset = 0; offset <= max_disparity; offset++)
			{
				if ((int)x - (int)offset < 0)
					break;

				int16_t right_pixel = GetPixel16S(right_img, x - offset, y);
				int16_t matching_cost = (int16_t)abs(left_pixel - right_pixel);
				SetPixel16S(&match_cost_images[offset], x, y, matching_cost);
			}
		}
	}

	return match_cost_images;
}

vx_image CreateBlockCostImages(vx_image pixel_cost_images, uint32_t block_halfsize, int16_t max_disparity)
{
	uint32_t width = pixel_cost_images[0].width;
	uint32_t height = pixel_cost_images[0].height;
	uint32_t num_disparities = max_disparity + 1;

	vx_image block_cost_images = (vx_image)calloc(num_disparities, sizeof(struct _vx_image));

	for (int16_t i = 0; i <= max_disparity; i++)
	{
		block_cost_images[i].data = calloc(width*height, sizeof(uint32_t));
		block_cost_images[i].width = width;
		block_cost_images[i].height = height;
		block_cost_images[i].image_type = VX_DF_IMAGE_U32;
		block_cost_images[i].color_space = VX_COLOR_SPACE_DEFAULT;
	}

	for (int16_t offset = 0; offset <= max_disparity; offset++)
	{
		FillBlockCostImage(&block_cost_images[offset], &pixel_cost_images[offset], offset, block_halfsize);
	}

	return block_cost_images;
}

void FillBlockCostImage(vx_image block_cost_image, vx_image pixel_cost_image, int16_t offset, uint32_t block_halfsize)
{
	uint32_t width = block_cost_image->width;
	uint32_t height = block_cost_image->height;
	uint32_t x = offset + block_halfsize;
	uint32_t y = block_halfsize;

	// first pixel
	vx_rectangle_t block;
	block.start_x = x - block_halfsize;
	block.start_y = y - block_halfsize;
	block.end_x = x + block_halfsize;
	block.end_y = y + block_halfsize;

	uint32_t cost = SummOverBlock(pixel_cost_image, &block);
	SetPixel32U(block_cost_image, x, y, cost);

	uint32_t take;
	uint32_t add;

	// first row
	for (x++; x < width - block_halfsize; x++)
	{
		block.start_x = x - 1 - block_halfsize;
		block.end_x = x - 1 - block_halfsize;
		block.start_y = y - block_halfsize;
		block.end_y = y + block_halfsize;

		take = SummOverBlock(pixel_cost_image, &block);

		block.start_x = x + block_halfsize;
		block.end_x = x + block_halfsize;

		add = SummOverBlock(pixel_cost_image, &block);

		cost = cost - take + add;
		SetPixel32U(block_cost_image, x, y, cost);
	}

	for (y++; y < height - block_halfsize; y++)
	{
		// first pixel in row
		x = block_halfsize + offset;
		cost = GetPixel32U(block_cost_image, x, y - 1);

		block.start_x = x - block_halfsize;
		block.end_x = x + block_halfsize;
		block.start_y = y - 1 - block_halfsize;
		block.end_y = y - 1 - block_halfsize;

		take = SummOverBlock(pixel_cost_image, &block);

		block.start_y = y + block_halfsize;
		block.end_y = y + block_halfsize;

		add = SummOverBlock(pixel_cost_image, &block);

		cost = cost - take + add;
		SetPixel32U(block_cost_image, x, y, cost);

		// other pixels in row
		for (x++; x < width - block_halfsize; x++)
		{
			uint32_t take_take = GetPixel16S(pixel_cost_image, x - 1 - block_halfsize, y - 1 - block_halfsize);
			uint32_t take_add = GetPixel16S(pixel_cost_image, x + block_halfsize, y - 1 - block_halfsize);
			take = take - take_take + take_add;

			uint32_t add_take = GetPixel16S(pixel_cost_image, x - 1 - block_halfsize, y + block_halfsize);
			uint32_t add_add = GetPixel16S(pixel_cost_image, x + block_halfsize, y + block_halfsize);
			add = add - add_take + add_add;

			cost = GetPixel32U(block_cost_image, x, y - 1);
			cost = cost - take + add;
			SetPixel32U(block_cost_image, x, y, cost);
		}
	}
}

void FreeImage(vx_image image)
{
	if (image)
	{
		if (image->data)
			free(image->data);
		free(image);
	}
}

void FreeImageArray(vx_image images, int32_t size)
{
	if (images)
	{
		for (int32_t i = 0; i < size; i++)
		{
			if (images[i].data)
				free(images[i].data);
		}
		free(images);
	}
}

int16_t Disparity(
	vx_coordinates2d_t *pixel, vx_image block_cost_images,
	const int16_t max_disparity, const uint32_t block_halfsize, const uint32_t uniqueness_threshold)
{
	uint32_t min_diff = UINT32_MAX;
	int16_t best_disp = 0;

	int16_t limit_disp = pixel->x >= block_halfsize + max_disparity ? max_disparity : (int16_t)(pixel->x - block_halfsize);

	for (int16_t disp = 0; disp <= limit_disp; disp++)
	{
		uint32_t diff = GetPixel32U(&block_cost_images[disp], pixel->x, pixel->y);
		if (diff < min_diff)
		{
			min_diff = diff;
			best_disp = disp;
		}
	}

	if (uniqueness_threshold > 0)
	{
		uint32_t disp_uniqueness_threshold = (uint32_t)(min_diff * (1 + 0.01f * (float)uniqueness_threshold));
		for (int16_t disp = 0; disp <= limit_disp; disp++)
		{
			if (disp != best_disp && disp != best_disp - 1 && disp != best_disp + 1)
			{
				uint32_t diff = GetPixel32U(&block_cost_images[disp], pixel->x, pixel->y);
				if (diff < disp_uniqueness_threshold)
					return UNRELIABLE;
			}
		}
	}

	if (0 < best_disp && best_disp < limit_disp)
	{
		uint32_t prev_diff = GetPixel32U(&block_cost_images[best_disp - 1], pixel->x, pixel->y);
		uint32_t next_diff = GetPixel32U(&block_cost_images[best_disp + 1], pixel->x, pixel->y);
		return SubPixelEstimation(best_disp, prev_diff, min_diff, next_diff);
	}

	return best_disp;
}

int16_t SubPixelEstimation(int16_t disparity, int prev_cost, int current_cost, int next_cost)
{
	float denum = (float)(prev_cost - 2 * current_cost + next_cost);
	if (denum != 0)
		return (int16_t)(disparity - 0.5f * (float)(next_cost - prev_cost) / denum);
	else
		return disparity;
}

uint32_t SummOverBlock(const vx_image match_cost_image, const vx_rectangle_t *block)
{
	uint32_t sum = 0;

	for (uint32_t y = block->start_y; y <= block->end_y; y++)
	{
		for (uint32_t x = block->start_x; x <= block->end_x; x++)
		{
			sum += (uint32_t)GetPixel16S(match_cost_image, x, y);
		}
	}

	return sum;
}

void InterpolateBadPixels(vx_image image)
{
	vx_coordinates2d_t pixel;
	for (pixel.y = 0; pixel.y < image->height; pixel.y++)
	{
		for (pixel.x = 0; pixel.x < image->width; pixel.x++)
		{
			if (GetPixel16S(image, pixel.x, pixel.y) == UNRELIABLE)
			{
				int16_t new_value = Interpolate(image, &pixel);
				SetPixel16S(image, pixel.x, pixel.y, new_value);
			}
		}
	}
}

int16_t Interpolate(vx_image image, vx_coordinates2d_t *pixel)
{
	const int16_t kernel[] = {
		1, 2, 3, 2, 1,
		2, 4, 6, 4, 2,
		3, 6, 9, 6, 3,
		2, 4, 6, 4, 2,
		1, 2, 3, 2, 1
	};
	const uint32_t kernel_halfsize = 2;
	const uint32_t kernel_size = 5;

	int sum = 0;
	int weight = 0;
	uint32_t num_valid = 0;

	for (uint32_t i = 0; i < kernel_size; i++)
	{
		for (uint32_t j = 0; j < kernel_size; j++)
		{
			int x = (int)(pixel->x) - (int)kernel_halfsize + (int)(j);
			int y = (int)(pixel->y) - (int)kernel_halfsize + (int)(i);

			if (x < 0 || y < 0 || x >= (int)(image->width) || y >= (int)(image->height))
				continue;

			int16_t current_pixel = GetPixel16S(image, x, y);
			if (current_pixel == UNRELIABLE)
				continue;

			sum += kernel[i*kernel_size + j] * current_pixel;
			weight += kernel[i*kernel_size + j];
			num_valid++;
		}
	}

	if (num_valid > 5 && abs(sum) > 30)
	{
		int16_t interpolated = (int16_t)(sum / weight);
		return interpolated;
	}
	else
		return UNRELIABLE;
}