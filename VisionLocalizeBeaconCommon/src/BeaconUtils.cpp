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

#include "BeaconUtils.h"

#include <fstream>
#include "StringUtils.h"

using namespace std;
using namespace cv;
using namespace openMVG;
using namespace openMVG::sfm;

//
// normalize each beacon signal so that the maximum is 100 and minimum is zero
//
void hulo::normalizeBeacon(cv::Mat &rssi, NormBeaconSignalType normBeaconType){
	CV_Assert(normBeaconType==NormBeaconSignalType::MAX || normBeaconType==NormBeaconSignalType::MEDIAN);

	if(normBeaconType==NormBeaconSignalType::MAX){
		// convert to float matrix
		cv::Mat floatRssi;
		rssi.convertTo(floatRssi, CV_32FC1);

		// find maxmimum
		cv::Mat maxVec;
		reduce(floatRssi, maxVec, 1, CV_REDUCE_MAX);
		maxVec = 1/maxVec;

		// divide each row maximum and multiply by hundred
		for (int i = 0; i < floatRssi.cols; i++) {
			floatRssi.col(i) = 100 * floatRssi.col(i).mul(maxVec);
		}

		// return result
		floatRssi.convertTo(rssi,rssi.type());
	} else if(normBeaconType==NormBeaconSignalType::MEDIAN) {
		// convert to float matrix
		cv::Mat floatRssi;
		rssi.convertTo(floatRssi, CV_32FC1);

		// find median of nonzero value
		vector<float> vec;
		for (int i = 0; i < floatRssi.cols; i++) {
			if (floatRssi.at<float>(0,i) != 0.0f) {
				vec.push_back(floatRssi.at<float>(0,i));
			}
		}
		std::nth_element(vec.begin(), vec.begin()+vec.size()/2, vec.end());
		vector<float> median;
		median.push_back(vec[vec.size()/2]);
		cv::Mat medVec(median);
		medVec = 1/medVec;

		// divide each row maximum and multiply by hundred
		for (int i = 0; i < floatRssi.cols; i++) {
			floatRssi.col(i) = 100 * floatRssi.col(i).mul(medVec);
		}

		// return result
		floatRssi.convertTo(rssi, rssi.type());
	}
}

//
// calculate difference between RSSI
// if an element is filled with zero, then leave out that element from computation
// the distance used is the mean l2 norm of co-occured non-zero elements of both vector
// at least minSameBeaconNum same beacons must be detected
// NOTE: the input is assumed to be row vector
//
float hulo::calRSSIDiff(cv::Mat vec1, cv::Mat vec2, int minSameBeaconNum){
	CV_Assert(vec1.rows==1 && vec2.rows==1 && vec1.cols==vec2.cols);

	int count = 0;
	float sum = 0.0f;

	for (int k = 0; k < vec1.cols; k++) {
		float a = static_cast<float>(vec1.at<uchar>(0,k));
		float b = static_cast<float>(vec2.at<uchar>(0,k));
		if (a >= 1.0f && b >= 1.0f) {
			count++;
			sum += abs(a-b);
		}
	}

	if (count > minSameBeaconNum) { // must detect at least minBeaconNum same beacons
		sum = sum/count; // mean l2 norm
	} else { // if no co-occuring elements, set to maximum float
		sum = numeric_limits<float>::max();
	}
	return sum;
}

//
// calculate occurrence of 2 RSSI vector, defined as
// intersect( r1( r1 > thresCooc ) , r2( r2 > thresCooc ) ) / union( r1( r1 > thresCooc ) , r2( r2 > thresCooc ) )
//
float hulo::calRSSICooc(cv::Mat vec1, cv::Mat vec2, int thresCooc){
	CV_Assert(vec1.rows==1 && vec2.rows==1 && vec1.cols==vec2.cols);

	float intMin = 0.0f; // minimum intersection
	float uniMax = 0.0f; // maximum union

	for (int k = 0; k < vec1.cols; k++) {
		int a = vec1.at<uchar>(0,k);
		int b = vec2.at<uchar>(0,k);
		if (a > thresCooc || b > thresCooc) {
			uniMax += max(a,b);
			intMin += min(a,b);
		}
	}

	return intMin/uniMax;
}

