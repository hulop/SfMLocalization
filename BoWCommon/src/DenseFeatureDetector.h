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

#pragma once

#include <opencv2/opencv.hpp>

namespace cv {

class DenseFeatureDetector: public cv::FeatureDetector {
public:
	struct Param {
		int xStep;
		int yStep;

		int imgBoundX;
		int imgBoundY;

		float initFeatureScale;
		int scaleLevels;
		float featureScaleMul;
		bool varyXyStepWithScale;
		bool varyImgBoundWithScale;

		Param() {
			xStep = 4;
			yStep = 4;

			imgBoundX = 0;
			imgBoundY = 0;

			initFeatureScale = 1.0f;
			scaleLevels = 1;
			featureScaleMul = 1.2f;
			varyXyStepWithScale = true;
			varyImgBoundWithScale = false;
		}
	};
	Param param;

	DenseFeatureDetector() {
	}
	DenseFeatureDetector(const Param& p) :
			param(p) {
	}

	CV_WRAP
	void detect(InputArray image, CV_OUT std::vector<KeyPoint>& keypoints,
			InputArray mask = noArray());

	CV_WRAP
	static cv::Ptr<cv::FeatureDetector> create(const Param& p = Param()) {
		return cv::Ptr<cv::FeatureDetector>(new DenseFeatureDetector(p));
	}
};

}
