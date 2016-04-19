//@file ref_Disparity.c
//@brief Contains implementation of functions aimed to getting disparity map
//@author Max Kimlyk
//@date 17 April 2016

#include "../ref.h"
#include <memory.h>

vx_status ref_DisparityMap(const vx_image left_image, const vx_image right_image, vx_image disparity_image)
{
	const uint32_t width = left_image->width;
	const uint32_t height = left_image->height;
	/*const uint32_t dst_width = dst_image->width;
	const uint32_t dst_height = dst_image->height;

	if (src_width != dst_width || src_height != dst_height)
	{
		return VX_ERROR_INVALID_PARAMETERS;
	}*/

	const uint8_t* src_data = left_image->data;
	uint8_t* dst_data = disparity_image->data;

	size_t size = width * height * sizeof(uint8_t);
	memcpy(dst_data, src_data, size);

	return VX_SUCCESS;
}
