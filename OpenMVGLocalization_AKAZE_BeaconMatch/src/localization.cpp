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

#include <opencv2/opencv.hpp>
#include <Eigen/Core>
#include <sfm/sfm_data.hpp>
#include <sfm/sfm_data_io.hpp>
#include <openMVG/features/regions.hpp>
#include <openMVG/features/regions_factory.hpp>
#include <openMVG/sfm/pipelines/sfm_robust_model_estimation.hpp>
#include <openMVG/sfm/pipelines/sfm_matches_provider.hpp>
#include <openMVG/sfm/pipelines/localization/SfM_Localizer.hpp>

#include "BeaconUtils.h"
#include "DenseLocalFeatureWrapper.h"
#include "BowDenseFeatureSettings.h"
#include "PcaWrapper.h"
#include "BoFSpatialPyramids.h"
#include "AKAZEOption.h"
#include "AKAZEOpenCV.h"
#include "SfMDataUtils.h"
#include "BoFUtils.h"
#include "MatchUtils.h"
#include "FileUtils.h"

using namespace std;
using namespace cv;
using namespace Eigen;
using namespace openMVG;
using namespace openMVG::sfm;
using namespace openMVG::features;
using namespace openMVG::cameras;
using namespace openMVG::matching;
using namespace hulo;

#define THRESHOLD_CALC_BEACON_COOCCURRENCE 10
#define MINUM_NUMBER_OF_SAME_BEACONS 2

#define MINUM_NUMBER_OF_POINT_PUTATIVE_MATCH 16
#define MINUM_NUMBER_OF_POINT_RESECTION 8
#define MINUM_NUMBER_OF_INLIER_RESECTION 10

const String keys =
		"{h help 			|| print this message}"
		"{@queryImage		||Query image file/folder}"
		"{@sfmData			||Folder that has sfm_data.json}"
		"{@matchdir			||Folder of descriptor/feature/match files}"
		"{@outputFolder		||Output folder for keeping result}"
		"{f fDistRatio		|0.6|Matching ratio}"
		"{r ransacRound		|200|Round for RANSAC geometric matching}"
		"{w writematch		|false|Write match files or not}"
		"{b beaconSetting	||Beacon setting file for reconstructed model}"
		"{e beaconQFile		||Folder containing csv files for query images}"
		"{k knnrssi			|0|Nearest k beacon RSSI signals}"
		"{kb knnbow			|0|Nearest k BOW features}"
		"{c cooccurrssi		|1.0|Ratio for accepting cooccurence}"
		"{x cenLocX			|0.0|X location for dead reckoning}"
		"{y cenLocY			|0.0|Y location for dead reckoning}"
		"{z cenLocZ			|0.0|Z location for dead reckoning}"
		"{d cenRadius		|-1.0|Radius for dead reckoning (-1.0 means infinite radius)}"
		"{a bowModelFile	||BOW model file}"
		"{p pcaModelFile	||PCA model file}"
		"{i locEvryNFrame	|1|Localize every i^th frame}"
		"{g geomLimit		|4.0|geometric matching precision}"
		"{v skipBeaconView	|1|select close iBeacon with this interval}"
		"{n normBeaconApproach |0|normalization approach for beacon [0 for max, 1 for median]}";

