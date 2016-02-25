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

#include <FileUtils.h>
#include <opencv2/opencv.hpp>
#include <sfm/sfm_data_io.hpp>
#include <openMVG/sfm/sfm_data.hpp>

#include "BowDenseFeatureSettings.h"
#include "DenseLocalFeatureWrapper.h"
#include "PcaWrapper.h"
#include "BoFSpatialPyramids.h"

#include <dirent.h>

using namespace std;
using namespace openMVG::sfm;
using namespace hulo;

const cv::String keys =
		"{h help 	   || print this message}"
		"{@inputDir    || input directory}"
		"{@bowFile     || output BoW file}"
		"{p pcaFile    || output PCA file (optional)}";

////////////	start settings			///////
static const int PCA_TRAIN_FEATURE_NUM = 300000;
static const int PCA_TRAIN_FEATURE_NUM_PER_IMAGE = 100;
static const int KMEANS_TRAIN_FEATURE_NUM = 300000;
static const int KMEANS_TRAIN_FEATURE_NUM_PER_IMAGE = 100;
static const int K = 100;
static const int PCA_DIM = 32;
static const BoFSpatialPyramids::NormBofFeatureType NORM_BOF_FEATURE_TYPE = BoFSpatialPyramids::L1_NORM_SQUARE_ROOT;
static const bool USE_SPATIAL_PYRAMID = true;
static const int PYRAMID_LEVEL = 2;
////////////	end settings			///////

//
// find sfm_data.json in matches folder to list input images
//
void readSfmDataFiles(const string dir, vector<string>& sfmDataFiles) {
	if (!stlplus::is_folder(dir)) {
		return;
	}

	vector<string> files = stlplus::folder_files(dir);
	for (int i = 0; i < files.size(); i++) {
		vector<string> parentDirs = stlplus::folder_elements(dir);
		if (parentDirs.size()>0 && parentDirs[parentDirs.size()-1]=="matches" && stlplus::filename_part(files[i])=="sfm_data.json") {
			sfmDataFiles.push_back(stlplus::create_filespec(dir, files[i]));
		}
	}
	vector<string> parentDirs = stlplus::folder_elements(dir);
	if (parentDirs.size()>0 && parentDirs[parentDirs.size()-1]!="matches") {
		/*
		// folders named as number cannot be listed by stlplus function
		vector<string> subDirs = stlplus::folder_subdirectories(dir);
		for (int i = 0; i < subDirs.size(); i++) {
			readSfmDataFiles(stlplus::create_filespec(dir, subDirs[i]), sfmDataFiles);
		}
		*/
		// list folders by using Linux function
		DIR *dirObj = opendir(dir.c_str());
		for (dirent* dirEntry = readdir(dirObj); dirEntry; dirEntry = readdir(dirObj)) {
			std::string strEntry = dirEntry->d_name;
			if (strEntry.compare(".")!=0 && strEntry.compare("..")!=0) {
				string strEntryPath = stlplus::create_filespec(dir, strEntry);
				if (stlplus::is_folder(strEntryPath)) {
					readSfmDataFiles(strEntryPath, sfmDataFiles);
				}
			}
		}
	}
}

cv::Mat getRandomTrainFeatures(
		DenseLocalFeatureWrapper &denseLocalFeatureWrapper,
		const vector<string> &images, int trainFeatureNum,
		int trainFeatureNumPerImage) {
	cv::RNG rng;

	int trainImageNum = trainFeatureNum / trainFeatureNumPerImage;
	cv::Mat features = cv::Mat::zeros(trainFeatureNumPerImage * trainImageNum,
			denseLocalFeatureWrapper.descriptorSize(), CV_32F);

#pragma omp parallel for
	for (int i = 0; i < trainImageNum; i++) {
		float randImage = rng.uniform(0.f, 1.f);
		cout << "extract feature from " << images[images.size() * randImage] << endl;

		std::vector<cv::KeyPoint> keypoints;
		cv::Mat descriptors = denseLocalFeatureWrapper.calcDenseLocalFeature(
				images[images.size() * randImage], keypoints);

		if (descriptors.empty()) {
			continue;
		}
		for (int j = 0; j < trainFeatureNumPerImage; j++) {
			float randFeature = rng.uniform(0.f, 1.f);
			int k = descriptors.rows * randFeature;

			for (int l = 0; l < descriptors.cols; l++) {
				features.at<float>(i * trainFeatureNumPerImage + j, l) =
						descriptors.at<float>(k, l);
			}
		}
		cout << "Number of images extracted feature for random training : " << i
				<< " / " << trainImageNum << endl;
	}

	return features;
}

