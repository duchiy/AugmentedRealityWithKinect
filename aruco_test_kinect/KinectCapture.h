#include "stdafx.h"
#include <iostream>
#include <Windows.h>
//#include <opencv2\imgproc\imgproc.hpp>
//#include <opencv2\core\core.hpp>
#include "opencv2/core.hpp"
#include "opencv2/imgproc.hpp"
#include "opencv2/highgui.hpp"
//#include "opencv2/videoio.hpp"
//#include <iostream>

#include <NuiApi.h>

using namespace std;
using namespace cv;

class KinectCapture
{
private:
	static const int WIDTH = 640;
	static const int HEIGHT = 480;
	static const int BYTES_PER_PIXEL = 4;

	INuiSensor*		mNuiSensor;
	HANDLE			mNextDepthFrameEvent;
	HANDLE			mNextColorFrameEvent;
	HANDLE			mDepthStreamHandle;
	HANDLE			mColorStreamHandle;
	// -- this is the grey scale depth image
	Mat				mCVImageDepth;
	// -- this is the color image
	Mat				mCVImageColor;
	// -- this is the color image mapped to the depth image
	Mat				mCVImageColorDepth;
	// -- color pixel to depth pixel mapping
 	RGBQUAD			*pmRGB;
	bool            _isOpened;
public:
	bool init() {
		int sensorCount = 0;
		
		HRESULT hr = NuiGetSensorCount(&sensorCount);
		if(FAILED(hr)) return false;

		hr = NuiCreateSensorByIndex(0, &mNuiSensor);
		if(FAILED(hr)) return false;

		hr = mNuiSensor->NuiStatus();
		if(hr != S_OK) {
			mNuiSensor->Release();
			return false;
		}

		if(mNuiSensor == NULL) return false;
		
		hr = mNuiSensor->NuiInitialize(NUI_INITIALIZE_FLAG_USES_DEPTH | NUI_INITIALIZE_FLAG_USES_COLOR);
		if(!SUCCEEDED(hr)) return false;

		mNextDepthFrameEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
		mNextColorFrameEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

		hr = mNuiSensor->NuiImageStreamOpen(NUI_IMAGE_TYPE_DEPTH, NUI_IMAGE_RESOLUTION_640x480, 0, 2, mNextDepthFrameEvent, &mDepthStreamHandle);
		hr = mNuiSensor->NuiImageStreamOpen(NUI_IMAGE_TYPE_COLOR, NUI_IMAGE_RESOLUTION_640x480, 0, 2, mNextColorFrameEvent, &mColorStreamHandle);

		mNuiSensor->NuiImageStreamSetImageFrameFlags(mDepthStreamHandle, NUI_IMAGE_STREAM_FRAME_LIMIT_MAXIMUM);

		if(mNuiSensor == NULL || FAILED(hr))
			return false;

		mCVImageDepth = Mat(Size(WIDTH, HEIGHT), CV_8UC1);
		mCVImageColorDepth = Mat(Size(WIDTH, HEIGHT), CV_8UC3);
		mCVImageColor = Mat(Size(WIDTH, HEIGHT), CV_8UC3);

		return true;
	}
	bool open(int index)
	{
		init();
		_isOpened=true;
		return true;
	}
	bool open(const String& filename)
	{
		return true;

	}
	bool isOpened()
	{

		return _isOpened;
	}
	KinectCapture()
	{
		pmRGB=new RGBQUAD[HEIGHT*WIDTH];
		_isOpened=false;
	}

	Mat& getDepthMat()
	{
		return mCVImageDepth;
	}

	Mat& getColorDepthMat()
	{
		return mCVImageColorDepth;
	}

	Mat& getColorMat()
	{
		return mCVImageColor;
	}

	bool retrieve(OutputArray image, int flag = 0)
	{
			cv::Mat image1 =getColorMat();
			cv::Mat image2;
			flip(image1, image2, 1);
			OutputArray m(image2);
			m.copyTo(image);
			return true;
	};
	void processDepth() {
		HRESULT hr;
		NUI_IMAGE_FRAME depthFrame, colorFrame;

		hr = mNuiSensor->NuiImageStreamGetNextFrame(mDepthStreamHandle, 5, &depthFrame);
		if(FAILED(hr)) return;
		hr = mNuiSensor->NuiImageStreamGetNextFrame(mColorStreamHandle, 10, &colorFrame);
		if(FAILED(hr)) return;

		INuiFrameTexture* depthTex = depthFrame.pFrameTexture;
		INuiFrameTexture* colorTex = colorFrame.pFrameTexture;
		NUI_LOCKED_RECT lockedRectDepth;
		NUI_LOCKED_RECT lockedRectColor;

		depthTex->LockRect(0, &lockedRectDepth, NULL, 0);
		colorTex->LockRect(0, &lockedRectColor, NULL, 0);

		if(lockedRectDepth.Pitch != 0 && lockedRectColor.Pitch != 0) {
			const USHORT *depthBufferRun = (const USHORT*)lockedRectDepth.pBits;
			const USHORT *depthBufferEnd = (const USHORT*)depthBufferRun + (WIDTH * HEIGHT);
			const BYTE *colorBufferRun = (const BYTE*)lockedRectColor.pBits;
			const BYTE *colorBufferEnd = (const BYTE*)colorBufferRun + (WIDTH * HEIGHT * 4);

			memcpy(pmRGB, colorBufferRun, WIDTH * HEIGHT * sizeof(RGBQUAD));

			int count = 0;
			int x, y;
			const float DEPTH_THRESH =200.0;
			while(depthBufferRun < depthBufferEnd) 
			{
				USHORT depth = NuiDepthPixelToDepth(*depthBufferRun);
				
				BYTE intensity = 256 - static_cast<BYTE>(((float)depth / DEPTH_THRESH) * 256.);

				x = count % WIDTH;
				y = floor((float)count / (float)WIDTH);
				
				mCVImageDepth.at<uchar>(y, x) = intensity;

				LONG colorInDepthX;
				LONG colorInDepthY;

				mNuiSensor->NuiImageGetColorPixelCoordinatesFromDepthPixel(NUI_IMAGE_RESOLUTION_640x480, NULL, x/2, y/2, *depthBufferRun, &colorInDepthX, &colorInDepthY);

				if(colorInDepthX >=0 && colorInDepthX < WIDTH && colorInDepthY >=0 && colorInDepthY < HEIGHT)
				{
					RGBQUAD &color = pmRGB[colorInDepthX + colorInDepthY * WIDTH];
					LONG colorIndex = colorInDepthX + colorInDepthY * WIDTH * 3;
					mCVImageColorDepth.at<Vec3b>(y, x) = Vec3b(color.rgbBlue, color.rgbGreen, color.rgbRed);
				} else {
					mCVImageColorDepth.at<Vec3b>(y, x) = Vec3b(0, 0, 0);
				}

				RGBQUAD &color = pmRGB[count];
				mCVImageColor.at<Vec3b>(y, x) = Vec3b(color.rgbBlue, color.rgbGreen, color.rgbRed);

				count++;
				depthBufferRun++;
			}
		}

		depthTex->UnlockRect(0);
		colorTex->UnlockRect(0);
		mNuiSensor->NuiImageStreamReleaseFrame(mDepthStreamHandle, &depthFrame);
		mNuiSensor->NuiImageStreamReleaseFrame(mColorStreamHandle, &colorFrame);
	}
	bool grab()
	{

		processDepth();
		return true;

	}
	~KinectCapture()
	{
		if(mNuiSensor)
			mNuiSensor->NuiShutdown();

		delete [] pmRGB;
	}
};