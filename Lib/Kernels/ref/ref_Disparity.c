//@file ref_Disparity.c
//@brief Contains implementation of functions aimed to getting disparity map
//@author Max Kimlyk
//@date 17 April 2016

// TODO: убрать это
#pragma warning( disable : 4189 )

#include "../ref.h"
#include <memory.h>

vx_status ref_DisparityMap(const vx_image src_image, vx_image dst_image)
{
	const uint32_t src_width = src_image->width;
	const uint32_t src_height = src_image->height;
	const uint32_t dst_width = dst_image->width;
	const uint32_t dst_height = dst_image->height;

	if (src_width != dst_width || src_height != dst_height)
	{
		return VX_ERROR_INVALID_PARAMETERS;
	}

	const uint8_t* src_data = src_image->data;
	uint8_t* dst_data = dst_image->data;

	size_t size = dst_image->width * dst_image->height * sizeof(uint8_t);
	memcpy(dst_data, src_data, size);

	return VX_SUCCESS;
}
