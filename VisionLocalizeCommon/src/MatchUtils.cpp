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

#include "MatchUtils.h"

#include "AKAZEOpenCV.h"
#include "MatUtils.h"
#include "FileUtils.h"
#include "SfMDataUtils.h"

#include <opencv2/opencv.hpp>
#include <openMVG/matching_image_collection/GeometricFilter.hpp>
#include <openMVG/matching_image_collection/F_ACRobust.hpp>

using namespace std;
using namespace cv;
using namespace openMVG;
using namespace openMVG::features;
using namespace openMVG::matching;
using namespace openMVG::matching_image_collection;
using namespace openMVG::sfm;

namespace {

// settings using OpenCV recommended values, this settings may improve accuracy
/*
static const int LSH_TABLE_NUMBER = 12; // OpenCV manual says usually between 10 and 30
static const int LSH_KEY_SIZE = 20; // OpenCV manual says usually between 10 and 20
static const int LSH_MULTI_PROBE_LEVEL = 2;  // OpenCV manual says usually 0 is regular LSH, and 2 is recommended
*/
// settings which may decrease accuracy, but much faster for small number of CPUs
static const int LSH_TABLE_NUMBER = 2;
static const int LSH_KEY_SIZE = 20;
static const int LSH_MULTI_PROBE_LEVEL = 2;

// same settings to OpenCV default parameters, this settings may improve accuracy
/*
static const int FLANN_SEARCH_PARAMS_CHECKS = 32;
static const float FLANN_SEARCH_PARAMS_EPS = 0;
static const bool FLANN_SEARCH_PARAMS_SORTED = true;
*/
// settings which may decreacausese accuracy, but much faster for small number of CPUs
static const int FLANN_SEARCH_PARAMS_CHECKS = 2;
static const float FLANN_SEARCH_PARAMS_EPS = 0;
static const bool FLANN_SEARCH_PARAMS_SORTED = true;

}

// match pairs from a list
// filenames : list of filenames of descriptor file
// pairs : list of pairs to match
// matches : list of matches for each pair
void hulo::matchAKAZE(const SfM_Data &sfm_data, const string &sMatchesDir,
		const vector<pair<size_t, size_t>> &pairs, const float fDistRatio,
		PairWiseMatches &matches) {

	cout << "Start putative matching" << endl;

	// iterate through each pair
#pragma omp parallel for
	for (int runIter = 0; runIter < pairs.size(); runIter++) {
		vector<pair<size_t, size_t>>::const_iterator iter = pairs.begin();
		advance(iter, runIter);

		// read files
		string filename1 = stlplus::create_filespec(sMatchesDir,
				stlplus::basename_part(
						sfm_data.views.at(iter->first)->s_Img_path), "desc");
		string filename2 = stlplus::create_filespec(sMatchesDir,
				stlplus::basename_part(
						sfm_data.views.at(iter->second)->s_Img_path), "desc");
		cv::Mat desc1;
		cv::Mat desc2;
		hulo::readAKAZEBin(filename1, desc1);
		hulo::readAKAZEBin(filename2, desc2);
		// cout << iter->first << " " << iter->second << endl;

		// perform matching
		if (desc1.rows < 2 || desc2.rows < 2) {
			continue;
		}

		cv::Mat matchesMat;
		cv::Mat distMat;
		cv::flann::Index matchers(desc2, cv::flann::LshIndexParams(LSH_TABLE_NUMBER, LSH_KEY_SIZE, LSH_MULTI_PROBE_LEVEL),
				cvflann::flann_distance_t::FLANN_DIST_HAMMING);
		matchers.knnSearch(desc1, matchesMat, distMat, 2,
				cv::flann::SearchParams(FLANN_SEARCH_PARAMS_CHECKS, FLANN_SEARCH_PARAMS_EPS, FLANN_SEARCH_PARAMS_SORTED));

		// ratio test
		vector<size_t> matchesInd(desc1.rows);
		for (int i = 0; i < desc1.rows; i++) {
			if ((0.0f + distMat.at<int>(i, 0)) / distMat.at<int>(i, 1)
					< fDistRatio) {
				if (distMat.at<int>(i, 1) < numeric_limits<int>::max()) {
					matchesInd[i] = matchesMat.at<int>(i, 0);
				}
			} else {
				matchesInd[i] = -1;
			}
		}

		// check if matches is one to one
		// i.e. no two or more features from query matches to a single training features
		for (int i = 0; i < matchesInd.size() - 1; i++) {

			if (matchesInd[i] == -1) {
				continue;
			}

			bool check1to1 = false;

			for (int j = i + 1; j < matchesInd.size(); j++) {
				if (matchesInd[i] == matchesInd[j]) {
					matchesInd[j] = -1;
					check1to1 = true;
				}
			}

			if (check1to1) {
				matchesInd[i] = -1;
			}
		}

		// add the remaining matches
		for (int i = 0; i < matchesInd.size() - 1; i++) {
			if (matchesInd[i] != -1) {
				matches[*iter].push_back(IndMatch(i, matchesInd[i]));
			}
		}
	}
}

