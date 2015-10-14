/*******************************************************************************
* Copyright (c) 2015 IBM Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*******************************************************************************/

#include "DenseFeatureDetector.h"

void cv::DenseFeatureDetector::detect(cv::InputArray image,
		std::vector<cv::KeyPoint>& keypoints, cv::InputArray mask) {
	cv::Mat maskMat =
			mask.empty() ?
					cv::Mat(image.size(), CV_8UC1, cv::Scalar(255)) :
					mask.getMat();
	cv::Mat imageMat = image.getMat();

	int width = imageMat.cols;
	int height = imageMat.rows;

	int boundX = param.imgBoundX;
	int boundY = param.imgBoundY;

	int xStep = param.xStep;
	int yStep = param.yStep;

	float featureSize = param.initFeatureScale;

	for (int s = 0; s < param.scaleLevels; s++) {
		for (int y = boundY; y < height - boundY; y += yStep) {
			unsigned char* maskline = maskMat.ptr<unsigned char>(y);

			for (int x = boundX; x < width - boundX; x += xStep) {
				if (maskline[x] > 128) {
					cv::KeyPoint kp;
					kp.pt = cv::Point(x, y);
					kp.size = featureSize;
					kp.response = 100.0f;
					kp.class_id = s;
					keypoints.push_back(kp);
				}
			}
		}

		if (param.varyImgBoundWithScale) {
			boundX *= param.featureScaleMul;
			boundY *= param.featureScaleMul;
		}
		if (param.varyXyStepWithScale) {
			xStep *= param.featureScaleMul;
			yStep *= param.featureScaleMul;
		}
		featureSize *= param.featureScaleMul;
	}
}
