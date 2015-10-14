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
#include <openMVG/features/regions.hpp>
#include <openMVG/matching_image_collection/GeometricFilter.hpp>
#include <openMVG/matching_image_collection/F_ACRobust.hpp>

using namespace std;
using namespace cv;
using namespace openMVG;
using namespace openMVG::features;
using namespace openMVG::matching;
using namespace openMVG::sfm;

// match pairs from a list
// filenames : list of filenames of descriptor file
// pairs : list of pairs to match
// matches : list of matches for each pair
void hulo::matchAKAZE(const SfM_Data &sfm_data, const string &sMatchesDir,
		const vector<pair<size_t, size_t>> &pairs, const float fDistRatio,
		PairWiseMatches &matches) {

	cout << "Start putative matching" << endl;

	int maxThreadNum = omp_get_max_threads();
	//	FlannBasedMatcher** matchers = new FlannBasedMatcher*[maxThreadNum];
	//#pragma omp parallel for
	//	for(int i=0; i<maxThreadNum; i++) {
	//		matchers[i] = new FlannBasedMatcher();
	//	}
	// iterate through each pair
#pragma omp parallel for
	//for(vector<pair<size_t,size_t>>::const_iterator iter = pairs.begin();iter!=pairs.end();iter++){
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
		if (hulo::SAVE_DESC_BINARY) {
			hulo::readMatBin(filename1, desc1);
			hulo::readMatBin(filename2, desc2);
		} else {
			FileStorage file1(filename1, FileStorage::READ);
			FileStorage file2(filename2, FileStorage::READ);
			file1["Descriptors"] >> desc1;
			file2["Descriptors"] >> desc2;
			file1.release();
			file2.release();
		}
		// cout << iter->first << " " << iter->second << endl;

		// perform matching
		if (desc1.rows < 2 || desc2.rows < 2) {
			continue;
		}
		//		vector<vector<DMatch>> matchesOut;
		//		matchers[omp_get_thread_num()]->knnMatch(desc1, desc2, matchesOut,2);

		cv::Mat matchesMat;
		cv::Mat distMat;
		cv::flann::Index matchers(desc2, cv::flann::LshIndexParams(2, 20, 2),
				cvflann::flann_distance_t::FLANN_DIST_HAMMING);
		matchers.knnSearch(desc1, matchesMat, distMat, 2,
				cv::flann::SearchParams(2, 0, true));

		// ratio test
		vector<size_t> matchesInd(desc1.rows);
		for (int i = 0; i < desc1.rows; i++) {
			//			if(matchesOut[i][0].distance/matchesOut[i][1].distance < fDistRatio){
			//				matchesInd[i] = matchesOut[i][0].trainIdx;
			//			} else {
			//				matchesInd[i] = -1;
			//			}
			if ((0.0f + distMat.at<int>(i, 0)) / distMat.at<int>(i, 1)
					< fDistRatio) {
				if (distMat.at<int>(i, 1) < numeric_limits<int>::max()) {
					matchesInd[i] = matchesMat.at<int>(i, 0);
				}
			} else {
				matchesInd[i] = -1;
			}
		}

		//		// check if matches is one to one
		//		// i.e. no two or more features from query matches to a single training features
		//		for(int i = 0 ; i < matchesInd.size()-1; i++){
		//			for(int j = i+1 ; j < matchesInd.size(); j++){
		//				if(matchesInd[i]==matchesInd[j]){
		//					matchesInd[i] = -1;
		//					matchesInd[j] = -1;
		//				}
		//			}
		//		}

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
				//matches[*iter].push_back(IndMatch(matchesOut[i][0].queryIdx,matchesOut[i][0].trainIdx));
				matches[*iter].push_back(IndMatch(i, matchesInd[i]));
			}
		}

		//matches.insert(make_pair(*iter,matchesOut));
		// cout << " " << matches[*iter].size() << endl;
	}
	//	for(int i=0; i<maxThreadNum; i++) {
	//		delete matchers[i];
	//	}
	//	delete matchers;
}

