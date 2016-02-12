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

#include "DenseLocalFeatureWrapper.h"
#include "DenseFeatureDetector.h"

using namespace hulo;

DenseLocalFeatureWrapper::DenseLocalFeatureWrapper(
		LocalFeatureType localFeatureType, int resizedImageSize,
		bool useColorFeature, bool useL1SquareRootFeature) {
	mLocalFeatureType = localFeatureType;
	mResizedImageSize = resizedImageSize;
	mUseColorFeature = useColorFeature;
	mUseL1SquareRootFeature = useL1SquareRootFeature;

	switch (localFeatureType) {
	case KAZE: {
		extractor = cv::KAZE::create();
		break;
	}
	case AKAZE: {
		extractor = cv::AKAZE::create();
		break;
	}
	case ORB: {
		extractor = cv::ORB::create();
		break;
	}
	default: {
		CV_Error(CV_StsBadArg, "invalid local feature type");
		break;
	}
	}

	cv::DenseFeatureDetector::Param detectorParam;
	detectorParam.xStep = DENSE_LOCAL_FEATURE_INIT_XY_STEP;
	detectorParam.yStep = DENSE_LOCAL_FEATURE_INIT_XY_STEP;
	detectorParam.imgBoundX = DENSE_LOCAL_FEATURE_INIT_IMAGE_BOUND;
	detectorParam.imgBoundY = DENSE_LOCAL_FEATURE_INIT_IMAGE_BOUND;
	detectorParam.initFeatureScale = DENSE_LOCAL_FEATURE_INIT_FEATURE_SCALE;
	detectorParam.scaleLevels = DENSE_LOCAL_FEATURE_SCALE_LEVELS;
	detectorParam.featureScaleMul = DENSE_LOCAL_FEATURE_SCALE_MULTIPLE;
	detectorParam.varyXyStepWithScale = DENSE_LOCAL_FEATURE_VARY_XY_STEP_WITH_SCALE;
	detectorParam.varyImgBoundWithScale = DENSE_LOCAL_FEATURE_VARY_IMAGE_BOUND_WITH_SCALE;
	detector = new cv::DenseFeatureDetector(detectorParam);
}

DenseLocalFeatureWrapper::~DenseLocalFeatureWrapper() {
	if (extractor)
		delete extractor;
	if (detector)
		delete detector;
}

int DenseLocalFeatureWrapper::descriptorSize() {
	if (mUseColorFeature) {
		return extractor->descriptorSize() * 3;
	} else {
		return extractor->descriptorSize();
	}
}

cv::Mat DenseLocalFeatureWrapper::calcDenseLocalFeature(const string& image,
		std::vector<cv::KeyPoint>& keypoints) {
	cv::Mat colorImage = cv::imread(image, cv::IMREAD_COLOR);
	return calcDenseLocalFeature(colorImage, keypoints);
}

cv::Mat DenseLocalFeatureWrapper::calcDenseLocalFeature(const cv::Mat& image,
		std::vector<cv::KeyPoint>& keypoints) {
	// resize all images to fixed size
	cv::Mat colorImage = image.clone();
	cv::resize(colorImage, colorImage,
			cv::Size(mResizedImageSize, mResizedImageSize), cv::INTER_CUBIC);

	cv::Mat grayImage;
	cv::cvtColor(colorImage, grayImage, CV_BGR2GRAY);
	cv::normalize(grayImage, grayImage, 0, 255, cv::NORM_MINMAX);

	detector->detect(grayImage, keypoints);

	// calculate descriptors
	cv::Mat descriptors;
	if (mUseColorFeature) {
		// calculate descriptors for HSV channels
		/*
		 cv::Mat hsvImage;
		 cv::cvtColor(colorImage, hsvImage, CV_BGR2HSV_FULL);
		 vector<cv::Mat> colorSplit;
		 split(hsvImage, colorSplit);
		 */
		// calculate descriptors for RGB channels
		vector<cv::Mat> colorSplit;
		split(colorImage, colorSplit);

		cv::Mat descriptorsC1, descriptorsC2, descriptorsC3;
		extractor->compute(colorSplit[0], keypoints, descriptorsC1);
		extractor->compute(colorSplit[1], keypoints, descriptorsC2);
		extractor->compute(colorSplit[2], keypoints, descriptorsC3);

		descriptors = cv::Mat(keypoints.size(),
				descriptorsC1.cols + descriptorsC2.cols + descriptorsC3.cols,
				descriptorsC1.type());
		for (int r = 0; r < keypoints.size(); r++) {
			for (int c = 0; c < descriptorsC1.cols; c++) {
				descriptors.data[r
						* (descriptorsC1.cols + descriptorsC2.cols
								+ descriptorsC3.cols) + c] =
						descriptorsC1.data[r * descriptorsC1.cols + c];
			}
			for (int c = 0; c < descriptorsC2.cols; c++) {
				descriptors.data[r
						* (descriptorsC1.cols + descriptorsC2.cols
								+ descriptorsC3.cols) + descriptorsC1.cols + c] =
						descriptorsC2.data[r * descriptorsC2.cols + c];
			}
			for (int c = 0; c < descriptorsC3.cols; c++) {
				descriptors.data[r
						* (descriptorsC1.cols + descriptorsC2.cols
								+ descriptorsC3.cols) + descriptorsC1.cols
						+ descriptorsC2.cols + c] = descriptorsC3.data[r
						* descriptorsC3.cols + c];
			}
		}
	} else {
		extractor->compute(grayImage, keypoints, descriptors);
	}

	cv::Mat results;
	if (descriptors.type() == CV_32FC1) {
		results = descriptors.clone();
	} else {
		descriptors.convertTo(results, CV_32FC1);
	}

	if (mUseL1SquareRootFeature) {
		for (int i = 0; i < results.rows; i++) {
			double norm1 = 0.0;
			for (int j = 0; j < results.cols; j++) {
				norm1 += results.at<float>(i, j);
			}
			if (norm1 > 0) {
				for (int j = 0; j < results.cols; j++) {
					results.at<float>(i, j) = sqrt(
							results.at<float>(i, j) / norm1);
				}
			}
		}
	}

	// show keypoints on image for debug
	/*
	 cout << "Local descriptors size : " << descriptors.rows << " X " << descriptors.cols << endl;
	 std::vector<cv::KeyPoint>::iterator itk;
	 for (itk = keypoints.begin(); itk != keypoints.end(); ++itk) {
	 cv::circle(colorImage, itk->pt, itk->size, cv::Scalar(0,255,255), 1, CV_AA);
	 cv::circle(colorImage, itk->pt, 1, cv::Scalar(0,255,0), -1);
	 }
	 cv::imshow("debug keypoints image", colorImage);
	 cv::waitKey(0);
	 */
	return results;
}
