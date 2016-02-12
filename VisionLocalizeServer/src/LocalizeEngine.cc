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

#include "LocalizeEngine.h"

#include <uuid/uuid.h>
#include <omp.h>

#include <openMVG/features/regions.hpp>
#include <openMVG/features/regions_factory.hpp>
#include <openMVG/sfm/pipelines/sfm_robust_model_estimation.hpp>
#include <openMVG/sfm/pipelines/sfm_matches_provider.hpp>
#include <openMVG/sfm/pipelines/localization/SfM_Localizer.hpp>

#include <opencv2/core/eigen.hpp>

#include "AKAZEOpenCV.h"
#include "FileUtils.h"
#include "MatchUtils.h"
#include "BowDenseFeatureSettings.h"
#include "BoFUtils.h"
#include "BeaconUtils.h"

using namespace std;
using namespace cv;
using namespace Eigen;
using namespace openMVG;
using namespace openMVG::sfm;
using namespace openMVG::cameras;
using namespace openMVG::features;
using namespace openMVG::matching;

using namespace hulo;

namespace {
	int MINUM_NUMBER_OF_POINT_PUTATIVE_MATCH = 16;
	int MINUM_NUMBER_OF_POINT_RESECTION = 8;
	int MINUM_NUMBER_OF_INLIER_RESECTION = 10;

	int THRESHOLD_CALC_BEACON_COOCCURRENCE = 10;
	int MINUM_NUMBER_OF_SAME_BEACONS = 2;
	double BEACON_COOCCURRENCE = 0.5;
	int BEACON_SKIP_FRAME = 1;
	NormBeaconSignalType NORMALIZE_BEACON_TYPE = NormBeaconSignalType::MEDIAN;
}

LocalizeEngine::LocalizeEngine()
{
}

LocalizeEngine::LocalizeEngine(const std::string sfmDataDir, const std::string matchDir, const std::string AmatFile,
		double secondTestRatio, int ransacRound, double ransacPrecision, bool guidedMatching, int beaconKnnNum, int bowKnnNum)
{
	mSfmDataDir = sfmDataDir;
	mMatchDir = matchDir;
	mSecondTestRatio = secondTestRatio;
	mRansacRound = ransacRound;
	mRansacPrecision = ransacPrecision;
	mGuidedMatching = guidedMatching;

	const string sSfM_data = stlplus::create_filespec(mSfmDataDir, "sfm_data", "json");
	cout << "Reading sfm_data.json file : " << sSfM_data << endl;
	if (!Load(mSfmData, sSfM_data,
			ESfM_Data(VIEWS | INTRINSICS | EXTRINSICS | STRUCTURE))) {
		cerr << endl << "The input sfm_data.json file \"" << sSfM_data << "\" cannot be read." << endl;
	}

	// construct feature providers for geometric matches
	unique_ptr<Regions> regions;
	regions.reset(new AKAZE_Binary_Regions);
	mRegionsProvider = make_shared<Regions_Provider>();
	if (!mRegionsProvider->load(mSfmData, mMatchDir, regions)) {
		cerr << "Cannot construct regions providers," << endl;
	}

	// get map of ViewID, FeatID to 3D point
	hulo::structureToMapViewFeatTo3D(mSfmData.GetLandmarks(), mMapViewFeatTo3D);

	cout << "Reading A mat file : " << AmatFile << endl;
	cv::FileStorage storage(AmatFile, cv::FileStorage::READ);
	storage["A"] >> mA;
	storage.release();
	cout << "Loaded A mat : " << mA << endl;

	// if BoW file exists, use BOW model
	const string bowFile = stlplus::create_filespec(mMatchDir, "BOWfile", "yml");
	if (stlplus::file_exists(bowFile)) {
		cout << "find BOW model : " << bowFile << endl;
		if (bowKnnNum>0) {
			mUseBow = true;
			mBowKnnNum = bowKnnNum;

			mDenseLocalFeatureWrapper = new DenseLocalFeatureWrapper(hulo::BOW_DENSE_LOCAL_FEATURE_TYPE,
					hulo::RESIZED_IMAGE_SIZE, hulo::USE_COLOR_FEATURE, hulo::L1_SQUARE_ROOT_LOCAL_FEATURE);

			cv::FileStorage fsRead(bowFile, cv::FileStorage::READ);
			mBofPyramids = new BoFSpatialPyramids();
			mBofPyramids->read(fsRead.root());
			cout << "loaded BOW model." << endl;

			// if PCA file exists, use PCA for BOW model
			const string pcaFile = stlplus::create_filespec(mMatchDir, "PCAfile", "yml");
			if (stlplus::file_exists(pcaFile)) {
				cout << "find PCA model for BOW : " << pcaFile << endl;
				mUseBowPca = true;

				cv::FileStorage fsRead(pcaFile, cv::FileStorage::READ);
				mPcaWrapper = new PcaWrapper();
				mPcaWrapper->read(fsRead.root());
				cout << "loaded PCA model." << endl;
			}
		} else {
			cout << "KNN number for BOW model is not set, localize without using BOW model" << endl;
		}
	} else {
		cout << "cannot find BOW model, localize without using BOW model" << endl;
	}
	// if beacon signal for SfM model exists, load beacon signal for all views
	const string beaconFile = stlplus::create_filespec(mSfmDataDir, "beacon", "txt");
	if (stlplus::file_exists(beaconFile)){
		cout << "find beacon signal for SfM model : " << beaconFile << endl;
		if (beaconKnnNum>0) {
			mUseBeacon = true;
			mBeaconKnnNum = beaconKnnNum;

			hulo::parseBeaconList(beaconFile, mMajorMinorIdIndexMap, mSfmViewRSSI);
			cout << "loaded beacon signal for SfM model." << endl;
		} else {
			cout << "KNN number for iBeacon is not set, localize without using iBeacon" << endl;
		}
	}
}