// perform tracking on AKAZE and return as match
// maxFrameDist : maximum distance between frame to write as match (though track length can be longer)
void hulo::trackAKAZE(const SfM_Data &sfm_data, const string &sMatchesDir,
		const size_t maxFrameDist, const float fDistRatio, PairWiseMatches &matches) {

	cout << "Start putative matching" << endl;

	cv::Mat featNumber = cv::Mat::zeros(sfm_data.views.size() - 1, 1, CV_32S); // save number of feature in each frame (last frame not needed)

	// iterate through each pair
#pragma omp parallel for
	for (int runIter = 0; runIter < sfm_data.views.size() - 1; runIter++) {
		Views::const_iterator iter = sfm_data.views.begin();
		advance(iter, runIter);

		// read descriptor
		string filename1 = stlplus::create_filespec(sMatchesDir,
				stlplus::basename_part(
						sfm_data.views.at(iter->first)->s_Img_path), "desc");
		string filename2 = stlplus::create_filespec(sMatchesDir,
				stlplus::basename_part(
						sfm_data.views.at((iter->first) + 1)->s_Img_path),
				"desc");
		cv::Mat desc1;
		cv::Mat desc2;
		hulo::readAKAZEBin(filename1, desc1);
		hulo::readAKAZEBin(filename2, desc2);

		// save number of feature. Note that feat number of last frame is not needed
		featNumber.at<int>(iter->first, 0) = desc1.rows;		//featLoc1.rows;

		// perform matching
		if (desc1.rows < 2 || desc2.rows < 2) {
			continue;
		}
		cv::Mat matchesMat;
		cv::Mat distMat;
		cv::flann::Index matchers(desc2, cv::flann::LshIndexParams(LSH_TABLE_NUMBER, LSH_KEY_SIZE, LSH_MULTI_PROBE_LEVEL),
				cvflann::flann_distance_t::FLANN_DIST_HAMMING);
		matchers.knnSearch(desc1, matchesMat, distMat, 2,
				cv::flann::SearchParams(FLANN_SEARCH_PARAMS_CHECKS, FLANN_SEARCH_PARAMS_EPS, FLANN_SEARCH_PARAMS_SORTED));

		// add match list
		// if 2 matches found: do ratio test
		// if 1 match found: just match
		// no matches found: skip
		vector<size_t> matchesInd(desc1.rows);
		for (int i = 0; i < desc1.rows; i++) {
			if ((0.0f + distMat.at<int>(i, 0)) / distMat.at<int>(i, 1) < fDistRatio) {
				if (distMat.at<int>(i, 1) < numeric_limits<int>::max()) {
					matchesInd[i] = matchesMat.at<int>(i, 0);
				}
			} else {
				matchesInd[i] = -1;
			}
		}

		// check if matches is one to one
		// i.e. no two or more features from query matches to a single training features
		for (int i = 0; i < matchesInd.size() - 1; i++) {
			if (matchesInd[i] == -1) {
				continue;
			}

			bool check1to1 = false;
			for (int j = i + 1; j < matchesInd.size(); j++) {
				if (matchesInd[i] == matchesInd[j]) {
					matchesInd[j] = -1;
					check1to1 = true;
				}
			}

			if (check1to1) {
				matchesInd[i] = -1;
			}
		}

		// save match
		for (int i = 0; i < matchesInd.size() - 1; i++) {
			if (matchesInd[i] != -1) {
				matches[Pair(iter->first, iter->first + 1)].push_back(IndMatch(i, matchesInd[i]));
			}
		}
	}

	// from matches, make track
	vector<cv::Mat> trackPointer;
	for (int frame = 0; frame < sfm_data.views.size() - 1; frame++) {
		cv::Mat tmp = -1 * cv::Mat::ones(featNumber.at<int>(frame, 0), 1, CV_32S);
		trackPointer.push_back(tmp);
		// set feature index that each feature in FRAME match to
		for (IndMatches::const_iterator iterMatch = matches[make_pair(frame, frame + 1)].begin();
				iterMatch != matches[Pair(frame, frame + 1)].end(); iterMatch++) {
#if (OPENMVG_VERSION_MAJOR<1)
			trackPointer.at(frame).at<int>(iterMatch->_i, 0) = static_cast<int>(iterMatch->_j);
#else
			trackPointer.at(frame).at<int>(iterMatch->i_, 0) = static_cast<int>(iterMatch->j_);
#endif
		}
	}

	// add matches to MATCHES
	for (int frame = 0; frame < sfm_data.views.size() - 1; frame++) {

		// skip frame+1 since already in matches
		for (int frameTo = frame + 2; frameTo < min(frame + maxFrameDist, sfm_data.views.size()); frameTo++) {

			// go through each feature and update match up to frame FRAMETO
			for (int i = 0; i < trackPointer.at(frame).rows; i++) {

				// get featID of feature in frame FRAMETO-1 that feature i of frame FRAME matches to
				int t = trackPointer.at(frame).at<int>(i, 0);
				if (t != -1) {
					int featMatchTo = trackPointer.at(frameTo - 1).at<int>(t, 0);
					trackPointer.at(frame).at<int>(i, 0) = featMatchTo;

					if (featMatchTo != -1) {
						matches[Pair(frame, frameTo)].push_back(IndMatch(i, featMatchTo));
					}
				}
			}
		}
	}
}