//
// parse beacon signals for all views
// each line in input file should have beacon signals for one view
//
void hulo::parseBeaconList(const string &sBeaconList, map<pair<size_t,size_t>,size_t> &beaconIndex, cv::Mat &rssi){
	ifstream beaconfile(sBeaconList);

	if (beaconfile.is_open()) {
		string line;
		getline(beaconfile,line);
		line = hulo::trim(line);

		// read beacon list and generate map
		int nBeacon = stoi(line);
		for (int i = 0; i<nBeacon; i++) {
			getline(beaconfile,line);
			size_t major = static_cast<size_t>(stoi(hulo::stringTok(line)));
			size_t minor = static_cast<size_t>(stoi(line));
			beaconIndex.insert(make_pair(make_pair(major,minor),i));
		}

		// read rssi into matrix
		getline(beaconfile,line);
		int nRssi = stoi(hulo::trim(line));
		rssi = cv::Mat::zeros(nRssi,nBeacon,CV_8UC1);
		while (getline(beaconfile,line)) {
			// read viewID
			int viewID = stoi(hulo::stringTok(line));
			CV_Assert(viewID>=0 && viewID<rssi.rows);
			
			// read each rssi entry
			for (int j = 0;j < beaconIndex.size(); j++) {
				rssi.at<uchar>(viewID,j) = static_cast<uchar>(stof(hulo::stringTok(line)));
			}
		}
	}

	beaconfile.close();
}

//
// parse a line read from file containing "Beacon" into a vector indexed by (major,minor) in beaconIndex
//
void hulo::parseBeaconString(string &line, map<pair<size_t,size_t>,size_t> &beaconIndex, cv::Mat &qRssi){
	hulo::stringTok(line); // timestamp
	hulo::stringTok(line); // "Beacon"
	hulo::stringTok(line); // skip another 4 empty spaces
	hulo::stringTok(line);
	hulo::stringTok(line);
	hulo::stringTok(line);
	int nSignal = stoi(hulo::stringTok(line)); // number of signal detected

	for (int j = 0;j < nSignal; j++) {
		size_t major = static_cast<size_t>(stoi(hulo::stringTok(line)));
		size_t minor = static_cast<size_t>(stoi(hulo::stringTok(line)));
		int strength = stoi(hulo::stringTok(line));

		if(strength == 0){
			continue;
		} else {
			strength += 100;
		}

		pair<size_t,size_t> majorMinorPair = make_pair(major,minor);
		map<pair<size_t,size_t>,size_t>::iterator rssiInd = beaconIndex.find(majorMinorPair);
		if (rssiInd != beaconIndex.end()) {
			qRssi.at<uchar>(rssiInd->second) = static_cast<uchar>(strength);
		}
	}
}

