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
		m_blockSize = 7;
		m_disparityThreshold = 50;
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
	cv::Mat m_leftImage;
	cv::Mat m_rightImage;
	cv::Mat m_sourceImage;

	int m_blockSize;
	int m_disparityThreshold;
};

///////////////////////////////////////////////////////////////////////////////
namespace
{
	const std::string SourceImageWindowName = "Source Image";
	const std::string DisparityMapWindowName = "Disparity Map";
	const std::string ControlsWindowName = "Controls";
	
}

///////////////////////////////////////////////////////////////////////////////
void demo_DisparityMap::execute()
{
	cv::namedWindow(SourceImageWindowName, CV_WINDOW_NORMAL);
	cv::namedWindow(DisparityMapWindowName, CV_WINDOW_NORMAL);
	cv::namedWindow(ControlsWindowName, CV_WINDOW_AUTOSIZE | CV_GUI_NORMAL);
	
	cv::createTrackbar("Block Size", ControlsWindowName, &m_blockSize, 20, applyParameters, this);
	cv::createTrackbar("Max Disp", ControlsWindowName, &m_disparityThreshold, 120, applyParameters, this);

	const std::string leftImgPath = "..\\Image\\disparity\\01left.png";
	const std::string rightImgPath = "..\\Image\\disparity\\01right.png";
	m_leftImage = cv::imread(leftImgPath, CV_LOAD_IMAGE_GRAYSCALE);
	m_rightImage = cv::imread(rightImgPath, CV_LOAD_IMAGE_GRAYSCALE);
	m_sourceImage = cv::imread(leftImgPath, CV_LOAD_IMAGE_GRAYSCALE);

	const cv::Size leftSize(m_leftImage.cols, m_rightImage.rows);
	const cv::Size rightSize(m_rightImage.cols, m_rightImage.rows);

	if (leftSize != rightSize)
	{
		std::cout << "ERROR: Left image size is not equal to right image size." << std::endl;
		return;
	}

	cv::imshow(SourceImageWindowName, m_sourceImage);

	applyParameters(0, this);
	cv::waitKey(0);
}

///////////////////////////////////////////////////////////////////////////////
void demo_DisparityMap::applyParameters(int, void* pointer)
{
	demo_DisparityMap *pThis = (demo_DisparityMap*)(pointer);

	const cv::Size size(pThis->m_leftImage.cols, pThis->m_rightImage.rows);

	///@{ OPENVX
	_vx_image leftVXImage = {
		pThis->m_leftImage.data,
		size.width,
		size.height,
		VX_DF_IMAGE_U8,
		VX_COLOR_SPACE_DEFAULT
	};

	_vx_image rightVXImage = {
		pThis->m_rightImage.data,
		size.width,
		size.height,
		VX_DF_IMAGE_U8,
		VX_COLOR_SPACE_DEFAULT
	};

	int16_t* disparityImageData = (int16_t*)(calloc(size.width * size.height, sizeof(int16_t)));
	_vx_image disparityVXImage = {
		disparityImageData,
		size.width,
		size.height,
		VX_DF_IMAGE_S16,
		VX_COLOR_SPACE_DEFAULT
	};

	ref_DisparityMap(&leftVXImage, &rightVXImage, &disparityVXImage, pThis->m_blockSize, (int16_t)pThis->m_disparityThreshold);

	const cv::Mat disparityMap16Bits = cv::Mat(size, CV_16SC1, disparityImageData);
	cv::Mat       disparityMap8Bits;

	double minVal, maxVal;
	minMaxLoc(disparityMap16Bits, &minVal, &maxVal);
	
	if (maxVal - minVal != 0)
	{
		disparityMap16Bits.convertTo(disparityMap8Bits, CV_8UC1, 255 / (maxVal - minVal));
		cv::applyColorMap(disparityMap8Bits, disparityMap8Bits, cv::ColormapTypes::COLORMAP_JET);

		cv::imshow(DisparityMapWindowName, disparityMap8Bits);
	}
	
	free(disparityImageData);

	///@}
}

///////////////////////////////////////////////////////////////////////////////
IDemoCasePtr CreateDisparityMapDemo()
{
	return std::make_unique<demo_DisparityMap>();
}
