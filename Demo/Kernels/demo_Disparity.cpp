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
	cv::Mat m_leftImage;
	cv::Mat m_rightImage;
	//cv::Mat m_disparityImage;
};

// TODO: убрать это
#pragma warning(disable : 4189) // unused variable 

///////////////////////////////////////////////////////////////////////////////
void demo_DisparityMap::execute()
{
	cv::namedWindow("Left", CV_WINDOW_NORMAL);
	cv::namedWindow("Right", CV_WINDOW_NORMAL);
	cv::namedWindow("Disparity Map", CV_WINDOW_NORMAL);
	
	const std::string leftImgPath = "..\\Image\\disparity\\01left.png";
	const std::string rightImgPath = "..\\Image\\disparity\\01right.png";
	m_leftImage = cv::imread(leftImgPath, CV_LOAD_IMAGE_GRAYSCALE);
	m_rightImage = cv::imread(rightImgPath, CV_LOAD_IMAGE_GRAYSCALE);
	cv::imshow("Left", m_leftImage);
	cv::imshow("Right", m_rightImage);

	const cv::Size leftSize(m_leftImage.cols, m_rightImage.rows);
	const cv::Size rightSize(m_rightImage.cols, m_rightImage.rows);

	if (leftSize != rightSize)
	{
		std::cout << "ERROR: Left image size is not equal to right image size." << std::endl;
		return;
	}

	///@{ OPENVX
	_vx_image leftVXImage = {
		m_leftImage.data,
		leftSize.width,
		leftSize.height,
		VX_DF_IMAGE_U8,
		VX_COLOR_SPACE_DEFAULT
	};

	_vx_image rightVXImage = {
		m_rightImage.data,
		rightSize.width,
		rightSize.height,
		VX_DF_IMAGE_U8,
		VX_COLOR_SPACE_DEFAULT
	};

	uint8_t* disparityImageData = static_cast<uint8_t*>(calloc(leftSize.width * leftSize.height, sizeof(uint8_t)));
	_vx_image disparityVXImage = {
		disparityImageData,
		leftSize.width,
		leftSize.height,
		VX_DF_IMAGE_U8,
		VX_COLOR_SPACE_DEFAULT
	};

	ref_DisparityMap(&leftVXImage, &rightVXImage, &disparityVXImage, 5, 50);

	const cv::Mat vxImage = cv::Mat(leftSize, CV_8UC1, disparityImageData);
	cv::imshow("Disparity Map", vxImage);

	///@}

	cv::waitKey(0);
	free(disparityImageData);
}

///////////////////////////////////////////////////////////////////////////////
void demo_DisparityMap::applyParameters(int, void*)
{
	// TODO: код из execute() сюда

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
