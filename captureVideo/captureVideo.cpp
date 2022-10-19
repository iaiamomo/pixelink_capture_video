// captureVideo.cpp : Questo file contiene la funzione 'main', in cui inizia e termina l'esecuzione del programma.
//

#include "stdafx.h"

#include <PixeLINKApi.h>
#include <PixeLINKCodes.h>
#include <PixeLINKTypes.h>

#include <assert.h>
#include <conio.h>
#include <iostream>
#include <vector>
#include <cstdio>
#include <cassert>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <cstring>

#include <sys/types.h>
#include <WinSock2.h>
#include <winsock.h>
#include <ws2tcpip.h>
#include <Windows.h>
#include <WinBase.h>
#include <winnt.h>
#include <sstream>
#include <fstream>

#include <time.h>

#pragma comment(lib, "Ws2_32.lib")

#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/videoio.hpp>

using namespace cv;

#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"
#define FRAME_WIDTH 2048
#define FRAME_HEIGHT 2048
int height, width, IM_HEIGHT, IM_WIDTH, imgSize;
SOCKET server, client;

static volatile int    s_getClipFinished = 0; // will be set to 1 by the callback function

static PXL_RETURN_CODE s_getClipReturnCode = ApiUnknownError;


typedef enum _PIXEL_TYPE
{
	PT_COLOR,
	PT_MONO,
	PT_OTHERWISE // Special cameras, like polarized and interleaved HDR
} PIXEL_TYPE;


U32 __stdcall
TerminationFunction(HANDLE hCamera, U32 numberOfFramesCaptured, PXL_RETURN_CODE retCode)
{

	printf("PxLGetClip has reported termination: ");

	switch (retCode)

	{

	case ApiSuccess:

		printf("Success.\n");

		break;


	case ApiDiskFullError:

		printf("Error - problems writing to disk (disk full?).\n");

		break;


	case ApiIOError:

		printf("Error - problems writing to disk.\n");

		break;

	default:

		printf("Error 0x%8.8X\n", retCode);

		break;

	}

	s_getClipReturnCode = retCode;

	s_getClipFinished = 1;

	return 0; // Termination function finished.

}

// IMPORTANT NOTE:
//    This function will only return a meaningful value, if called while NOT streaming
PIXEL_TYPE getPixelType(HANDLE hCamera)
{
	PIXEL_TYPE pixelType = PT_OTHERWISE;
	// Take a simple minded approach; All Pixelink monochrome cameras support PIXEL_FORMAT_MONO8, and all
	// Pixelink color cameas support PIXEL_FORMAT_BAYER8.  So, try setting each of these to see if it 
	// works.

	//  However, we should take care to restore the current pixel format.
	U32 flags = 0;
	U32 numParams = 1;
	float savedPixelFormat;
	float newPixelFormat;
	PXL_RETURN_CODE rc = PxLGetFeature(hCamera, FEATURE_PIXEL_FORMAT, &flags, &numParams, &savedPixelFormat);
	if (!API_SUCCESS(rc)) return pixelType;

	// Is it mono??
	newPixelFormat = PIXEL_FORMAT_MONO8;
	rc = PxLSetFeature(hCamera, FEATURE_PIXEL_FORMAT, FEATURE_FLAG_MANUAL, 1, &newPixelFormat);
	if (!API_SUCCESS(rc)) {
		// Nope, so is it color?
		newPixelFormat = PIXEL_FORMAT_BAYER8;
		rc = PxLSetFeature(hCamera, FEATURE_PIXEL_FORMAT, FEATURE_FLAG_MANUAL, 1, &newPixelFormat);
		if (API_SUCCESS(rc)) {
			// Yes, it IS color
			pixelType = PT_COLOR;
		}
	}
	else {
		// Yes, it IS mono
		pixelType = PT_MONO;
	}

	// Restore the saved pixel format
	PxLSetFeature(hCamera, FEATURE_PIXEL_FORMAT, FEATURE_FLAG_MANUAL, 1, &savedPixelFormat);

	return pixelType;
}

U32 __stdcall MyPreviewCallback(HANDLE           hCamera,
	void* pFrameData,
	U32                dataFormat,
	FRAME_DESC const* pFrameDesc,
	void* pContext)
{

	// Process the frame here. 
	// NOTE: For preview callbacks, the frame data is writable.  
	// NOTE: The frame descriptor provided is const.
	printf("i\n");
	return ApiSuccess;

}