std::unique_ptr<openMVG::features::Regions> LocalizeEngine::extractAKAZESingleImg(const std::string& imageID,
		const cv::Mat& image, const std::string& outputFolder, const hulo::AKAZEOption &akazeOption,
		std::vector<std::pair<float, float>> &locFeat) {
	double startAKAZE = (double) getTickCount();

	Ptr<AKAZE> akaze = AKAZE::create(
			AKAZE::DESCRIPTOR_MLDB, 0, akazeOption.desc_ch, akazeOption.thres,
			akazeOption.nOct, akazeOption.nOctLay);
	double initAKAZE = (double) getTickCount();

	// write out to file
	string filebase = stlplus::create_filespec(outputFolder, imageID);
	string sFeat = filebase + ".feat";
	string sDesc = filebase + ".desc";

	// extract features
	vector<KeyPoint> kpts;
	cv::Mat desc;
	akaze->detectAndCompute(image, noArray(), kpts, desc);
	double extractFeature = (double) getTickCount();

	// write out
	ofstream fileFeat(sFeat);
	FileStorage fileDesc(sDesc, FileStorage::WRITE);

	if (fileFeat.is_open() && fileDesc.isOpened()) {
		// save each feature write each feature to file
		for (vector<KeyPoint>::const_iterator iterKP = kpts.begin();
				iterKP != kpts.end(); iterKP++) {
			locFeat.push_back(
					make_pair(static_cast<float>(iterKP->pt.x),
							static_cast<float>(iterKP->pt.y)));
			fileFeat << iterKP->pt.x << " " << iterKP->pt.y << " "
				<< iterKP->size << " " << iterKP->angle << endl;
		}
		fileFeat.close();

		// write descriptor to file
		fileDesc.release();
		hulo::saveAKAZEBin(sDesc, desc);
	} else {
		cerr << "cannot open file to write features for " << filebase
				<< endl;
	}
	double writeFeature = (double) getTickCount();

	unique_ptr<Regions> region_type;
	region_type.reset(new AKAZE_Binary_Regions);
	std::unique_ptr<Regions> regions(region_type->EmptyClone());
	if (!regions->Load(sFeat, sDesc)) {
		cout << "cannot load region from the written feature file" << endl;
	}
	double endAKAZE = (double) getTickCount();

	cout << "Init AKAZE : " << (initAKAZE - startAKAZE) / getTickFrequency() << " s\n";
	cout << "Extract feature : " << (extractFeature - initAKAZE) / getTickFrequency() << " s\n";
	cout << "Write feature : " << (writeFeature - extractFeature) / getTickFrequency() << " s\n";
	cout << "Load regions : " << (endAKAZE - writeFeature) / getTickFrequency() << " s\n";
	cout << "Total AKAZE time : " << (endAKAZE - startAKAZE) / getTickFrequency() << " s\n";

	return regions;
}

