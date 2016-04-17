//@file demo_DisparityMap.cpp
//@brief Contains demo for disparity functions
//@author Kimlyk Max
//@date 17 April 2016

#include "../stdafx.h"
#include <opencv2/opencv.hpp>

extern "C"
{
#include "Lib/Kernels/ref.h"
#include "Lib/Common/types.h"
}

#include "../DemoEngine.h"

///////////////////////////////////////////////////////////////////////////////
//@brief Demonstration of DisparityMap function
class demo_DisparityMap: public IDemoCase
{
public:
	///@brief defaut constructor
	demo_DisparityMap()
	{
	}

	///@see IDemoCase::ReplyName
	virtual std::string ReplyName() const override
	{
		return "DisparityMap";
	}

private:
	///@see IDemoCase::execute
	virtual void execute() override;

	///@brief provide interactive demo
	static void applyParameters(int pos, void* data);

private:
	cv::Mat m_srcImage;
};

///////////////////////////////////////////////////////////////////////////////
namespace
{
	const std::string m_SrcWindow = "src";
	const std::string m_DestWindow = "dest";
	//const std::string m_diffWindow = m_openVXWindow + "-" + m_openCVWindow;
}

///////////////////////////////////////////////////////////////////////////////
void demo_DisparityMap::execute()
{
	cv::namedWindow(m_SrcWindow, CV_WINDOW_NORMAL);
	cv::namedWindow(m_DestWindow, CV_WINDOW_NORMAL);
	
	const std::string imgPath = "..\\Image\\testimg1_8UC1.png";
	m_srcImage = cv::imread(imgPath, CV_LOAD_IMAGE_GRAYSCALE);
	cv::imshow(m_SrcWindow, m_srcImage);

	const cv::Size imgSize(m_srcImage.cols, m_srcImage.rows);

	///@{ OPENVX
	//_vx_threshold vxThresh = { VX_THRESHOLD_TYPE_BINARY, uint8_t(demo->m_threshold), 0/* dummy value */, 255 /* dummy value */ };
	_vx_image srcVXImage = {
		m_srcImage.data,
		imgSize.width,
		imgSize.height,
		VX_DF_IMAGE_U8,
		VX_COLOR_SPACE_DEFAULT
	};

	uint8_t* outVXImage = static_cast<uint8_t*>(calloc(imgSize.width* imgSize.height, sizeof(uint8_t)));
	_vx_image dstVXImage = {
		outVXImage,
		imgSize.width,
		imgSize.height,
		VX_DF_IMAGE_U8,
		VX_COLOR_SPACE_DEFAULT
	};

	ref_DisparityMap(&srcVXImage, &dstVXImage);

	const cv::Mat vxImage = cv::Mat(imgSize, CV_8UC1, outVXImage);
	cv::imshow(m_DestWindow, vxImage);

	///@}

	//applyParameters(0, this);

	cv::waitKey(0);
}

///////////////////////////////////////////////////////////////////////////////
void demo_DisparityMap::applyParameters(int, void*)
{
	//auto demo = static_cast<demo_DisparityMap*>(data);

	// Show difference of OpenVX and OpenCV
	//const cv::Mat diffImage(imgSize, CV_8UC1);
	//cv::absdiff(vxImage, cvImage, diffImage);
	//cv::imshow(m_diffWindow, diffImage);
}

///////////////////////////////////////////////////////////////////////////////
IDemoCasePtr CreateDisparityMapDemo()
{
	return std::make_unique<demo_DisparityMap>();
}