int main()
{
	// Step 1
	//      Prepare the camera
	// Inizializzazione PixeLink Camera 
	HANDLE hCamera;
	PXL_RETURN_CODE rc = PxLInitialize(0, &hCamera);
	if (!API_SUCCESS(rc)) {
		printf("Error: Unable to initialize a camera\n");
		return EXIT_FAILURE;
	}

	//
   // Step 2
   //      Figure out if this is a mono or color camera, so that we know the type of
   //      image we will be working with. 
	PIXEL_TYPE pixelType = getPixelType(hCamera);
	if (pixelType == PT_OTHERWISE) {
		printf("Error: We can't deal with this type of camera\n");
		PxLUninitialize(hCamera);
		return EXIT_FAILURE;
	}


	U32 flags = 0;
	U32 numParams = 1;
	float* params;
	rc = PxLGetFeature(hCamera, FEATURE_FRAME_RATE, &flags, &numParams, NULL);

	params = (float*)malloc(sizeof(float) * numParams);

	// Read the current flags and parameter setting
	rc = PxLGetFeature(hCamera, FEATURE_FRAME_RATE, &flags, &numParams, params);
	printf("Frame rate %f\n", params[0]);

	// Just going to declare a very large buffer here
	// One that's large enough for any PixeLINK 4.0 camera
	std::vector<U8> frameBuffer(4000 * 4000 * 2);
	FRAME_DESC frameDesc;
	frameDesc.uSize = sizeof(FRAME_DESC); // Let the Pixelink API know what version we were compiled against
   // color has 3 channels, mono just 1
	U32 imageBufferSize = frameBuffer.size() * (pixelType == PT_COLOR ? 3 : 1);
	std::vector<U8> imageBuffer(imageBufferSize);

	//
	// Step 3
	//      Start the stream and Grab an image from the camera. 
	rc = PxLSetStreamState(hCamera, START_STREAM);
	if (!API_SUCCESS(rc)) {
		printf("Error: Unable to start the stream on the camera\n");
		PxLUninitialize(hCamera);
		return EXIT_FAILURE;
	}

	rc = PxLGetNextFrame(hCamera, (U32)frameBuffer.size(), &frameBuffer[0], &frameDesc);
	Mat  openCVImage((int)(frameDesc.Roi.fHeight / frameDesc.PixelAddressingValue.fVertical),
		(int)(frameDesc.Roi.fWidth / frameDesc.PixelAddressingValue.fHorizontal),
		pixelType == PT_MONO ? CV_8UC1 : CV_8UC3,
		&imageBuffer[0]);

	VideoWriter outputVideo;
	Size s = Size((int)(frameDesc.Roi.fHeight / frameDesc.PixelAddressingValue.fVertical),
		(int)(frameDesc.Roi.fWidth / frameDesc.PixelAddressingValue.fHorizontal));


	time_t now = time(0);
	struct tm* tstruct = localtime(&now);
	char       outputFileName[MAX_PATH];
	strftime(outputFileName, MAX_PATH, "%Y-%m-%d_%H-%M-%S.mpeg", tstruct);

	outputVideo.open(outputFileName, -1, params[0], s); //"prova.mpeg"

	bool LoopExecution = true;
	int count1 = 0;

	namedWindow("control window");

	while (LoopExecution) {
		count1++;

		rc = PxLGetNextFrame(hCamera, (U32)frameBuffer.size(), &frameBuffer[0], &frameDesc);
		if (count1 % 100 == 0) {
			printf("i");
		}

		if (count1 >= 1200) {
			printf("Terminate process");
			break;
		}

		if (true) {
			if (API_SUCCESS(rc)) {
				// Convert it to an image buffer.  Note that frame can be in any one of a large number of pixel
				// formats, so we will simplify things by converting all mono to mono8, and all color to rgb24
				if (pixelType == PT_MONO) {
					rc = PxLFormatImage(&frameBuffer[0], &frameDesc, IMAGE_FORMAT_RAW_MONO8, &imageBuffer[0], &imageBufferSize);
				}
				else {
					rc = PxLFormatImage(&frameBuffer[0], &frameDesc, IMAGE_FORMAT_RAW_BGR24, &imageBuffer[0], &imageBufferSize);
				}
				if (API_SUCCESS(rc)) {

					//
					// Step 4
					//      'convert' the image to one that openCV can manipulate

					if (waitKey(1) == 27) {
						break;
					}
					outputVideo.write(openCVImage);
					if (false) {


						if (openCVImage.data)
						{
							int imgSize = openCVImage.total() * openCVImage.elemSize();
							std::vector<uchar> buff;//buffer for coding
							std::vector<int> param(2);
							param[0] = cv::IMWRITE_JPEG_QUALITY;
							param[1] = 100;
							cv::imencode(".jpg", openCVImage, buff, param);
						}
						else {
							printf("Error: Could not format the image for openCV\n");
						}
					}
				}
			}
			else {
				printf("Error: Could not grab an image from the camera\n");
			}
		}
	}


	//
	// Step 6
	//      Cleanup
	outputVideo.release();
	rc = PxLSetStreamState(hCamera, STOP_STREAM);
	ASSERT(API_SUCCESS(rc));
	rc = PxLUninitialize(hCamera);
	ASSERT(API_SUCCESS(rc));

	return EXIT_SUCCESS;
}
