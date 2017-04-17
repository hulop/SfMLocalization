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
#include <Eigen/Core>
#include <sfm/sfm_data.hpp>
#include <sfm/sfm_data_io.hpp>
#include <openMVG/version.hpp>
#include <openMVG/features/regions.hpp>
#include <openMVG/sfm/pipelines/sfm_robust_model_estimation.hpp>
#include <openMVG/sfm/pipelines/sfm_matches_provider.hpp>
#include <openMVG/sfm/pipelines/sfm_regions_provider.hpp>

#include "AKAZEOption.h"
#include "SfMDataUtils.h"
#include "DenseLocalFeatureWrapper.h"
#include "PcaWrapper.h"
#include "BoFSpatialPyramids.h"
#include "HuloSfMRegionsProvider.h"

class LocalizeEngine {
private:
	std::string mSfmDataDir;
	std::string mMatchDir;
	double mSecondTestRatio;
	int mRansacRound;
	double mRansacPrecision;
	bool mGuidedMatching;
	openMVG::sfm::SfM_Data mSfmData;
	std::shared_ptr<hulo::HuloSfMRegionsProvider> mRegionsProvider;
	hulo::MapViewFeatTo3D mMapViewFeatTo3D;
	cv::Mat mA;

	bool mUseBow;
	bool mUseBowPca;
	int mBowKnnNum;
	hulo::DenseLocalFeatureWrapper *mDenseLocalFeatureWrapper;
	hulo::PcaWrapper *mPcaWrapper;
	hulo::BoFSpatialPyramids *mBofPyramids;

	bool mUseBeacon;
	bool mBeaconKnnNum;
	std::map<std::pair<size_t, size_t>, size_t> mMajorMinorIdIndexMap;
	cv::Mat mSfmViewRSSI;

	void getLocalViews(openMVG::sfm::SfM_Data &sfm_data, const cv::Mat &A, const cv::Mat &loc,
			const double radius, std::set<openMVG::IndexT> &localViews);

	std::unique_ptr<openMVG::features::Regions> extractAKAZESingleImg(const std::string& imageID, const cv::Mat& image,
			const std::string& outputFolder, const hulo::AKAZEOption &akazeOption, std::vector<std::pair<float, float>> &locFeat);

public:
	LocalizeEngine();
	LocalizeEngine(const std::string sfmDataDir, const std::string matchDir, const std::string AmatFile,
			double secondTestRatio, int ransacRound, double ransacPrecision, bool guidedMatching,
			int beaconKnnNum=0, int bowKnnNum=0);

	std::vector<double> localize(const cv::Mat& image, const std::string workingDir, const std::string& beaconStr,
			const bool bReturnKeypoints, cv::Mat& points2D, cv::Mat& points3D, std::vector<int>& pointsInlier,
			bool bReturnTime, std::vector<double>& times,
			const std::vector<double>& center=std::vector<double>(), double radius=-1.0);
};
