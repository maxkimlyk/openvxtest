//@file demo_DisparityMap.cpp
//@brief Contains demo for disparity functions
//@author Kimlyk Max
//@date 17 April 2016

#include "../stdafx.h"
#include <opencv2/opencv.hpp>
#include <opencv2/calib3d.hpp>

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
	demo_DisparityMap(const std::string image)
	{
		m_image = image;
		m_blockHalfsize = 5;
		m_numDisparities = 64;
		m_uniquenessThreshold = 15;
	}

	///@see IDemoCase::ReplyName
	virtual std::string ReplyName() const override
	{
		return "DisparityMap: " + m_image;
	}

private:
	///@see IDemoCase::execute
	virtual void execute() override;

	///@brief provide interactive demo
	static void applyParameters(int pos, void* data);

private:
	std::string m_image;

	cv::Mat m_leftImage;
	cv::Mat m_rightImage;
	cv::Mat m_sourceImage;

	int m_blockHalfsize;
	int m_numDisparities;
	int m_uniquenessThreshold;
};

///////////////////////////////////////////////////////////////////////////////
namespace
{
	const std::string SourceImageWindowName = "Source Image";
	const std::string DisparityMapWindowName = "My Disparity Map";
	const std::string ControlsWindowName = "Controls";
	const std::string CVDisparityMapWindowName = "OpevCV Disparity Map";
}

///////////////////////////////////////////////////////////////////////////////
void demo_DisparityMap::execute()
{
	cv::namedWindow(SourceImageWindowName, CV_WINDOW_NORMAL | CV_WINDOW_AUTOSIZE);
	cv::namedWindow(DisparityMapWindowName, CV_WINDOW_NORMAL | CV_WINDOW_AUTOSIZE);
	cv::namedWindow(CVDisparityMapWindowName, CV_WINDOW_NORMAL | CV_WINDOW_AUTOSIZE);
	cv::namedWindow(ControlsWindowName, CV_WINDOW_AUTOSIZE | CV_GUI_NORMAL);
	
	cv::createTrackbar("BlckSize/2", ControlsWindowName, &m_blockHalfsize, 20, applyParameters, this);
	cv::createTrackbar("Max Disp", ControlsWindowName, &m_numDisparities, 120, applyParameters, this);
	cv::createTrackbar("Uniq Thres", ControlsWindowName, &m_uniquenessThreshold, 50, applyParameters, this);

	const std::string leftImgPath = "..\\Image\\disparity\\" + m_image + "_left.png";
	const std::string rightImgPath = "..\\Image\\disparity\\" + m_image + "_right.png";

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

	ref_DisparityMap(
		&leftVXImage, &rightVXImage, &disparityVXImage, 
		pThis->m_blockHalfsize * 2 + 1, (int16_t)pThis->m_numDisparities, (uint32_t)pThis->m_uniquenessThreshold);

	const cv::Mat disparityMap16Bits = cv::Mat(size, CV_16SC1, disparityImageData);
	cv::Mat       disparityMap8Bits;

	double minVal, maxVal;
	minMaxLoc(disparityMap16Bits, &minVal, &maxVal);
	
	if (maxVal - minVal != 0)
	{
		disparityMap16Bits.convertTo(disparityMap8Bits, CV_8UC1, 255 / (maxVal - minVal));
		cv::imshow(DisparityMapWindowName, disparityMap8Bits);
	}
	
	free(disparityImageData);

	///@}

	int numDisparities = (pThis->m_numDisparities / 16) * 16;
	int sadWindowSize = pThis->m_blockHalfsize * 2 + 1;

	if (sadWindowSize < 5)
		sadWindowSize = 5;

	cv::Ptr<cv::StereoBM> sbm = cv::StereoBM::create(numDisparities, sadWindowSize);
	sbm->setUniquenessRatio(pThis->m_uniquenessThreshold);
	sbm->setMinDisparity(0);
	sbm->setPreFilterSize(5);
	sbm->setPreFilterCap(31);
	
	cv::Mat cvDisparityMap16Bits = cv::Mat(pThis->m_leftImage.rows, pThis->m_rightImage.cols, CV_16S);
	sbm->compute(pThis->m_leftImage, pThis->m_rightImage, cvDisparityMap16Bits);

	minMaxLoc(cvDisparityMap16Bits, &minVal, &maxVal);

	if (maxVal - minVal != 0)
	{
		cvDisparityMap16Bits.convertTo(disparityMap8Bits, CV_8UC1, 255 / (maxVal - minVal));
		cv::imshow(CVDisparityMapWindowName, disparityMap8Bits);
	}
}

///////////////////////////////////////////////////////////////////////////////
IDemoCasePtr CreateDisparityMapDemo(const std::string image)
{
	return std::make_unique<demo_DisparityMap>(image);
}
