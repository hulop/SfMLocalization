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

#include <vector>
#include <opencv2/opencv.hpp>

using namespace std;

namespace hulo {

static const float DENSE_LOCAL_FEATURE_INIT_FEATURE_SCALE = 4.0;
static const int DENSE_LOCAL_FEATURE_SCALE_LEVELS = 4;
static const float DENSE_LOCAL_FEATURE_SCALE_MULTIPLE = 1.5;
static const int DENSE_LOCAL_FEATURE_INIT_XY_STEP = 6;
static const int DENSE_LOCAL_FEATURE_INIT_IMAGE_BOUND = 0;
static const bool DENSE_LOCAL_FEATURE_VARY_XY_STEP_WITH_SCALE = false;
static const bool DENSE_LOCAL_FEATURE_VARY_IMAGE_BOUND_WITH_SCALE = false;

class DenseLocalFeatureWrapper
{
public:
	enum LocalFeatureType { KAZE = 0, AKAZE = 1, ORB = 2};

	DenseLocalFeatureWrapper(LocalFeatureType localFeatureType, int resizedImageSize, bool useColorFeature, bool useL1SquareRootFeature);
	~DenseLocalFeatureWrapper();

	int descriptorSize();
	cv::Mat calcDenseLocalFeature(string image, std::vector<cv::KeyPoint>& keypoints);

private:
	LocalFeatureType mLocalFeatureType;
	int mResizedImageSize;
	bool mUseColorFeature;
	bool mUseL1SquareRootFeature;

	cv::Ptr<cv::Feature2D> extractor;
	cv::Ptr<cv::Feature2D> detector;
};

}
