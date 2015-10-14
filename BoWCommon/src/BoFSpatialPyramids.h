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
#include <omp.h>

using namespace std;

namespace hulo {

class BoFSpatialPyramids {
public:
	enum NormBofFeatureType {
		UNKNOWN=-1, NONE = 0, L2_NORM = 1, L1_NORM_SQUARE_ROOT = 2
	};

	BoFSpatialPyramids();
	BoFSpatialPyramids(const cv::Mat &kmeansTrainFeatures,
			int _K, int _RESIZED_IMAGE_SIZE,
			NormBofFeatureType _NORM_BOF_FEATURE_TYPE,
			bool _USE_SPATIAL_PYRAMID, int _PYRAMID_LEVEL);

	void write(cv::FileStorage& fs) const;
	void read(const cv::FileNode& node);

	cv::Mat calcBoF(const cv::Mat &descriptors,
			const std::vector<cv::KeyPoint> &keypoints);
	int getDim();

private:
	int K;
	int RESIZED_IMAGE_SIZE;
	NormBofFeatureType NORM_BOF_FEATURE_TYPE;
	bool USE_SPATIAL_PYRAMID;
	int PYRAMID_LEVEL;

	cv::Mat centers;
	cv::flann::KDTreeIndexParams **flannIndexParamsArray;
	cv::flann::Index **flannIndexArray;

	cv::Mat trainKMeans(const cv::Mat &features);
};

}