// match pairs from a list
// filenames : list of filenames of descriptor file
// pairs : list of pairs to match
// matches : list of matches for each pair
void hulo::matchAKAZEToQuery(const SfM_Data &sfm_data,
		const string &sMatchesDir,
		const string &sQueryMatchesDir,
		const vector<size_t> &pairs, // list of viewID to match to
		const size_t queryInd, // index of query
		const float fDistRatio, PairWiseMatches &matches,
		map<pair<size_t, size_t>, map<size_t, int>> &featDist) // map from pair of (viewID,viewID) to map of featID of queryImg to distance between the feature and its match in reconstructed images
		{

	cout << "Start putative matching" << endl;

	// read query descriptor
	string filename2 = stlplus::create_filespec(sQueryMatchesDir, sfm_data.views.at(queryInd)->s_Img_path, "desc");
	cv::Mat desc2;
	hulo::readAKAZEBin(filename2, desc2);

	if (desc2.rows < 1) {
		return;
	}

	int maxThreadNum = omp_get_max_threads();
	cv::flann::Index** matchers = new cv::flann::Index*[maxThreadNum];

#pragma omp parallel for
	for (int i = 0; i < maxThreadNum; i++) {
		matchers[i] = new cv::flann::Index(desc2,
				cv::flann::LshIndexParams(LSH_TABLE_NUMBER, LSH_KEY_SIZE, LSH_MULTI_PROBE_LEVEL),
				cvflann::flann_distance_t::FLANN_DIST_HAMMING);
	}

	// allocate space in matches
	for (vector<size_t>::const_iterator iterPair = pairs.begin();
			iterPair != pairs.end(); ++iterPair) {
		matches.insert(
				make_pair<Pair, IndMatches>(Pair(*iterPair, queryInd),
						IndMatches()));
	}

	// iterate through each pair
#pragma omp parallel for
	for (int runIter = 0; runIter < pairs.size(); runIter++) {
		vector<size_t>::const_iterator iter = pairs.begin();
		advance(iter, runIter);

		// read files
		string filename1 = stlplus::create_filespec(sMatchesDir,
				stlplus::basename_part(sfm_data.views.at(*iter)->s_Img_path),
				"desc");
		cv::Mat desc1;
		hulo::readAKAZEBin(filename1, desc1);

		// cout << iter->first << " " << iter->second << endl;

		// perform matching
		cv::Mat matchesMat;
		cv::Mat distMat;
		matchers[omp_get_thread_num()]->knnSearch(desc1, matchesMat, distMat, 2,
				cv::flann::SearchParams(FLANN_SEARCH_PARAMS_CHECKS, FLANN_SEARCH_PARAMS_EPS, FLANN_SEARCH_PARAMS_SORTED));

		// ratio test
		Pair pairInd(*iter, queryInd);
		IndMatches indMatches;
		map<size_t, int> featDistMap;
		for (int i = 0; i < desc1.rows; i++) {
			if ((0.0f + distMat.at<int>(i, 0)) / distMat.at<int>(i, 1)
					< fDistRatio) {
				if (distMat.at<int>(i, 1) < numeric_limits<int>::max()) {
					indMatches.push_back(IndMatch(i, matchesMat.at<int>(i, 0)));
					featDistMap[matchesMat.at<int>(i, 0)] = distMat.at<int>(i,
							0);
				}
			}
		}
#pragma omp critical
		{
			matches[pairInd] = indMatches;
			featDist[pairInd] = featDistMap;
		}
	}

	for (int i = 0; i < maxThreadNum; i++) {
		delete matchers[i];
	}
	delete matchers;
}