// perform tracking on AKAZE and return as match
// maxFrameDist : maximum distance between frame to write as match (though track length can be longer)
void hulo::trackAKAZE(const SfM_Data &sfm_data, const string &sMatchesDir,
		const size_t maxFrameDist, const float fDistRatio, PairWiseMatches &matches) {

	cout << "Start putative matching" << endl;

	cv::Mat featNumber = cv::Mat::zeros(sfm_data.views.size() - 1, 1, CV_32S); // save number of feature in each frame (last frame not needed)

	// iterate through each pair
#pragma omp parallel for
	//for(vector<pair<size_t,size_t>>::const_iterator iter = pairs.begin();iter!=pairs.end();iter++){
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
		if (hulo::SAVE_DESC_BINARY) {
			hulo::readMatBin(filename1, desc1);
			hulo::readMatBin(filename2, desc2);
		} else {
			FileStorage file1(filename1, FileStorage::READ);
			FileStorage file2(filename2, FileStorage::READ);
			file1["Descriptors"] >> desc1;
			file2["Descriptors"] >> desc2;
			file1.release();
			file2.release();
		}

		// save number of feature. Note that feat number of last frame is not needed
		featNumber.at<int>(iter->first, 0) = desc1.rows;		//featLoc1.rows;

		// perform matching
		if (desc1.rows < 2 || desc2.rows < 2) {
			continue;
		}
		cv::Mat matchesMat;
		cv::Mat distMat;
		cv::flann::Index matchers(desc2, cv::flann::LshIndexParams(2, 20, 2),
				cvflann::flann_distance_t::FLANN_DIST_HAMMING);
		matchers.knnSearch(desc1, matchesMat, distMat, 2,
				cv::flann::SearchParams(2, 0, true));

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
			trackPointer.at(frame).at<int>(iterMatch->_i, 0) = static_cast<int>(iterMatch->_j);
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
		const vector<size_t> &pairs, // list of viewID to match to
		const size_t queryInd, // index of query
		const float fDistRatio, PairWiseMatches &matches,
		map<pair<size_t, size_t>, map<size_t, int>> &featDist) // map from pair of (viewID,viewID) to map of featID of queryImg to distance between the feature and its match in reconstructed images
		{

	cout << "Start putative matching" << endl;

	// read query descriptor
	string filename2 = stlplus::create_filespec(sMatchesDir,
			stlplus::basename_part(sfm_data.views.at(queryInd)->s_Img_path),
			"desc");
	cv::Mat desc2;
	if (SAVE_DESC_BINARY) {
		hulo::readMatBin(filename2, desc2);
	} else {
		FileStorage file2(filename2, FileStorage::READ);
		file2["Descriptors"] >> desc2;
		file2.release();
	}

	if (desc2.rows < 1) {
		return;
	}

	int maxThreadNum = omp_get_max_threads();
	//FlannBasedMatcher** matchers = new FlannBasedMatcher*[maxThreadNum];
	cv::flann::Index** matchers = new cv::flann::Index*[maxThreadNum];

#pragma omp parallel for
	for (int i = 0; i < maxThreadNum; i++) {
		//		matchers[i] = new FlannBasedMatcher();
		//		matchers[i]->add(desc2);
		//		matchers[i]->train();
		//matchers[i] = new cv::flann::Index(desc2,cv::flann::KDTreeIndexParams(1));
		matchers[i] = new cv::flann::Index(desc2,
				cv::flann::LshIndexParams(2, 20, 2),
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
	//for(vector<pair<size_t,size_t>>::const_iterator iter = pairs.begin();iter!=pairs.end();iter++){
	for (int runIter = 0; runIter < pairs.size(); runIter++) {
		vector<size_t>::const_iterator iter = pairs.begin();
		advance(iter, runIter);
		// read files

		string filename1 = stlplus::create_filespec(sMatchesDir,
				stlplus::basename_part(sfm_data.views.at(*iter)->s_Img_path),
				"desc");
		cv::Mat desc1;
		if (SAVE_DESC_BINARY) {
			hulo::readMatBin(filename1, desc1);
		} else {
			FileStorage file1(filename1, FileStorage::READ);
			file1["Descriptors"] >> desc1;
			file1.release();
		}

		// cout << iter->first << " " << iter->second << endl;

		// perform matching
		//vector<vector<DMatch>> matchesOut;
		//cv::Mat matchesF;
		//cv::Mat distF;
		//matchers[omp_get_thread_num()]->knnSearch(desc1,matchesF,distF,2,cv::flann::SearchParams(55,20,true));
		cv::Mat matchesMat;
		cv::Mat distMat;
		matchers[omp_get_thread_num()]->knnSearch(desc1, matchesMat, distMat, 2,
				cv::flann::SearchParams(2, 0, true));

		// ratio test

		Pair pairInd(*iter, queryInd);
		IndMatches indMatches;
		map<size_t, int> featDistMap;
		//#		pragma omp critical
		for (int i = 0; i < desc1.rows; i++) {
			//			if(distF.at<float>(i,0)/distF.at<float>(i,1) < fDistRatio){
			//				matches[pairInd].push_back(IndMatch(i,matchesF.at<int>(i,0)));
			//				if(featDist.find(pairInd) == featDist.end()){
			//					map<size_t,float> v;
			//					featDist[pairInd] = v;
			//				}
			//				featDist[pairInd][matchesF.at<int>(i,0)] = distF.at<float>(i,0);
			//			}
			if ((0.0f + distMat.at<int>(i, 0)) / distMat.at<int>(i, 1)
					< fDistRatio) {
				if (distMat.at<int>(i, 1) < numeric_limits<int>::max()) {
					//cout << (0.0f+distMat.at<int>(i,0))/distMat.at<int>(i,1) << " " << matchesMat.at<int>(i,0) << " " << distMat.at<int>(i,0) << " " << matchesMat.at<int>(i,1) << " " << distMat.at<int>(i,1) << endl;
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
		//matches.insert(make_pair(*iter,matchesOut));
		// cout << " " << matches[*iter].size() << endl;
	}

	//	int maxThreadNum = omp_get_max_threads();
	//	FlannBasedMatcher** matchers = new FlannBasedMatcher*[maxThreadNum];
	//
	//
	//#pragma omp parallel for
	//	for(int i=0; i<maxThreadNum; i++) {
	//		matchers[i] = new FlannBasedMatcher();
	//		matchers[i]->add(desc2);
	//		matchers[i]->train();
	//	}
	//
	//	// allocate space in matches
	//	for(vector<size_t>::const_iterator iterPair = pairs.begin();
	//			iterPair != pairs.end(); ++iterPair){
	//		matches.insert(make_pair<Pair,IndMatches>(Pair(*iterPair,queryInd),IndMatches()));
	//	}
	//
	//	// iterate through each pair
	//#pragma omp parallel for
	//	//for(vector<pair<size_t,size_t>>::const_iterator iter = pairs.begin();iter!=pairs.end();iter++){
	//	for(int runIter = 0; runIter < pairs.size(); runIter++){
	//		vector<size_t>::const_iterator iter = pairs.begin();
	//		advance(iter,runIter);
	//		// read files
	//
	//		string filename1 = stlplus::create_filespec(sMatchesDir,stlplus::basename_part(sfm_data.views.at(*iter)->s_Img_path),"desc");
	//		cv::Mat desc1;
	//		if (SAVE_DESC_BINARY) {
	//			hulo::readMatBin(filename1, desc1);
	//		} else {
	//			FileStorage file1(filename1,FileStorage::READ);
	//			file1["Descriptors"] >> desc1;
	//			file1.release();
	//		}
	//
	//		// cout << iter->first << " " << iter->second << endl;
	//
	//		// perform matching
	//		vector<vector<DMatch>> matchesOut;
	//		matchers[omp_get_thread_num()]->knnMatch(desc1, matchesOut,2);
	//
	//		// ratio test
	//		Pair pairInd = Pair(*iter,queryInd);
	//		//#		pragma omp critical
	//		for(int i = 0; i < desc1.rows; i++){
	//			if(matchesOut[i][0].distance/matchesOut[i][1].distance < fDistRatio){
	//				matches[pairInd].push_back(IndMatch(matchesOut[i][0].queryIdx,matchesOut[i][0].trainIdx));
	//				if(featDist.find(pairInd) == featDist.end()){
	//					map<size_t,float> v;
	//					featDist[pairInd] = v;
	//				}
	//				featDist[pairInd][matchesOut[i][0].trainIdx] = matchesOut[i][0].distance;
	//			}
	//		}
	//
	//		//matches.insert(make_pair(*iter,matchesOut));
	//		// cout << " " << matches[*iter].size() << endl;
	//	}
	for (int i = 0; i < maxThreadNum; i++) {
		delete matchers[i];
	}
	delete matchers;
}

//void matchAKAZEToQuery(const SfM_Data &sfm_data,
//		const string &sMatchesDir,
//		const vector<size_t> &pairs, // list of viewID to match to
//		const size_t queryInd, // index of query
//		const float fDistRatio,
//		PairWiseMatches &matches){
//
//	cout << "Start putative matching" << endl;
//
//	// read query descriptor
//	string filename2 = stlplus::create_filespec(sMatchesDir,stlplus::basename_part(sfm_data.views.at(queryInd)->s_Img_path),"desc");
//	cv::Mat desc2;
// if (SAVE_DESC_BINARY) {
//		hulo::readMatBin(filename2, desc2);
//	} else {
//		FileStorage file2(filename2,FileStorage::READ);
//		file2["Descriptors"] >> desc2;
//		file2.release();
//		}
//
//	flann::KDTreeIndexParams indexParam(5);
//	//flann::AutotunedIndexParams indexParam(0.5,0.01,0,0.1);
//	flann::Index kdtree = flann::Index(desc2,indexParam);
//
//	// iterate through each pair
//#pragma omp parallel for
//	//for(vector<pair<size_t,size_t>>::const_iterator iter = pairs.begin();iter!=pairs.end();iter++){
//	for(int runIter = 0; runIter < pairs.size(); runIter++){
//		vector<size_t>::const_iterator iter = pairs.begin();
//		advance(iter,runIter);
//		// read files
//
//		string filename1 = stlplus::create_filespec(sMatchesDir,stlplus::basename_part(sfm_data.views.at(*iter)->s_Img_path),"desc");
//		cv::Mat desc1;
// 	if (SAVE_DESC_BINARY) {
//			hulo::readMatBin(filename1, desc1);
//		} else {
//			FileStorage file1(filename1,FileStorage::READ);
//			file1["Descriptors"] >> desc1;
//			file1.release();
//			}
//
//
//		// cout << iter->first << " " << iter->second << endl;
//
//		// perform matching
//
//		pair<size_t,size_t> pairMatch = make_pair(*iter,queryInd);
//		cv::Mat indMatches;
//		cv::Mat dist;
//		kdtree.knnSearch(desc1,indMatches,dist,2,flann::SearchParams(32));
//		for(int i = 0; i < desc1.rows; i++){
//			cout << dist.at<float>(i,0)/dist.at<float>(i,1) << " ";
//			if(dist.at<float>(i,0)/dist.at<float>(i,1) < fDistRatio){
//				matches[pairMatch].push_back(IndMatch(i,indMatches.at<IndexT>(i,0)));
//			}
//		}
//		cout << "adfdafad" << endl;
//
////		vector<vector<DMatch>> matchesOut;
////		matcher.knnMatch(desc1, matchesOut,2);
////
////		// ratio test
////		for(int i = 0; i < desc1.rows; i++){
////			if(matchesOut[i][0].distance/matchesOut[i][1].distance < fDistRatio){
////				matches[make_pair(*iter,queryInd)].push_back(IndMatch(matchesOut[i][0].queryIdx,matchesOut[i][0].trainIdx));
////			}
////		}
//
//		//matches.insert(make_pair(*iter,matchesOut));
//		// cout << " " << matches[*iter].size() << endl;
//	}
//
//}

// perform geometric match
// use openMVG function
// inputting sfm_data instead of list of files because also need size of each image
void hulo::geometricMatch(SfM_Data &sfm_dataFull, // list of file names
		const string &matchesDir, // location of .feat files
		PairWiseMatches &map_putativeMatches, // reference to putative matches
		PairWiseMatches &map_geometricMatches, // reference to output
		int ransacRound, // number of ransacRound
		double geomPrec) //precision of geometric matching
		{
	cout << "Start geometric matching" << endl;
	//	double start = (double) getTickCount();

	// create region type
	unique_ptr<Regions> regions;
	regions.reset(new AKAZE_Float_Regions);

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

	//	double getsfm = (double) getTickCount();

	// construct feature providers for geometric matches
	shared_ptr<Features_Provider> feats_provider =
			make_shared<Features_Provider>();
	if (!feats_provider->load(sfm_data, matchesDir, regions)) {
		cerr << "Cannot construct feature providers," << endl;
		return;
	}

	//	double loadFeat = (double) getTickCount();

	// get image size
	vector<pair<size_t, size_t>> vec_imagesSize;
	vec_imagesSize.reserve(
			sfm_dataFull.GetViews().rbegin()->second->id_view + 1); // reserve space for vector equal to number of views due to openMVG format used in geometric filter
	for (Views::const_iterator iter = sfm_dataFull.GetViews().begin();
			iter != sfm_dataFull.GetViews().end(); iter++) {
		vec_imagesSize.insert(vec_imagesSize.begin() + iter->first,
				make_pair(iter->second->ui_width, iter->second->ui_height));
	}
	//	double getSize = (double) getTickCount();

	ImageCollectionGeometricFilter collectionGeomFilter(feats_provider.get());
	for (Hash_Map<IndexT, PointFeatures>::const_iterator iter =
			feats_provider->feats_per_view.begin();
			iter != feats_provider->feats_per_view.end(); iter++)
		cout << "(" << iter->first << "," << iter->second.size() << ") ";
	cout << endl;
	cout << endl;
	for (PairWiseMatches::const_iterator iter = map_putativeMatches.begin();
			iter != map_putativeMatches.end(); iter++)
		cout << "(" << iter->first.first << "," << iter->first.second << ","
				<< iter->second.size() << ") ";
	cout << endl;
	cout << map_putativeMatches.size() << "e" << vec_imagesSize.size() << endl;
	collectionGeomFilter.Filter(
			GeometricFilter_FMatrix_AC(geomPrec, ransacRound),
			map_putativeMatches, map_geometricMatches, vec_imagesSize);
	//	double matchingT = (double) getTickCount();
	cout << "mapgeo size: " << map_geometricMatches.size() << endl;
	for (PairWiseMatches::const_iterator iter = map_geometricMatches.begin();
			iter != map_geometricMatches.end(); iter++)
		cout << "(" << iter->first.first << "," << iter->first.second << ","
				<< iter->second.size() << ") ";
	//	double endTime = (double) getTickCount();
	//	cout << "SfM time: " << (getsfm-start)/getTickFrequency()<< endl;
	//		cout << "Loading time: " << (loadFeat-getsfm)/getTickFrequency() << endl;
	//		cout << "Vecsize time: " << (getSize-loadFeat)/getTickFrequency()<< endl;
	//		cout << "Matching time: " << (matchingT-getSize)/getTickFrequency()<< endl;
	//		cout << "End: " << (endTime - matchingT)/getTickFrequency()<< endl;
	//		cout << "Total: " << (endTime - start)/getTickFrequency()<< endl;
}

// match pairs from a list to a query file
// sfm_data : list of filenames of descriptor file
// pairs : list of pairs to match
// matches : list of matches for each pair
void hulo::matchAKAZE3DToQuery(const SfM_Data &sfm_data,
		const string &sMatchesDir,
		int qIndex, // index of query image in sfm_data
		const float fDistRatio, PairWiseMatches &matches,
		set<IndexT> &localViews) {

	// compute list of features used in each reconstructed image
	map<int, vector<int> > imgFeatList;
	hulo::listReconFeat(sfm_data, imgFeatList);

	cout << "Start putative matching" << endl;

	int maxThreadNum = omp_get_max_threads();
	FlannBasedMatcher** matchers = new FlannBasedMatcher*[maxThreadNum];
#pragma omp parallel for
	for (int i = 0; i < maxThreadNum; i++) {
		matchers[i] = new FlannBasedMatcher();
	}

	// read query file
	string qFilename = stlplus::create_filespec(sMatchesDir,
			stlplus::basename_part(sfm_data.views.at(qIndex)->s_Img_path),
			"desc");
	cv::Mat qDesc;
	if (SAVE_DESC_BINARY) {
		hulo::readMatBin(qFilename, qDesc);
	} else {
		FileStorage file1(qFilename, FileStorage::READ);
		file1["Descriptors"] >> qDesc;
		file1.release();
	}

	// iterate through each pair
#pragma omp parallel for
	//for(vector<pair<size_t,size_t>>::const_iterator iter = pairs.begin();iter!=pairs.end();iter++){
	for (int runIter = 0; runIter < imgFeatList.size(); runIter++) {
		if (runIter != qIndex && localViews.find(runIter) != localViews.end()) { // prevent matching with query image and image too far away

				// get iterator to image to be matched
			map<int, vector<int>>::const_iterator iter = imgFeatList.begin();
			advance(iter, runIter);

			// read descriptor files
			string reconFilename = stlplus::create_filespec(sMatchesDir,
					stlplus::basename_part(
							sfm_data.views.at(iter->first)->s_Img_path),
					"desc");
			cv::Mat reconDescTmp;
			if (SAVE_DESC_BINARY) {
				hulo::readMatBin(reconFilename, reconDescTmp);
			} else {
				FileStorage file2(reconFilename, FileStorage::READ);
				file2["Descriptors"] >> reconDescTmp;
				file2.release();
			}

			cv::Mat reconDesc;
			hulo::copyRowsMat(reconDescTmp, iter->second, reconDesc);

			//cout << iter->first << " " << reconFilename;

			// perform matching
			vector<vector<DMatch>> matchesOut;
			matchers[omp_get_thread_num()]->knnMatch(reconDesc, qDesc,
					matchesOut, 2);

			// ratio test
			for (int i = 0; i < matchesOut.size(); i++) {
				if (matchesOut[i][0].distance / matchesOut[i][1].distance
						< fDistRatio) {
					matches[make_pair(iter->first, qIndex)].push_back(
							IndMatch(
									imgFeatList[iter->first][matchesOut[i][0].queryIdx], // true index in sfm_data of reconstructed feature
									matchesOut[i][0].trainIdx));
				}
			}

			//matches.insert(make_pair(*iter,matchesOut));
			//cout << " " << matches[make_pair(iter->first,qIndex)].size() << endl;
		}
	}
	for (int i = 0; i < maxThreadNum; i++) {
		delete matchers[i];
	}
	delete matchers;
}
