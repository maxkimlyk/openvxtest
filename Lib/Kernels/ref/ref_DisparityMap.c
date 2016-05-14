//@file ref_Disparity.c
//@brief Contains implementation of functions aimed to getting disparity map
//@author Max Kimlyk
//@date 17 April 2016

#include "../ref.h"
#include <memory.h>

// Function prototypes
bool CheckImageSizes(const vx_image left_img, const vx_image right_img, const vx_image disp_img);

uint8_t GetPixel8U(const vx_image image, uint32_t x, uint32_t y);
void    SetPixel8U(vx_image image, uint32_t x, uint32_t y, uint8_t value);
int16_t GetPixel16S(const vx_image image, uint32_t x, uint32_t y);
void    SetPixel16S(vx_image image, uint32_t x, uint32_t y, int16_t value);
uint32_t GetPixel32U(const vx_image image, uint32_t x, uint32_t y);
void     SetPixel32U(vx_image image, uint32_t x, uint32_t y, uint32_t value);

vx_image CreatePixelCostImages(vx_image left_img, vx_image right_img, int16_t max_disparity);
void     FreePixelCostImages(vx_image match_cost_images, int16_t max_disparity);

vx_image CreateBlockCostImages(vx_image pixel_cost_images, uint32_t block_halfsize, int16_t max_disparity);
void     FillBlockCostImage(vx_image block_cost_image, vx_image pixel_cost_image, int16_t offset, uint32_t block_halfsize);
void     FreeBlockCostImage(vx_image block_cost_images, int16_t max_disparity);

int16_t Disparity(vx_coordinates2d_t *pixel, vx_image block_cost_images, const int16_t max_disparity, const uint32_t block_halfsize);
int16_t SubPixelEstimation(int16_t disparity, int prev_cost, int current_cost, int next_cost);

uint32_t SummOverBlock(const vx_image match_cost_image, const vx_rectangle_t *block);
///////////////////////////////////////////////////////////////////////////////

vx_status ref_DisparityMap(
	const vx_image left_img, const vx_image right_img, vx_image disp_img,
	const uint32_t block_size, const int16_t max_disparity)
{
	if (!CheckImageSizes(left_img, right_img, disp_img))
	{
		return VX_ERROR_INVALID_PARAMETERS;
	}

	const uint32_t width = left_img->width;
	const uint32_t height = left_img->height;
	const uint32_t block_halfsize = block_size / 2;

	const vx_image template_img = left_img;
	const vx_image match_img = right_img;

	struct _vx_image *pixel_cost_images = CreatePixelCostImages(template_img, match_img, max_disparity);
	struct _vx_image *block_cost_images = CreateBlockCostImages(pixel_cost_images, block_halfsize, max_disparity);

	FreePixelCostImages(pixel_cost_images, max_disparity);

	vx_coordinates2d_t pixel;

	for (pixel.y = block_halfsize; pixel.y < height - block_halfsize; pixel.y++)
	{
		for (pixel.x = block_halfsize; pixel.x < width - block_halfsize; pixel.x++)
		{
			int16_t disparity = Disparity(&pixel, block_cost_images, max_disparity, block_halfsize);
			SetPixel16S(disp_img, pixel.x, pixel.y, disparity);
		}
	}

	FreeBlockCostImage(block_cost_images, max_disparity);

	return VX_SUCCESS;
}

bool CheckImageSizes(const vx_image left_img, const vx_image right_img, const vx_image disp_img)
{
	return (left_img->width == right_img->width && left_img->width == disp_img->width &&
		left_img->height == right_img->height && left_img->height == disp_img->height);
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

vx_image CreatePixelCostImages(vx_image left_img, vx_image right_img, int16_t max_disparity)
{
	uint32_t width = left_img->width;
	uint32_t height = right_img->height;
	uint32_t num_disparities = max_disparity + 1;

	vx_image match_cost_images = (vx_image)calloc(num_disparities, sizeof(struct _vx_image));

	for (int16_t i = 0; i <= max_disparity; i++)
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
			uint8_t left_pixel = GetPixel8U(left_img, x, y);

			for (int16_t offset = 0; offset <= max_disparity; offset++)
			{
				if ((int)x - (int)offset < 0)
					break;

				uint8_t right_pixel = GetPixel8U(right_img, x - offset, y);
				uint8_t matching_cost = (uint8_t)abs((int)(left_pixel)-(int)(right_pixel));
				SetPixel8U(&match_cost_images[offset], x, y, matching_cost);
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
		uint32_t *image_data = (uint32_t*)calloc(width*height, sizeof(uint32_t));
		block_cost_images[i].data = image_data;
		block_cost_images[i].width = width;
		block_cost_images[i].height = height;
		block_cost_images[i].image_type = VX_DF_IMAGE_S32;
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
			uint32_t take_take = GetPixel8U(pixel_cost_image, x - 1 - block_halfsize, y - 1 - block_halfsize);
			uint32_t take_add = GetPixel8U(pixel_cost_image, x + block_halfsize, y - 1 - block_halfsize);
			take = take - take_take + take_add;

			uint32_t add_take = GetPixel8U(pixel_cost_image, x - 1 - block_halfsize, y + block_halfsize);
			uint32_t add_add = GetPixel8U(pixel_cost_image, x + block_halfsize, y + block_halfsize);
			add = add - add_take + add_add;

			cost = GetPixel32U(block_cost_image, x, y - 1);
			cost = cost - take + add;
			SetPixel32U(block_cost_image, x, y, cost);
		}
	}
}

void FreePixelCostImages(vx_image match_cost_images, int16_t max_disparity)
{
	for (int16_t i = 0; i <= max_disparity; i++)
	{
		free(match_cost_images[i].data);
	}
	free(match_cost_images);
}

void FreeBlockCostImage(vx_image block_cost_images, int16_t max_disparity)
{
	for (int16_t i = 0; i <= max_disparity; i++)
	{
		free(block_cost_images[i].data);
	}
	free(block_cost_images);
}

int16_t Disparity(vx_coordinates2d_t *pixel, vx_image block_cost_images, const int16_t max_disparity, const uint32_t block_halfsize)
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
			sum += GetPixel8U(match_cost_image, x, y);
		}
	}

	return sum;
}