//
// read first beacon entry closest to file namefrom csv file and generate rssi vector
//
void hulo::readRssiFromCSV(const string &sQRssi, const string &filename, map<pair<size_t,size_t>,size_t> &beaconIndex, cv::Mat &qRssi){
	// allocate space
	qRssi = cv::Mat::zeros(1,beaconIndex.size(),CV_8UC1);

	size_t timePhoto = 0;
	vector<size_t> timeBeacon;
	vector<string> stringBeacon;

	// for indicating photo name is found in csv file,
	// and also to choose beacon rssi from the same file only
	bool foundPhoto = false;

	if (stlplus::is_file(sQRssi)) {
		ifstream qCSVfile(sQRssi);
		if (qCSVfile.is_open()) {
			// read until find a Beacon keyword
			string line;

			// readline and save time and string of beacon and time of photo
			while (getline(qCSVfile,line)) {
				if (line.find("Beacon")!=string::npos) {
					size_t timeTmp = static_cast<size_t>(stol(hulo::stringTok(line)));
					timeBeacon.push_back(timeTmp);
					stringBeacon.push_back(to_string(timeTmp)+","+line);
				} else {
					if (line.find(filename)!=string::npos) {
						timePhoto = static_cast<size_t>(stol(hulo::stringTok(line))); // get time
						foundPhoto = true;
					}
				}
			}
			qCSVfile.close();
		}
	} else if(stlplus::is_folder(sQRssi)) {
		// for folder
		vector<string> fileList = stlplus::folder_files(sQRssi);

		for (vector<string>::const_iterator iterFile = fileList.begin();
				iterFile != fileList.end(); ++iterFile) {
			// check if csv file or not
			if (stlplus::extension_part(*iterFile).compare("csv")) {
				continue;
			}

			// read beacon
			ifstream qCSVfile(stlplus::create_filespec(sQRssi,*iterFile));
			if (qCSVfile.is_open()) {
				// read until find a Beacon keyword
				string line;

				// readline and save time and string of beacon and time of photo
				while (getline(qCSVfile,line)) {
					if (line.find("Beacon")!=string::npos) {
						size_t timeTmp = static_cast<size_t>(stol(hulo::stringTok(line)));
						timeBeacon.push_back(timeTmp);
						stringBeacon.push_back(to_string(timeTmp)+","+line);
					} else {
						if (line.find(filename)!=string::npos) {
							timePhoto = static_cast<size_t>(stol(hulo::stringTok(line))); // get time
							foundPhoto = true;
						}
					}
				}
				qCSVfile.close();
			}

			// if found photo in this csv file, break
			if (foundPhoto) {
				break;
			} else {
				// if not, then remove the beacon data and move to next file
				timeBeacon.clear();
				stringBeacon.clear();
			}

		}
	}

	if (foundPhoto && timeBeacon.size()>0) {
		// find beacon with closest time
		int closestInd = -1;
		size_t timeDiff = numeric_limits<size_t>().max();
		for (int i = 0; i < timeBeacon.size(); i++) {
			if (abs(timeBeacon[i]-timePhoto)<timeDiff) {
				timeDiff = abs(timeBeacon[i]-timePhoto);
				closestInd = i;
			}
		}

		// if a beacon line is found, put the entries in rssi vector
		parseBeaconString(stringBeacon[closestInd], beaconIndex, qRssi);
	}
}

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
void hulo::getBeaconView(
		const string &sQRssi,
		const string &qImgName,
		map<pair<size_t,size_t>,size_t> &beaconIndex,
		cv::Mat &rssi,
		set<IndexT> &viewList,
		int thresRssiCooc,
		int minSameBeaconNum,
		int knn,
		float cooc,
		SfM_Data &sfm_data,
		int skipFrame,
		NormBeaconSignalType normBeaconType){
	// read rssi of query image
	cv::Mat qRssi;
	readRssiFromCSV(sQRssi, qImgName, beaconIndex, qRssi);
	normalizeBeacon(qRssi, normBeaconType); // normalize to [0,100]

	return getBeaconView(qRssi, beaconIndex, rssi, viewList, thresRssiCooc, minSameBeaconNum,
			knn, cooc, sfm_data, skipFrame, normBeaconType);
}

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
void hulo::getBeaconView(
		const cv::Mat& qRssi,
		map<pair<size_t,size_t>,size_t> &beaconIndex,
		cv::Mat &rssi,
		set<IndexT> &viewList,
		int thresRssiCooc,
		int minSameBeaconNum,
		int knn,
		float cooc,
		SfM_Data &sfm_data,
		int skipFrame,
		NormBeaconSignalType normBeaconType){
	// find nearest neighbor
	cv::Mat distance = cv::Mat::zeros(1, rssi.rows, CV_32F);
	int countBeacon = 0;
	for (int row=0; row<rssi.rows; row++) {
		// two conditions
		// 1. cooccurence must be above threshold
		// 2. the image must be used for reconstruction
		if (calRSSICooc(rssi.row(row), qRssi, thresRssiCooc) > cooc
				&& sfm_data.poses.find(sfm_data.views.at(row)->id_pose)!=sfm_data.poses.end()) {
			distance.at<float>(0,row) = calRSSIDiff(rssi.row(row), qRssi, minSameBeaconNum);
			countBeacon++;
		} else {
			distance.at<float>(0,row) = numeric_limits<float>().max();
		}
	}

	cv::Mat sortInd; // sort get index
	cv::sortIdx(distance,sortInd,CV_SORT_EVERY_ROW + CV_SORT_ASCENDING);
	cv::Mat sortVal; // sort get value
	cv::sort(distance,sortVal,CV_SORT_EVERY_ROW + CV_SORT_ASCENDING);

	// generate and return list
	while (skipFrame*knn > rssi.rows) {
		skipFrame--;
		if (skipFrame==0) {
			skipFrame = 1;
			break;
		}
	}
	for (int row = 0; row < min(skipFrame*knn,rssi.rows);row+=skipFrame) {
		if (sortVal.at<float>(0,row) < numeric_limits<float>().max()) {
			viewList.insert(sortInd.at<int>(0,row));
		}
	}
}