int main(int argc, char **argv) {
	// Parse arguments
	CommandLineParser parser(argc, argv, keys);
	parser.about("Execute localization for query image or folder");
	if (argc < 2) {
		parser.printMessage();
		return 1;
	}
	if (parser.has("h")) {
		parser.printMessage();
		return 1;
	}
	string sQueryImg = parser.get<string>(0);
	string sSfMDir = parser.get<string>(1);
	string sMatchesDir = parser.get<string>(2);
	string sOutputFolder = parser.get<string>(3);
	float fDistRatio = parser.get<float>("f");
	int ransacRound = parser.get<int>("r");
	bool bWriteMatchFile = parser.get<bool>("w");
	string beaconSet = parser.get<string>("b");
	string beaconQFile = parser.get<string>("e");
	int knnrssi = parser.get<int>("k");
	int knnbow = parser.get<int>("kb");
	float cooc = parser.get<float>("c");
	double cenLocX = parser.get<double>("x");
	double cenLocY = parser.get<double>("y");
	double cenLocZ = parser.get<double>("z");
	double cenRadius = parser.get<double>("d");
	string bowFile = parser.get<string>("a");
	string pcaFile = parser.get<string>("p");
	int locEvryNFrame = parser.get<int>("i");
	double geomPrec = parser.get<double>("g");
	int skipBeaconView = parser.get<int>("v");
	int normBeaonTypeInt = parser.get<int>("n");
	bool bGuided_matching = false;

    if (!parser.check() || sQueryImg.size()==0 || sSfMDir.size()==0
    		|| sMatchesDir.size()==0 || sOutputFolder.size()==0) {
        parser.printMessage();
        return 1;
    }

	cout << "Start localizing input image." << endl;
	double start = (double) getTickCount();

	NormBeaconSignalType normBeaonType = NormBeaconSignalType::UNKNOWN;
	if (normBeaonTypeInt==0) {
		normBeaonType = NormBeaconSignalType::MAX;
	} else if (normBeaonTypeInt==1) {
		normBeaonType = NormBeaconSignalType::MEDIAN;
	}

	Vec3 cenLoc;
	cenLoc << cenLocX, cenLocY, cenLocZ;
	cout << "center location : " << cenLoc << endl;
	cout << "query image : " << sQueryImg << endl;
	cout << "sfm directory : " << sSfMDir << endl;
	cout << "match directory : " << sMatchesDir << endl;

	vector<string> listImage;
	if (stlplus::is_file(sQueryImg)) {
		string ext = stlplus::extension_part(sQueryImg);
		if (ext.compare("jpg") && ext.compare("JPG") && ext.compare("jpeg")
				&& ext.compare("JPEG") && ext.compare("png")
				&& ext.compare("PNG")) {
			cout << "Input image is not JPEG or PNG file" << endl;
			return EXIT_FAILURE;
		}

		listImage.push_back(sQueryImg);
	} else if (stlplus::is_folder(sQueryImg)) {
		vector<string> listImageTmp = stlplus::folder_files(sQueryImg);
		sort(listImageTmp.begin(), listImageTmp.end());

		for (vector<string>::const_iterator iterS = listImageTmp.begin();
				iterS != listImageTmp.end(); ++iterS) {

			string ext = stlplus::extension_part(*iterS);
			if (!(ext.compare("jpg") && ext.compare("JPG")
					&& ext.compare("jpeg") && ext.compare("JPEG")
					&& ext.compare("png") && ext.compare("PNG"))) {
				listImage.push_back(
						stlplus::create_filespec(sQueryImg,
								stlplus::basename_part(*iterS), ext));
			}
		}
		if (listImage.size()==0) {
			cout << "JPEG or PNG file is not found in input image directory" << endl;
			return EXIT_FAILURE;
		}
	}

	// check arguments
	CV_Assert((beaconSet != "" && beaconQFile != "" && knnrssi > 0 && skipBeaconView > 0) || cenRadius>=0.0);

	// load sfm_data of reconstructed 3D
	//// read SfM_Data scene
	const string sSfM_data = stlplus::create_filespec(sSfMDir, "sfm_data", "json");
	cout << "Reading sfm_data.json file : " << sSfM_data << endl;
	SfM_Data sfm_data;
	if (!Load(sfm_data, sSfM_data,
			ESfM_Data(VIEWS | INTRINSICS | EXTRINSICS | STRUCTURE))) {
		cerr << endl << "The input sfm_data.json file \"" << sSfM_data << "\" cannot be read." << endl;
		return EXIT_FAILURE;
	}
	double endReadSfm_data = (double) getTickCount();

	// get map of ViewID, FeatID to 3D point
	MapViewFeatTo3D mapViewFeatTo3D;
	hulo::structureToMapViewFeatTo3D(sfm_data.GetLandmarks(), mapViewFeatTo3D);

	// read beacon data if selected to be used
	// read beacon info to get
	// 1. (major,minor)->rssi vector index map
	// 2. matrix of rssi
	map<pair<size_t, size_t>, size_t> beaconIndex;
	cv::Mat rssi;
	if (beaconSet != "" && beaconQFile != "" && knnrssi > 0) {
		hulo::parseBeaconList(beaconSet, beaconIndex, rssi);
	}

	// prepare Bag of Words model
	DenseLocalFeatureWrapper *denseLocalFeatureWrapper =
			new DenseLocalFeatureWrapper(hulo::BOW_DENSE_LOCAL_FEATURE_TYPE,
					hulo::RESIZED_IMAGE_SIZE, hulo::USE_COLOR_FEATURE,
					hulo::L1_SQUARE_ROOT_LOCAL_FEATURE);
	PcaWrapper *pcaWrapper;
	BoFSpatialPyramids *bofPyramids;
	if (pcaFile != "") {
		cv::FileStorage fsRead(pcaFile, cv::FileStorage::READ);
		pcaWrapper = new PcaWrapper();
		pcaWrapper->read(fsRead.root());
	}
	if (bowFile != "") {
		cv::FileStorage fsRead(bowFile, cv::FileStorage::READ);
		bofPyramids = new BoFSpatialPyramids();
		bofPyramids->read(fsRead.root());
	}

	if (locEvryNFrame <= 0) {
		cout << "Number of frame set to skip is invalid. Reset to localize every frame" << endl;
		locEvryNFrame = 1;
	}

	// construct feature providers for geometric matches
	unique_ptr<Regions> regions;
	regions.reset(new AKAZE_Binary_Regions);
	shared_ptr<Regions_Provider> regions_provider = make_shared<Regions_Provider>();
	if (!regions_provider->load(sfm_data, sMatchesDir, regions)) {
		cerr << "Cannot construct regions providers," << endl;
		return EXIT_FAILURE;
	}

	// iterate thru image
	int imageNumber = 0;
	int matchNextNFrame = 0;
	for (vector<string>::const_iterator iterFile = listImage.begin();
			iterFile != listImage.end(); ++iterFile) {
		imageNumber++;

		// to accelerate matching process in video case,
		// skip matching some frames. If a good localization is found,
		// then localize all next locEvryNFrame frames.
		if (imageNumber % locEvryNFrame == 0) {
			;
		} else if (matchNextNFrame <= 0) {
			continue;
		} else if (matchNextNFrame > 0) {
			matchNextNFrame--;
		}

		double imstart = (double) getTickCount();
		sQueryImg = *iterFile;

		string sQueryImgNoExt = sQueryImg.substr(0, sQueryImg.find_last_of('.'));
		string sQueryImgMatchDir = stlplus::create_filespec(sMatchesDir, stlplus::basename_part(sQueryImg));
		if (stlplus::folder_exists(sQueryImgMatchDir)) {
			stlplus::folder_delete(sQueryImgMatchDir, true);
		}
		stlplus::folder_create(sQueryImgMatchDir);

		///////////////////////////////////////
		// Extract features from query image //
		///////////////////////////////////////
		cout << "Extract features from query image" << endl;

		// create and save image describer file
		string sImageDesc = stlplus::create_filespec(sMatchesDir,
				"image_describer.txt");
		AKAZEOption akazeOption = AKAZEOption();
		akazeOption.read(sImageDesc);

		size_t qImgH, qImgW;
		vector<pair<float, float>> qFeatLoc;
		unique_ptr<Regions> queryRegions = hulo::extractAKAZESingleImg(sQueryImg, sQueryImgMatchDir, akazeOption, qFeatLoc, qImgW, qImgH);
		double endFeat = (double) getTickCount();
		/////////////////////////////////////////
		// Match features to those in SfM file //
		/////////////////////////////////////////

		// restrict sfm location search
		// return localViews with viewID of views that are close to image
		// this must be done before adding query image to sfm_data
		set<IndexT> localViews;
		if (beaconSet != "" && beaconQFile != "" && knnrssi > 0
				&& skipBeaconView > 0) {
			hulo::getBeaconView(beaconQFile, stlplus::basename_part(sQueryImg),
					beaconIndex, rssi, localViews,
					THRESHOLD_CALC_BEACON_COOCCURRENCE, MINUM_NUMBER_OF_SAME_BEACONS,
					knnrssi, cooc, sfm_data, skipBeaconView, normBeaonType);
			cout << "number of selected local views by beacon : " << localViews.size() << endl;
		} else {
			if (cenRadius>0) {
				hulo::getLocalViews(sfm_data, cenLoc, cenRadius, localViews);
				cout << "number of selected local views by center location : " << localViews.size() << endl;
			} else {
				for (Views::const_iterator iter = sfm_data.views.begin(); iter != sfm_data.views.end(); iter++) {
					if (sfm_data.poses.find(iter->second->id_pose) != sfm_data.poses.end()) {
						localViews.insert(iter->first);
					}
				}
				cout << "number of views in SfM data : " << localViews.size() << endl;
			}
		}
		double endBeacon = (double) getTickCount();

		if (bowFile != "" && knnbow > 0 && localViews.size() > knnbow) {
			cv::Mat bofMat;
			if (pcaFile != "") {
				vector<cv::KeyPoint> keypoints;
				cv::Mat descriptors =
						denseLocalFeatureWrapper->calcDenseLocalFeature(
								sQueryImg, keypoints);
				cv::Mat pcaDescriptors = pcaWrapper->calcPcaProject(
						descriptors);
				bofMat = bofPyramids->calcBoF(pcaDescriptors, keypoints);
			} else {
				std::vector<cv::KeyPoint> keypoints;
				cv::Mat descriptors =
						denseLocalFeatureWrapper->calcDenseLocalFeature(
								sQueryImg, keypoints);
				bofMat = bofPyramids->calcBoF(descriptors, keypoints);
			}
			set<openMVG::IndexT> bofSelectedViews;
			hulo::selectViewByBoF(bofMat, sMatchesDir, localViews, sfm_data, knnbow, bofSelectedViews);
			localViews.clear();
			localViews = set<openMVG::IndexT>(bofSelectedViews);
			cout << "number of selected local views by bow : " << localViews.size() << endl;
		}

		// add query image to sfm_data
		IndexT indQueryFile = sfm_data.views.rbegin()->second->id_view + 1; // set index of query file to (id of last view of sfm data) + 1
		cout << "query image size : " << qImgW << " x " << qImgH << endl;
		sfm_data.views.insert(
				make_pair(indQueryFile,
						make_shared<View>(stlplus::basename_part(sQueryImg),
								indQueryFile, 0, 0, qImgW, qImgH)));
		regions_provider->regions_per_view[indQueryFile] = std::move(queryRegions);
		cout << "image # " << imageNumber << "/" << listImage.size() << endl;
		cout << "query ind: " << indQueryFile << endl;
		cout << "fileLoc:" << sfm_data.views.at(indQueryFile)->s_Img_path
				<< endl;
		cout << "fileDir: " << sQueryImg << endl;
		cout << "sfm_data: " << sSfMDir << endl;

		// generate pairs
		vector<size_t> pairs;
		for (Views::const_iterator iter = sfm_data.views.begin();
				iter != sfm_data.views.end(); iter++) {
			if (localViews.find(iter->second->id_view) != localViews.end()) {
				pairs.push_back(iter->second->id_view);
			}
		}
		cout << "size pairs: " << pairs.size() << endl;

		// perform putative matching
		PairWiseMatches map_putativeMatches;
		map<pair<size_t, size_t>, map<size_t, int>> featDist;
		hulo::matchAKAZEToQuery(sfm_data, sMatchesDir, sQueryImgMatchDir, pairs, indQueryFile, fDistRatio, map_putativeMatches, featDist);
		if (bWriteMatchFile) {
			hulo::exportPairWiseMatches(map_putativeMatches,
					stlplus::create_filespec(sQueryImgMatchDir,
							"matches.putativeQ.txt")); // export result
		}
		// the indices in above image is based on vec_fileNames, not Viewf from sfm_data!!!
		double endMatch = (double) getTickCount();

		// remove putative matches with fewer points
		for (auto iterMatch = map_putativeMatches.cbegin();
				iterMatch != map_putativeMatches.cend();) {
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
			sfm_data.views.erase(indQueryFile);
			regions_provider->regions_per_view.erase(indQueryFile);

			// print time
			double end = (double) getTickCount();
			cout << "Read sfm_data: "
					<< (endReadSfm_data - start) / getTickFrequency() << " s\n";
			cout << "Load img + cal feat: "
					<< (endFeat - imstart) / getTickFrequency() << " s\n";
			cout << "Calculate close beacon: "
					<< (endBeacon - endFeat) / getTickFrequency() << " s\n";
			cout << "Putative matching: "
					<< (end - endBeacon) / getTickFrequency() << " s\n";
			cout << "Total image time: " << (end - imstart) / getTickFrequency()
					<< " s\n";
			cout << "Total time: " << (end - start) / getTickFrequency()
					<< " s\n";

			continue;
		}

		//// compute geometric matches
		PairWiseMatches map_geometricMatches;
		geometricMatch(sfm_data, regions_provider, map_putativeMatches,
				map_geometricMatches, ransacRound, geomPrec, bGuided_matching);
		if (bWriteMatchFile) {
			hulo::exportPairWiseMatches(map_geometricMatches,
					stlplus::create_filespec(sMatchesDir, "matches.fQ.txt")); // export result
		}

		double endGMatch = (double) getTickCount();
		cout << "number of geometric matches : " << map_geometricMatches.size() << endl;

		shared_ptr<Matches_Provider> matches_provider = make_shared<
				Matches_Provider>();
		matches_provider->_pairWise_matches = map_geometricMatches;

		// build list of tracks
		cout << "Match provider size "
				<< matches_provider->_pairWise_matches.size() << endl;

		// intersect tracks from query image to reconstructed 3D
		// any 2D pts that have been matched to more than one 3D point will be removed
		// in matchProviderToMatchSet
		cout << "map to 3D size = " << mapViewFeatTo3D.size() << endl;
		QFeatTo3DFeat mapFeatTo3DFeat;
		hulo::matchProviderToMatchSet(matches_provider, mapViewFeatTo3D, mapFeatTo3DFeat, featDist);
		cout << "mapFeatTo3DFeat size = " << mapFeatTo3DFeat.size() << endl;

		// matrices for saving 2D points from query image and 3D points from reconstructed model
		Image_Localizer_Match_Data resection_data;
		resection_data.pt2D.resize(2, mapFeatTo3DFeat.size());
		resection_data.pt3D.resize(3, mapFeatTo3DFeat.size());

		// get intrinsic
		const Intrinsics::const_iterator iterIntrinsic_I =
				sfm_data.GetIntrinsics().find(0);
		Pinhole_Intrinsic *cam_I =
				dynamic_cast<Pinhole_Intrinsic*>(iterIntrinsic_I->second.get());

		// copy data to matrices
		size_t cpt = 0;
		for (QFeatTo3DFeat::const_iterator iterfeatId = mapFeatTo3DFeat.begin();
				iterfeatId != mapFeatTo3DFeat.end(); ++iterfeatId, ++cpt) {

			// copy 3d location
			resection_data.pt3D.col(cpt) = sfm_data.GetLandmarks().at(iterfeatId->second).X;

			// copy 2d location
			const Vec2 feat = { qFeatLoc[iterfeatId->first].first,
					qFeatLoc[iterfeatId->first].second };
			resection_data.pt2D.col(cpt) = cam_I->get_ud_pixel(feat);
		}
		cout << "cpt = " << cpt << endl;

		// perform resection
		bool bResection = false;
		if (cpt > MINUM_NUMBER_OF_POINT_RESECTION) {
			geometry::Pose3 pose;
			bResection = sfm::SfM_Localizer::Localize(make_pair(qImgH, qImgW), cam_I, resection_data, pose);
		}

		if (!bResection || resection_data.vec_inliers.size() <= MINUM_NUMBER_OF_INLIER_RESECTION) {
			cout << "Fail to estimate camera matrix" << endl;

			// remove added image from sfm_data
			sfm_data.views.erase(indQueryFile);
			regions_provider->regions_per_view.erase(indQueryFile);

			// print time
			double end = (double) getTickCount();
			cout << "#inliers = " << resection_data.vec_inliers.size() << endl;
			cout << "Read sfm_data: "
					<< (endReadSfm_data - start) / getTickFrequency() << " s\n";
			cout << "Load img + cal feat: "
					<< (endFeat - imstart) / getTickFrequency() << " s\n";
			cout << "Calculate close beacon: "
					<< (endBeacon - endFeat) / getTickFrequency() << " s\n";
			cout << "Putative matching: "
					<< (endMatch - endBeacon) / getTickFrequency() << " s\n";
			cout << "Geometric matching: "
					<< (endGMatch - endMatch) / getTickFrequency() << " s\n";
			cout << "PnP: " << (end - endGMatch) / getTickFrequency() << " s\n";
			cout << "Total image time: " << (end - imstart) / getTickFrequency()
					<< " s\n";
			cout << "Total time: " << (end - start) / getTickFrequency()
					<< " s\n";

			continue;
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

		double end = (double) getTickCount();
		cout << "complete" << endl;

		cout << "Read sfm_data: "
				<< (endReadSfm_data - start) / getTickFrequency() << " s\n";
		cout << "Load img + cal feat: "
				<< (endFeat - imstart) / getTickFrequency() << " s\n";
		cout << "Calculate close beacon: "
				<< (endBeacon - endFeat) / getTickFrequency() << " s\n";
		cout << "Putative matching: "
				<< (endMatch - endBeacon) / getTickFrequency() << " s\n";
		cout << "Geometric matching: "
				<< (endGMatch - endMatch) / getTickFrequency() << " s\n";
		cout << "PnP: " << (end - endGMatch) / getTickFrequency() << " s\n";
		cout << "Total image time: " << (end - imstart) / getTickFrequency()
				<< " s\n";
		cout << "Total time: " << (end - start) / getTickFrequency() << " s\n";

		if (sOutputFolder.length() > 0) {
			ofstream os(
					stlplus::create_filespec(sOutputFolder,
							stlplus::basename_part(sQueryImg), "json"));
			if (os.is_open()) {

				// set output format for matrix and vector
				IOFormat matFormat(6, 0, ",", ",\n", "[", "]", "[", "]");
				IOFormat vecFormat(6, 0, ",", ",\n", "", "", "[", "]");

				// output json
				os << "{" << endl;
				os << "\t\"filename\": \"" << sQueryImg << "\"," << endl;
				os << "\t\"sfm_data\": \"" << sSfM_data << "\"," << endl;
				os << "\t\"matches_dir\": \"" << sMatchesDir << "\"," << endl;
				os << "\t\"K\": " << K_.format(matFormat) << "," << endl;
				os << "\t\"R\": " << R_.format(matFormat) << "," << endl;
				os << "\t\"t\": " << t_out.format(vecFormat) << "," << endl;
				os << "\t\"pair\": [";
				for (vector<size_t>::const_iterator iterW = resection_data.vec_inliers.begin();
						iterW != resection_data.vec_inliers.end(); ++iterW) {
					pair<size_t, size_t> pairM = *(mapFeatTo3DFeat.begin()
							+ *iterW);
					os << "[" << pairM.first << "," << pairM.second << "]";
					if (iterW + 1 != resection_data.vec_inliers.end()) {
						os << ",";
					}
				}

				os << "]" << endl;
				os << "}" << endl;
				os.close();
			} else {
				cerr << "cannot write out result to "
						<< stlplus::create_filespec(sOutputFolder,
								stlplus::basename_part(sQueryImg), "json")
						<< endl;
			}
		}

		// remove added image from sfm_data
		sfm_data.views.erase(indQueryFile);
		regions_provider->regions_per_view.erase(indQueryFile);

		// if localization reach this part, it means a localization is found,
		// hence we will localize locEvryNFrame frames after this frame
		matchNextNFrame = locEvryNFrame - 1;
	}
}