// perform geometric match
// use openMVG function
// inputting sfm_data instead of list of files because also need size of each image
void hulo::geometricMatch(SfM_Data &sfm_dataFull, // list of file names
		shared_ptr<Regions_Provider> regions_provider, // keypoint information
		PairWiseMatches &map_putativeMatches, // reference to putative matches
		PairWiseMatches &map_geometricMatches, // reference to output
		int ransacRound, // number of ransacRound
		double geomPrec,
		bool bGuided_matching) //precision of geometric matching
		{
	// construct sfm_data containing only those views in map_putative to reduce feature loading time
	SfM_Data sfm_data;
	for (PairWiseMatches::const_iterator iterP = map_putativeMatches.begin();
			iterP != map_putativeMatches.end(); ++iterP) {

		if (sfm_data.GetViews().find(iterP->first.first)
				== sfm_data.GetViews().end()) {
			sfm_data.views.insert(
					make_pair(iterP->first.first,
							make_shared<View>(
									sfm_dataFull.views.at(iterP->first.first)->s_Img_path,
									sfm_dataFull.views.at(iterP->first.first)->id_view,
									sfm_dataFull.views.at(iterP->first.first)->id_intrinsic,
									sfm_dataFull.views.at(iterP->first.first)->id_pose,
									sfm_dataFull.views.at(iterP->first.first)->ui_width,
									sfm_dataFull.views.at(iterP->first.first)->ui_height)));
		}

		if (sfm_data.GetViews().find(iterP->first.second)
				== sfm_data.GetViews().end()) {
			sfm_data.views.insert(
					make_pair(iterP->first.second,
							make_shared<View>(
									sfm_dataFull.views.at(iterP->first.second)->s_Img_path,
									sfm_dataFull.views.at(iterP->first.second)->id_view,
									sfm_dataFull.views.at(iterP->first.second)->id_intrinsic,
									sfm_dataFull.views.at(iterP->first.second)->id_pose,
									sfm_dataFull.views.at(iterP->first.second)->ui_width,
									sfm_dataFull.views.at(iterP->first.second)->ui_height)));
		}
	}

	ImageCollectionGeometricFilter collectionGeomFilter(&sfm_data, regions_provider);
	collectionGeomFilter.Robust_model_estimation(
			GeometricFilter_FMatrix_AC(geomPrec, ransacRound),
			map_putativeMatches, bGuided_matching);
	map_geometricMatches = collectionGeomFilter.Get_geometric_matches();

	cout << "number of putative matches : " << map_putativeMatches.size() << endl;
	cout << "number of geometric matches : " << map_geometricMatches.size() << endl;
}