int main(int argc, char* argv[]) {
	cv::CommandLineParser parser(argc, argv, keys);
	parser.about("Train Bag of Words Model");
	if (argc < 2) {
		parser.printMessage();
		return 1;
	}
	if (parser.has("h")) {
		parser.printMessage();
		return 1;
	}
	cv::String inputDir = parser.get<cv::String>(0);
	cv::String bowFile = parser.get<std::string>(1);
	cv::String pcaFile = parser.get<std::string>("p");

    if (!parser.check() || inputDir.size()==0 || bowFile.size()==0) {
        parser.printMessage();
        return 1;
    }

	vector<string> sfmDataFiles;
	readSfmDataFiles(inputDir, sfmDataFiles);
	cout << "number of sfm data files found : " << sfmDataFiles.size() << endl;

	vector<string> images;
	for (int i=0; i<sfmDataFiles.size(); i++) {
		SfM_Data sfm_data;
		if(!Load(sfm_data, sfmDataFiles[i], ESfM_Data(VIEWS|INTRINSICS))){
			cerr << "Cannot load " << sfmDataFiles[i] << endl;
			return EXIT_FAILURE;
		}
		for(Views::const_iterator iter = sfm_data.views.begin(); iter != sfm_data.views.end(); iter++){
			images.push_back(stlplus::create_filespec(sfm_data.s_root_path,stlplus::basename_part(iter->second->s_Img_path),stlplus::extension_part(iter->second->s_Img_path)));
		}
	}
	cout << "number of image files found : " << images.size() << endl;

	DenseLocalFeatureWrapper *denseLocalFeatureWrapper =
			new DenseLocalFeatureWrapper(hulo::BOW_DENSE_LOCAL_FEATURE_TYPE, hulo::RESIZED_IMAGE_SIZE,
					hulo::USE_COLOR_FEATURE, hulo::L1_SQUARE_ROOT_LOCAL_FEATURE);

	if (pcaFile != "") {
		cout << "Start getting features to train PCA...." << endl;
		cv::Mat pcaTrainFeatures = getRandomTrainFeatures(
				*denseLocalFeatureWrapper, images, PCA_TRAIN_FEATURE_NUM,
				PCA_TRAIN_FEATURE_NUM_PER_IMAGE);
		cout
				<< "End getting features to train PCA. Training feature matrix size is ("
				<< pcaTrainFeatures.rows << " x " << pcaTrainFeatures.cols
				<< ")" << endl;

		cout << "Start train PCA...." << endl;
		PcaWrapper *pcaWrapper = new PcaWrapper(pcaTrainFeatures, PCA_DIM);
		cout << "End train PCA." << endl;

		cout << "Start save PCA model...." << endl;
		{
			cv::FileStorage fsWrite(pcaFile, cv::FileStorage::WRITE);
			pcaWrapper->write(fsWrite);
			fsWrite.release();
		}
		cout << "End save PCA model." << endl;

		cout << "Reload PCA model from file...." << endl;
		{
			delete pcaWrapper;
			cv::FileStorage fsRead(pcaFile, cv::FileStorage::READ);
			pcaWrapper = new PcaWrapper();
			pcaWrapper->read(fsRead.root());
		}
		cout << "End reload PCA model from file." << endl;

		cout << "Start getting features to train kmeans...." << endl;
		cv::Mat kmeansTrainFeatures = getRandomTrainFeatures(
				*denseLocalFeatureWrapper, images, KMEANS_TRAIN_FEATURE_NUM,
				KMEANS_TRAIN_FEATURE_NUM_PER_IMAGE);
		cout << "End getting features to train k-means. Training feature matrix size is (" << kmeansTrainFeatures.rows << " x " << kmeansTrainFeatures.cols << ")" << endl;

		cout << "Start PCA projection of train kmeans Vector...." << endl;
		cv::Mat pcaKmeansTrainFeatures;
		pcaKmeansTrainFeatures = pcaWrapper->calcPcaProject(kmeansTrainFeatures);
		cout << "End PCA projection of train kmeans Vector...." << endl;

		cout << "Start train BoF...." << endl;
		BoFSpatialPyramids* bof = new BoFSpatialPyramids(pcaKmeansTrainFeatures, K, RESIZED_IMAGE_SIZE,
				NORM_BOF_FEATURE_TYPE, USE_SPATIAL_PYRAMID, PYRAMID_LEVEL);
		cout << "End train BoF." << endl;

		cout << "Start save BoF model...." << endl;
		{
			cv::FileStorage fsWrite(bowFile, cv::FileStorage::WRITE);
			bof->write(fsWrite);
			fsWrite.release();
		}
		cout << "End save BoF model." << endl;

		cout << "Reload BoF model from file...." << endl;
		{
			delete bof;
			cv::FileStorage fsRead(bowFile, cv::FileStorage::READ);
			bof = new BoFSpatialPyramids();
			bof->read(fsRead.root());
		}
		cout << "End reload BoF model from file." << endl;

		cout << "Start calculate BoF feature for all images." << endl;
		for (int i=0; i<sfmDataFiles.size(); i++) {
			SfM_Data sfm_data;
			if(!Load(sfm_data, sfmDataFiles[i], ESfM_Data(VIEWS|INTRINSICS))){
				cerr << "Cannot load " << sfmDataFiles[i] << endl;
				return EXIT_FAILURE;
			}

			std::vector<string> imageFiles;
			std::vector<string> bofFiles;
			for(Views::const_iterator iter = sfm_data.views.begin(); iter != sfm_data.views.end(); iter++){
				string imageFile = stlplus::create_filespec(sfm_data.s_root_path,stlplus::basename_part(iter->second->s_Img_path),stlplus::extension_part(iter->second->s_Img_path));
				string bofFile = stlplus::create_filespec(stlplus::folder_part(sfmDataFiles[i]),stlplus::basename_part(iter->second->s_Img_path),"bow");
				imageFiles.push_back(imageFile);
				bofFiles.push_back(bofFile);
			}
			CV_Assert(imageFiles.size()==bofFiles.size());

#pragma omp parallel for
			for (int j=0; j<imageFiles.size(); j++) {
				string imageFile = imageFiles[j];
				cout << "Calculate BoW for image : " << imageFile << endl;

				std::vector<cv::KeyPoint> keypoints;
				cv::Mat descriptors = denseLocalFeatureWrapper->calcDenseLocalFeature(imageFile, keypoints);
				cv::Mat pcaDescriptors = pcaWrapper->calcPcaProject(descriptors);
				cv::Mat bofMat = bof->calcBoF(pcaDescriptors, keypoints);

				string bofFile = bofFiles[j];
				//hulo::saveMat(bofFile, bofMat);
				hulo::saveMatBin(bofFile, bofMat);
				cout << "Saved BoW feature : " << bofFile << endl;
			}
		}
		cout << "End calculate BoF feature for all images." << endl;
	} else {
		cout << "Start getting features to train kmeans...." << endl;
		cv::Mat kmeansTrainFeatures = getRandomTrainFeatures(
				*denseLocalFeatureWrapper, images, KMEANS_TRAIN_FEATURE_NUM,
				KMEANS_TRAIN_FEATURE_NUM_PER_IMAGE);
		cout << "End getting features to train k-means. Training feature matrix size is (" << kmeansTrainFeatures.rows << " x " << kmeansTrainFeatures.cols << ")" << endl;

		cout << "Start train BoF...." << endl;
		BoFSpatialPyramids* bof = new BoFSpatialPyramids(kmeansTrainFeatures, K, RESIZED_IMAGE_SIZE,
				NORM_BOF_FEATURE_TYPE, USE_SPATIAL_PYRAMID, PYRAMID_LEVEL);
		cout << "End train BoF." << endl;

		cout << "Start save BoF model...." << endl;
		{
			cv::FileStorage fsWrite(bowFile, cv::FileStorage::WRITE);
			bof->write(fsWrite);
			fsWrite.release();
		}
		cout << "End save BoF model." << endl;

		cout << "Reload BoF model from file...." << endl;
		{
			delete bof;
			cv::FileStorage fsRead(bowFile, cv::FileStorage::READ);
			bof = new BoFSpatialPyramids();
			bof->read(fsRead.root());
		}
		cout << "End reload BoF model from file." << endl;

		cout << "Start calculate BoF feature for all images." << endl;
		for (int i=0; i<sfmDataFiles.size(); i++) {
			SfM_Data sfm_data;
			if(!Load(sfm_data, sfmDataFiles[i], ESfM_Data(VIEWS|INTRINSICS))){
				cerr << "Cannot load " << sfmDataFiles[i] << endl;
				return EXIT_FAILURE;
			}

			std::vector<string> imageFiles;
			std::vector<string> bofFiles;
			for(Views::const_iterator iter = sfm_data.views.begin(); iter != sfm_data.views.end(); iter++){
				string imageFile = stlplus::create_filespec(sfm_data.s_root_path,stlplus::basename_part(iter->second->s_Img_path),stlplus::extension_part(iter->second->s_Img_path));
				string bofFile = stlplus::create_filespec(stlplus::folder_part(sfmDataFiles[i]),stlplus::basename_part(iter->second->s_Img_path),"bow");
				imageFiles.push_back(imageFile);
				bofFiles.push_back(bofFile);
			}
			CV_Assert(imageFiles.size()==bofFiles.size());

#pragma omp parallel for
			for (int j=0; j<imageFiles.size(); j++) {
				string imageFile = imageFiles[j];
				cout << "Calculate BoW for image : " << imageFile << endl;

				std::vector<cv::KeyPoint> keypoints;
				cv::Mat descriptors = denseLocalFeatureWrapper->calcDenseLocalFeature(imageFile, keypoints);
				cv::Mat bofMat = bof->calcBoF(descriptors, keypoints);

				string bofFile = bofFiles[j];
				//hulo::saveMat(bofFile, bofMat);
				hulo::saveMatBin(bofFile, bofMat);
				cout << "Saved BoW feature : " << bofFile << endl;
			}
		}
		cout << "End calculate BoF feature for all images." << endl;
	}

	return 0;
}