void LocalizeEngine::getLocalViews(openMVG::sfm::SfM_Data &sfm_data, const cv::Mat &A, const cv::Mat &loc,
		const double radius, std::set<openMVG::IndexT> &localViews) {
	// iterate through sfm_data
	for (Views::const_iterator iter = sfm_data.views.begin();
			iter != sfm_data.views.end(); iter++) {
		if (sfm_data.poses.find(iter->second->id_pose) != sfm_data.poses.end()) {
			Vec3 pose = sfm_data.poses.at(iter->second->id_pose).center();

			cv::Mat pose_h(4, 1, CV_64F);
			pose_h.at<double>(0) = pose[0];
			pose_h.at<double>(1) = pose[1];
			pose_h.at<double>(2) = pose[2];
			pose_h.at<double>(3) = 1.0;
			cv::Mat gpose_h = mA * pose_h;
			cv::Mat gpose(3, 1, CV_64F);
			gpose.at<double>(0) = gpose_h.at<double>(0);
			gpose.at<double>(1) = gpose_h.at<double>(1);
			gpose.at<double>(2) = gpose_h.at<double>(2);
			if (norm(gpose - loc) <= radius) {
				localViews.insert(iter->first);
			}
		}
	}
}

std::vector<double> LocalizeEngine::localize(const cv::Mat& image, const std::string workingDir,
		const std::string& beaconStr, const std::vector<double>& center, double radius)
{
	vector<double> result;
	double startQuery = (double) getTickCount();

	// find restricted views if center and radius are specified
	set<IndexT> localViews;
	if (center.size()==3 && radius>0) {
		cv::Mat cenLoc(3, 1, CV_64FC1);
		cenLoc.at<double>(0) = center[0];
		cenLoc.at<double>(1) = center[1];
		cenLoc.at<double>(2) = center[2];

		cout << "select view : center = " << cenLoc << ", radius = " << radius << endl;
		getLocalViews(mSfmData, mA, cenLoc, radius, localViews);
		cout << "number of selected local views by center location : " << localViews.size() << endl;
		if (localViews.size()==0) {
			return result;
		}
	} else {
		for (Views::const_iterator iter = mSfmData.views.begin(); iter != mSfmData.views.end(); iter++) {
			if (mSfmData.poses.find(iter->second->id_pose) != mSfmData.poses.end()) {
				localViews.insert(iter->first);
			}
		}
	}
	double loadSfmViews = (double) getTickCount();

	if (mUseBeacon && mBeaconKnnNum>0 && beaconStr.size()>0) {
		cv::Mat queryRSSI = cv::Mat::zeros(1, mMajorMinorIdIndexMap.size(), CV_8UC1);
		string queryBeacon = beaconStr;
		hulo::parseBeaconString(queryBeacon, mMajorMinorIdIndexMap, queryRSSI);

		normalizeBeacon(queryRSSI, NORMALIZE_BEACON_TYPE);
		getBeaconView(queryRSSI, mMajorMinorIdIndexMap, mSfmViewRSSI, localViews,
				THRESHOLD_CALC_BEACON_COOCCURRENCE, MINUM_NUMBER_OF_SAME_BEACONS,
				mBeaconKnnNum, BEACON_COOCCURRENCE, mSfmData, BEACON_SKIP_FRAME, NORMALIZE_BEACON_TYPE);
	}
	double selectViewsByBeacon = (double) getTickCount();

	if (mUseBow && mBowKnnNum>0 && localViews.size() > mBowKnnNum) {
		std::vector<cv::KeyPoint> keypoints;
		cv::Mat descriptors = mDenseLocalFeatureWrapper->calcDenseLocalFeature(image, keypoints);

		cv::Mat bofMat;
		if (mUseBowPca) {
			cv::Mat pcaDescriptors = mPcaWrapper->calcPcaProject(descriptors);
			bofMat = mBofPyramids->calcBoF(pcaDescriptors, keypoints);
		} else {
			bofMat = mBofPyramids->calcBoF(descriptors, keypoints);
		}

		set<openMVG::IndexT> bofSelectedViews;
		hulo::selectViewByBoF(bofMat, mMatchDir, localViews, mSfmData, mBowKnnNum, bofSelectedViews);
		localViews.clear();
		localViews = set<openMVG::IndexT>(bofSelectedViews);
		cout << "number of selected local views by bow : " << localViews.size() << endl;
	}
	double selectViewsByBow = (double) getTickCount();

	///////////////////////////////////////
	// Extract features from query image //
	///////////////////////////////////////
	cout << "Extract features from query image" << endl;
	// create and save image describer file
	string sImageDesc = stlplus::create_filespec(workingDir, "image_describer.txt");
	AKAZEOption akazeOption = AKAZEOption();
	akazeOption.read(sImageDesc);
	double loadImageDescriber = (double) getTickCount();

	// generate ID of query image to store feature
	string imageID;
	{
		uuid_t uuidValue;
		uuid_generate(uuidValue);
		char uuidChar[36];
		uuid_unparse_upper(uuidValue, uuidChar);
		imageID = string(uuidChar);
	}
	int imageHeight = image.rows;
	int imageWidth = image.cols;
	cout << "image ID :" << imageID << endl;
	cout << "image size : " << image.size() << endl;
	double generateUUID = (double) getTickCount();

	vector<pair<float, float>> qFeatLoc;
	unique_ptr<Regions> queryRegions = extractAKAZESingleImg(imageID, image, workingDir, akazeOption, qFeatLoc);
	double endFeat = (double) getTickCount();

	/////////////////////////////////////////
	// Match features to those in SfM file //
	/////////////////////////////////////////
	// add query image to sfm_data
	IndexT indQueryFile = mSfmData.views.rbegin()->second->id_view + 1; // set index of query file to (id of last view of sfm data) + 1
	cout << "query image size : " << imageWidth << " x " << imageHeight << endl;
	mSfmData.views.insert(make_pair(indQueryFile, make_shared<View>(imageID, indQueryFile, 0, 0, imageWidth, imageHeight)));
	mRegionsProvider->regions_per_view[indQueryFile] = std::move(queryRegions);
	cout << "query image index : " << indQueryFile << endl;
	cout << "query image path : " << mSfmData.views.at(indQueryFile)->s_Img_path << endl;

	// generate pairs
	vector<size_t> pairs;
	if (localViews.size()==0) {
		// use all views
		for (Views::const_iterator iter = mSfmData.views.begin(); iter != mSfmData.views.end(); iter++) {
			pairs.push_back(iter->second->id_view);
		}
	} else {
		// restrict views
		for (Views::const_iterator iter = mSfmData.views.begin(); iter != mSfmData.views.end(); iter++) {
			if (localViews.find(iter->second->id_view) != localViews.end()) {
				pairs.push_back(iter->second->id_view);
			}
		}
	}
	cout << "size pairs: " << pairs.size() << endl;

	// perform putative matching
	PairWiseMatches map_putativeMatches;
	map<pair<size_t, size_t>, map<size_t, int>> featDist;
	hulo::matchAKAZEToQuery(mSfmData, mMatchDir, workingDir, pairs, indQueryFile, mSecondTestRatio, map_putativeMatches, featDist);
	// the indices in above image is based on vec_fileNames, not Viewf from sfm_data!!!
	double endMatch = (double) getTickCount();

	// remove putative matches with fewer points
	for (auto iterMatch = map_putativeMatches.cbegin(); iterMatch != map_putativeMatches.cend();) {
		if (iterMatch->second.size() < MINUM_NUMBER_OF_POINT_PUTATIVE_MATCH) {
			map_putativeMatches.erase(iterMatch++);
		} else {
			++iterMatch;
		}
	}
	cout << "number of putative matches : " << map_putativeMatches.size() << endl;

	// exit if found no match
	if (map_putativeMatches.size() == 0) {
		cout << "Not enough putative matches" << endl;

		// remove added image from sfm_data
		mSfmData.views.erase(indQueryFile);
		mRegionsProvider->regions_per_view.erase(indQueryFile);

		cout << "Load SfM views : " << (loadSfmViews - startQuery) / getTickFrequency() << " s\n";
		cout << "Select views by iBeacon : " << (selectViewsByBeacon - loadSfmViews) / getTickFrequency() << " s\n";
		cout << "Select views by BoW : " << (selectViewsByBow - selectViewsByBeacon) / getTickFrequency() << " s\n";
		cout << "Load image describer : " << (loadImageDescriber - selectViewsByBow) / getTickFrequency() << " s\n";
		cout << "Generate UUID : " << (generateUUID - loadImageDescriber) / getTickFrequency() << " s\n";
		cout << "Extract feature from query image : " << (endFeat - generateUUID) / getTickFrequency() << " s\n";
		cout << "Putative matching : " << (endMatch - endFeat) / getTickFrequency() << " s\n";

		return result;
	}

	//// compute geometric matches
	PairWiseMatches map_geometricMatches;
	hulo::geometricMatch(mSfmData, mRegionsProvider, map_putativeMatches,
					map_geometricMatches, mRansacRound, mRansacPrecision, mGuidedMatching);

	double endGMatch = (double) getTickCount();
	cout << endl << "number of geometric matches : " << map_geometricMatches.size() << endl;

	// exit if found no match
	if (map_geometricMatches.size() == 0) {
		cout << "Not enough geometric matches" << endl;

		// remove added image from sfm_data
		mSfmData.views.erase(indQueryFile);
		mRegionsProvider->regions_per_view.erase(indQueryFile);

		cout << "Load SfM views : " << (loadSfmViews - startQuery) / getTickFrequency() << " s\n";
		cout << "Select views by iBeacon : " << (selectViewsByBeacon - loadSfmViews) / getTickFrequency() << " s\n";
		cout << "Select views by BoW : " << (selectViewsByBow - selectViewsByBeacon) / getTickFrequency() << " s\n";
		cout << "Load image describer : " << (loadImageDescriber - selectViewsByBow) / getTickFrequency() << " s\n";
		cout << "Generate UUID : " << (generateUUID - loadImageDescriber) / getTickFrequency() << " s\n";
		cout << "Extract feature from query image : " << (endFeat - generateUUID) / getTickFrequency() << " s\n";
		cout << "Putative matching : " << (endMatch - endFeat) / getTickFrequency() << " s\n";
		cout << "Geometric matching : " << (endGMatch - endMatch) / getTickFrequency() << " s\n";

		return result;
	}

	shared_ptr<Matches_Provider> matches_provider = make_shared<Matches_Provider>();
	matches_provider->_pairWise_matches = map_geometricMatches;

	// build list of tracks
	cout << "Match provider size " << matches_provider->_pairWise_matches.size() << endl;

	// intersect tracks from query image to reconstructed 3D
	// any 2D pts that have been matched to more than one 3D point will be removed
	// in matchProviderToMatchSet
	cout << "map to 3D size = " << mMapViewFeatTo3D.size() << endl;
	QFeatTo3DFeat mapFeatTo3DFeat;
	hulo::matchProviderToMatchSet(matches_provider, mMapViewFeatTo3D, mapFeatTo3DFeat, featDist);
	cout << "mapFeatTo3DFeat size = " << mapFeatTo3DFeat.size() << endl;

	// matrices for saving 2D points from query image and 3D points from reconstructed model
	Image_Localizer_Match_Data resection_data;
	resection_data.pt2D.resize(2, mapFeatTo3DFeat.size());
	resection_data.pt3D.resize(3, mapFeatTo3DFeat.size());

	// get intrinsic
	const Intrinsics::const_iterator iterIntrinsic_I = mSfmData.GetIntrinsics().find(0);
	Pinhole_Intrinsic *cam_I = dynamic_cast<Pinhole_Intrinsic*>(iterIntrinsic_I->second.get());

	// copy data to matrices
	size_t cpt = 0;
	for (QFeatTo3DFeat::const_iterator iterfeatId = mapFeatTo3DFeat.begin();
			iterfeatId != mapFeatTo3DFeat.end(); ++iterfeatId, ++cpt) {

		// copy 3d location
		resection_data.pt3D.col(cpt) = mSfmData.GetLandmarks().at(iterfeatId->second).X;

		// copy 2d location
		const Vec2 feat = { qFeatLoc[iterfeatId->first].first,
				qFeatLoc[iterfeatId->first].second };
		resection_data.pt2D.col(cpt) = cam_I->get_ud_pixel(feat);
	}
	cout << "cpt = " << cpt << endl;

	// perform resection
	double errorMax = numeric_limits<double>::max();
	bool bResection = false;
	if (cpt > MINUM_NUMBER_OF_POINT_RESECTION) {
		geometry::Pose3 pose;
		bResection = sfm::SfM_Localizer::Localize(make_pair(imageHeight, imageWidth), cam_I, resection_data, pose);
	}
	double endResection = (double) getTickCount();

	if (!bResection || resection_data.vec_inliers.size() <= MINUM_NUMBER_OF_INLIER_RESECTION) {
		cout << "Fail to estimate camera matrix" << endl;

		// remove added image from sfm_data
		mSfmData.views.erase(indQueryFile);
		mRegionsProvider->regions_per_view.erase(indQueryFile);

		cout << "#inliers = " << resection_data.vec_inliers.size() << endl;

		cout << "Load SfM views : " << (loadSfmViews - startQuery) / getTickFrequency() << " s\n";
		cout << "Select views by iBeacon : " << (selectViewsByBeacon - loadSfmViews) / getTickFrequency() << " s\n";
		cout << "Select views by BoW : " << (selectViewsByBow - selectViewsByBeacon) / getTickFrequency() << " s\n";
		cout << "Load image describer : " << (loadImageDescriber - selectViewsByBow) / getTickFrequency() << " s\n";
		cout << "Generate UUID : " << (generateUUID - loadImageDescriber) / getTickFrequency() << " s\n";
		cout << "Extract feature from query image : " << (endFeat - generateUUID) / getTickFrequency() << " s\n";
		cout << "Putative matching : " << (endMatch - endFeat) / getTickFrequency() << " s\n";
		cout << "Geometric matching : " << (endGMatch - endMatch) / getTickFrequency() << " s\n";
		cout << "PnP : " << (endResection - endGMatch) / getTickFrequency() << " s\n";

		return result;
	}

	Mat3 K_, R_;
	Vec3 t_, t_out;
	KRt_From_P(resection_data.projection_matrix, &K_, &R_, &t_);
	t_out = -R_.transpose() * t_;
	cout << "#inliers = " << resection_data.vec_inliers.size() << endl;
	cout << "P = " << endl << resection_data.projection_matrix << endl;
	cout << "K = " << endl << K_ << endl;
	cout << "R = " << endl << R_ << endl;
	cout << "t = " << endl << t_ << endl;
	cout << "-R't = " << endl << -R_.transpose() * t_ << endl;

	cv::Mat t_out_h(4, 1, CV_64F);
	t_out_h.at<double>(0) = t_out[0];
	t_out_h.at<double>(1) = t_out[1];
	t_out_h.at<double>(2) = t_out[2];
	t_out_h.at<double>(3) = 1.0;
	cv::Mat global_t_out = mA * t_out_h;
	cout << "global t = " << endl << global_t_out << endl;
	for (int i=0; i<3; i++) {
		result.push_back(global_t_out.at<double>(i));
	}

	cv::Mat cvR_;
	cv::eigen2cv(R_, cvR_);
	cv::Mat global_R = mA(cv::Rect(0,0,3,3)) * cvR_;
	cout << "global R = " << endl << global_R << endl;
	for (int i=0; i<3; i++) {
		for (int j=0; j<3; j++) {
			result.push_back(global_R.at<double>(i,j));
		}
	}

	// remove added image from sfm_data
	mSfmData.views.erase(indQueryFile);
	mRegionsProvider->regions_per_view.erase(indQueryFile);

	double endQuery = (double) getTickCount();
	cout << "Load SfM views : " << (loadSfmViews - startQuery) / getTickFrequency() << " s\n";
	cout << "Select views by iBeacon : " << (selectViewsByBeacon - loadSfmViews) / getTickFrequency() << " s\n";
	cout << "Select views by BoW : " << (selectViewsByBow - selectViewsByBeacon) / getTickFrequency() << " s\n";
	cout << "Load image describer : " << (loadImageDescriber - selectViewsByBow) / getTickFrequency() << " s\n";
	cout << "Generate UUID : " << (generateUUID - loadImageDescriber) / getTickFrequency() << " s\n";
	cout << "Extract feature from query image : " << (endFeat - generateUUID) / getTickFrequency() << " s\n";
	cout << "Putative matching : " << (endMatch - endFeat) / getTickFrequency() << " s\n";
	cout << "Geometric matching : " << (endGMatch - endMatch) / getTickFrequency() << " s\n";
	cout << "PnP : " << (endResection - endGMatch) / getTickFrequency() << " s\n";
	cout << "Transform coordinate : " << (endQuery - endResection) / getTickFrequency() << " s\n";
	cout << "Total time: " << (endQuery - startQuery) / getTickFrequency() << " s\n";

	cout << "complete!" << endl;

	return result;
}
