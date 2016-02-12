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
#include <openMVG/types.hpp>
#include <openMVG/sfm/sfm_data.hpp>

namespace hulo {

enum NormBeaconSignalType {
	UNKNOWN=-1, MAX = 0, MEDIAN = 1
};

//
// normalize each beacon signal so that the maximum is 100 and minimum is zero
//
extern void normalizeBeacon(cv::Mat &rssi, NormBeaconSignalType normBeaconType=NormBeaconSignalType::MAX);

//
// calculate difference between RSSI
// if an element is filled with zero, then leave out that element from computation
// the distance used is the mean l2 norm of co-occured non-zero elements of both vector
// at least minSameBeaconNum same beacons must be detected
// NOTE: the input is assumed to be row vector!
//
extern float calRSSIDiff(cv::Mat vec1, cv::Mat vec2, int minSameBeaconNum);

//
// calculate occurrence of 2 RSSI vector, defined as
// intersect( r1( r1 > thresCooc ) , r2( r2 > thresCooc ) ) / union( r1( r1 > thresCooc ) , r2( r2 > thresCooc ) )
//
extern float calRSSICooc(cv::Mat vec1, cv::Mat vec2, int thresCooc);

//
// parse beacon signals for all views
// each line in input file should have beacon signals for one view
//
extern void parseBeaconList(const std::string &sBeaconList,
		std::map<std::pair<size_t, size_t>, size_t> &beaconIndex, cv::Mat &rssi);

//
// parse a line read from file containing "Beacon" into a vector indexed by (major,minor) in beaconIndex
//
extern void parseBeaconString(std::string &line,
		std::map<std::pair<size_t, size_t>, size_t> &beaconIndex, cv::Mat &qRssi);

//
// read first beacon entry closest to file namefrom csv file and generate rssi vector
//
extern void readRssiFromCSV(const std::string &sQRssi, const std::string &filename,
		std::map<std::pair<size_t, size_t>, size_t> &beaconIndex, cv::Mat &qRssi);

//
// compare beacon RSSI from sBeaconRSSI to RSSI for all views and return view ID of k nearest neighbor
// sQRssi: csv file containing beacon reading of input image
// qImgName: input image name
// rssi : RSSI signals for all views
// viewList: to return the list of images found nearby
// knn: number of nearest neighboe to return
// cooc: cooccurrence threshold
// sfm_data: sfm_data of openmvg
// skipBeaconView :
// normApproach: normalization approach [0 for max, 1 for median]
//
extern void getBeaconView(const std::string &sQRssi,
		const std::string &qImgName,
		std::map<std::pair<size_t, size_t>, size_t> &beaconIndex,
		cv::Mat &rssi,
		std::set<openMVG::IndexT> &viewList,
		int thresRssiCooc,
		int minSameBeaconNum,
		int knn,
		float cooc,
		openMVG::sfm::SfM_Data &sfm_data,
		int skipBeaconView,
		NormBeaconSignalType normBeaconType=NormBeaconSignalType::MAX);

//
// compare beacon RSSI from sBeaconRSSI to RSSI for all views and return view ID of k nearest neighbor
// qRssi: RSSI signals for query image
// rssi : RSSI signals for all views
// viewList: to return the list of images found nearby
// knn: number of nearest neighboe to return
// cooc: cooccurrence threshold
// sfm_data: sfm_data of openmvg
// skipBeaconView :
// normApproach: normalization approach [0 for max, 1 for median]
//
extern void getBeaconView(const cv::Mat& qRssi,
		std::map<std::pair<size_t, size_t>, size_t> &beaconIndex,
		cv::Mat &rssi,
		std::set<openMVG::IndexT> &viewList,
		int thresRssiCooc,
		int minSameBeaconNum,
		int knn,
		float cooc,
		openMVG::sfm::SfM_Data &sfm_data,
		int skipBeaconView,
		NormBeaconSignalType normBeaconType=NormBeaconSignalType::MAX);

}
