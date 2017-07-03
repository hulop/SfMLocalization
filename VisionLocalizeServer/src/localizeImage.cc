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

#include <nan.h>
#include <node.h>
#include <node_buffer.h>
#include <v8.h>
#include <uuid/uuid.h>
#include <stdlib.h>

#include <opencv2/opencv.hpp>
#include <sfm/sfm_data.hpp>
#include <sfm/sfm_data_io.hpp>
#include <openMVG/sfm/pipelines/sfm_robust_model_estimation.hpp>
#include <openMVG/sfm/pipelines/sfm_matches_provider.hpp>

#include "LocalizeEngine.h"

using namespace std;
using namespace v8;
using namespace Nan;

std::map<std::string, std::map<std::string, LocalizeEngine> > sfmDataDirMap;
std::map<std::string, std::map<std::string, LocalizeEngine> > matchDirMap;
std::map<std::string, std::map<std::string, LocalizeEngine> > aMatFileMap;

static const double SECOND_TEST_RATIO = 0.6;
static const int RANSAC_ROUND = 25;
static const double RANSAC_PRECISION = 4.0;
static const bool GUIDED_MATCHING = false;
// setting for version 0.2
//static const int BEACON_KNN_NUM = 400; // set 0 if you do not want to use BOW
// setting for WACV 2017
static const int BEACON_KNN_NUM = 800; // set 0 if you do not want to use BOW
// setting for version 0.2
//static const int BOW_KNN_NUM = 200; // set 0 if you do not want to use BOW
// modified 2016.02.16
//static const int BOW_KNN_NUM = 20; // set 0 if you do not want to use BOW
// setting for WACV 2017
static const int BOW_KNN_NUM = 100; // set 0 if you do not want to use BOW

static const std::string TMP_DIR = "/tmp/vision-localize-server";

std::map<std::string, std::map<std::string, LocalizeEngine> > localizeEngineMap;
std::map<std::string, cv::Mat> userMapxMap;
std::map<std::string, cv::Mat> userMapyMap;
std::map<std::string, cv::Mat> userCameraMatMap;
std::map<std::string, cv::Mat> userDistMap;
std::map<std::string, cv::Mat> userNewCameraMatMap;
std::map<std::string, cv::Rect> userValidRoiMap;

/*
 * Caution : This function caches SfM data in memory to improve performance.
 * 			 You cannot call this function at the same time with same User ID and Map ID.
 */
Local<Array> execLocalizeImage(const std::string& userID, const std::string& kMatFile, const std::string& distMatFile,
		const std::string& mapID, const std::string& sfmDataDir, const std::string& matchDir, const std::string& aMatFile,
		double scaleImage, const cv::Mat& _image, const std::string& beaconStr,
		const bool bReturnKeypoints, const bool bReturnTime,
		const std::vector<double>& center=std::vector<double>(), double radius=-1.0) {
	cout << "start localize" << endl;
	double startLocalizeTime = (double) cv::getTickCount();

	// scale input image if option is set
	cv::Mat image;
	if (scaleImage==1.0) {
		image = _image;
	} else {
		cv::resize(_image, image, cv::Size(), scaleImage, scaleImage);
	}

	// select localize engine
	cout << "start initialize LocalizeEngine" << endl;
	LocalizeEngine* localizeEngine;
	if (localizeEngineMap.find(userID) != localizeEngineMap.end()
			&& localizeEngineMap[userID].find(mapID) != localizeEngineMap[userID].end()) {
		localizeEngine = &localizeEngineMap[userID][mapID];
	} else {
		cout << "load new localize engine, userID : " << userID << ", mapID : " << mapID << endl;
		// update localize engine map
		localizeEngineMap[userID][mapID] = LocalizeEngine(sfmDataDir, matchDir, aMatFile, SECOND_TEST_RATIO,
				RANSAC_ROUND, RANSAC_PRECISION, GUIDED_MATCHING, BEACON_KNN_NUM, BOW_KNN_NUM);

		localizeEngine = &localizeEngineMap[userID][mapID];
	}
	cout << "finish initialize LocalizeEngine" << endl;

	// create unique task ID for creating work directory
	string localizeTaskID;
	{
		uuid_t uuidValue;
		uuid_generate(uuidValue);
		char uuidChar[36];
		uuid_unparse_upper(uuidValue, uuidChar);
		localizeTaskID = string(uuidChar);
	}

	// create working directory
	if (!stlplus::folder_exists(TMP_DIR)) {
		 stlplus::folder_create(TMP_DIR);
		 cout << "working directory does not exits, created folder : " << TMP_DIR << endl;
	}
	string workingDir = stlplus::create_filespec(TMP_DIR, localizeTaskID);
	if (stlplus::folder_create(workingDir)) {
		cout << "successed to create working folder : " << workingDir << endl;
	} else {
		cout << "failed to create working folder : " << workingDir << endl;
#if NODE_MAJOR_VERSION>0
		return New<Array>(0);
#else
		return Array::New(0);
#endif
	}

	cv::Mat mapx, mapy, intrinsicK, intrinsicDist, newCameraMat;
	cv::Rect validRoi;
	if (userMapxMap.find(userID)==userMapxMap.end() || userMapyMap.find(userID)==userMapyMap.end()
			|| userCameraMatMap.find(userID)==userCameraMatMap.end() || userDistMap.find(userID)==userDistMap.end()
			|| userNewCameraMatMap.find(userID)==userNewCameraMatMap.end() || userValidRoiMap.find(userID)==userValidRoiMap.end()) {
		{
			cv::FileStorage storage(kMatFile, cv::FileStorage::READ);
			storage["K"] >> intrinsicK;
			storage.release();
		}
		{
			cv::FileStorage storage(distMatFile, cv::FileStorage::READ);
			storage["dist"] >> intrinsicDist;
			storage.release();
		}
		cv::Size imageSize = cv::Size(image.cols, image.rows);
		newCameraMat = cv::getOptimalNewCameraMatrix(intrinsicK, intrinsicDist, imageSize,
				1.0, imageSize, &validRoi);
		cv::initUndistortRectifyMap(intrinsicK, intrinsicDist, cv::Mat(),
				newCameraMat, imageSize, CV_16SC2, mapx, mapy);

		userMapxMap[userID] = mapx;
		userMapyMap[userID] = mapy;
		userCameraMatMap[userID] = intrinsicK;
		userDistMap[userID] = intrinsicDist;
		userNewCameraMatMap[userID] = newCameraMat;
		userValidRoiMap[userID] = validRoi;
	} else {
		mapx = userMapxMap[userID];
		mapy = userMapyMap[userID];
		intrinsicK = userCameraMatMap[userID];
		intrinsicDist = userDistMap[userID];
		newCameraMat = userNewCameraMatMap[userID];
		validRoi = userValidRoiMap[userID];
	}

	cv::Mat undistortImage;

	// faster version
	//cv::remap(image, undistortImage, mapx, mapy, cv::INTER_LINEAR);
	// slower version
	cv::undistort(image, undistortImage, intrinsicK, intrinsicDist, newCameraMat);

	undistortImage = undistortImage(validRoi).clone();

	// execute localize
	cv::Mat points2D;
	cv::Mat points3D;
	std::vector<int> pointsInlier;
	std::vector<double> times;
	cout << "start LocalizeEngine::localize" << endl;
	vector<double> pos = localizeEngine->localize(undistortImage, workingDir, beaconStr,
			bReturnKeypoints, points2D, points3D, pointsInlier, bReturnTime, times, center, radius);
	cout << "finish LocalizeEngine::localize" << endl;
	
	// remove working directory
	if (stlplus::folder_delete(workingDir, true)) {
		cout << "successed to delete working folder : " << workingDir << endl;
	} else {
		cout << "failed to delete working folder : " << workingDir << endl;
	}
	double endLocalizeTime = (double) cv::getTickCount();

	// return result
	if (pos.size()==12) {
#if NODE_MAJOR_VERSION>0
		Local<Array> result = New<Array>(5);
#else
		Local<Array> result = Array::New(5);
#endif

#if NODE_MAJOR_VERSION>0
		Local<Array> posArray = New<Array>(12);
#else
		Local<Array> posArray = Array::New(12);
#endif
		for (int i=0; i<12; i++) {
#if NODE_MAJOR_VERSION>0
			posArray->Set(New<Number>(i), New<Number>(pos[i]));
#else
			posArray->Set(Number::New(i), Number::New(pos[i]));
#endif
		}
#if NODE_MAJOR_VERSION>0
		result->Set(New<Number>(0), posArray);
#else
		result->Set(Number::New(0), posArray);
#endif

		if (bReturnKeypoints) {
#if NODE_MAJOR_VERSION>0
		    Local<Array> points2dArray = New<Array>(points2D.rows);
#else
			Local<Array> points2dArray = Array::New(points2D.rows);
#endif
			for (int i=0; i<points2D.rows; i++) {
#if NODE_MAJOR_VERSION>0
			 	Local<Array> points2dRow = New<Array>(points2D.cols);
#else
				Local<Array> points2dRow = Array::New(points2D.cols);
#endif
				for (int j=0; j<points2D.cols; j++) {
					if (scaleImage==1.0) {
#if NODE_MAJOR_VERSION>0
						points2dRow->Set(New<Number>(j), New<Number>(points2D.at<double>(i,j)));
#else
						points2dRow->Set(Number::New(j), Number::New(points2D.at<double>(i,j)));
#endif
					} else {
#if NODE_MAJOR_VERSION>0
						points2dRow->Set(New<Number>(j), New<Number>(points2D.at<double>(i,j)/scaleImage));
#else
						points2dRow->Set(Number::New(j), Number::New(points2D.at<double>(i,j)/scaleImage));
#endif
					}
				}
				cout << endl;
#if NODE_MAJOR_VERSION>0
				points2dArray->Set(New<Number>(i), points2dRow);
#else
				points2dArray->Set(Number::New(i), points2dRow);
#endif
			}
#if NODE_MAJOR_VERSION>0
			result->Set(New<Number>(1), points2dArray);
#else
			result->Set(Number::New(1), points2dArray);
#endif

#if NODE_MAJOR_VERSION>0
			Local<Array> points3dArray = New<Array>(points3D.rows);
#else
			Local<Array> points3dArray = Array::New(points3D.rows);
#endif			
			for (int i=0; i<points3D.rows; i++) {
#if NODE_MAJOR_VERSION>0
				Local<Array> points3dRow = New<Array>(points3D.cols);
#else
				Local<Array> points3dRow = Array::New(points3D.cols);
#endif
				for (int j=0; j<points3D.cols; j++) {
#if NODE_MAJOR_VERSION>0
					points3dRow->Set(New<Number>(j), New<Number>(points3D.at<double>(i,j)));
#else
					points3dRow->Set(Number::New(j), Number::New(points3D.at<double>(i,j)));
#endif
				}
#if NODE_MAJOR_VERSION>0
				points3dArray->Set(New<Number>(i), points3dRow);
#else
				points3dArray->Set(Number::New(i), points3dRow);
#endif
			}
#if NODE_MAJOR_VERSION>0
			result->Set(New<Number>(2), points3dArray);
#else
			result->Set(Number::New(2), points3dArray);
#endif

#if NODE_MAJOR_VERSION>0
			Local<Array> inlierArray = New<Array>(pointsInlier.size());
#else
			Local<Array> inlierArray = Array::New(pointsInlier.size());
#endif
			for (int i=0; i<pointsInlier.size(); i++) {
#if NODE_MAJOR_VERSION>0
				inlierArray->Set(New<Number>(i), New<Number>(pointsInlier[i]));
#else
				inlierArray->Set(Number::New(i), Number::New(pointsInlier[i]));
#endif
			}
#if NODE_MAJOR_VERSION>0
			result->Set(New<Number>(3), inlierArray);
#else
			result->Set(Number::New(3), inlierArray);
#endif
		}
		if (bReturnTime) {
			double timeOthers = (endLocalizeTime - startLocalizeTime) / cv::getTickFrequency();
			for (int i=0; i<times.size(); i++) {
				timeOthers -= times[i];
			}

#if NODE_MAJOR_VERSION>0
			Local<Array> timesArray = New<Array>(times.size()+1);
#else
			Local<Array> timesArray = Array::New(times.size()+1);
#endif
			for (int i=0; i<times.size(); i++) {
#if NODE_MAJOR_VERSION>0
				timesArray->Set(New<Number>(i), New<Number>(times[i]));
#else
				timesArray->Set(Number::New(i), Number::New(times[i]));
#endif
			}
#if NODE_MAJOR_VERSION>0
			timesArray->Set(New<Number>(times.size()), New<Number>(timeOthers));
#else
			timesArray->Set(Number::New(times.size()), Number::New(timeOthers));
#endif

#if NODE_MAJOR_VERSION>0
			result->Set(New<Number>(4), timesArray);
#else
			result->Set(Number::New(4), timesArray);
#endif
		}
		return result;
	} else {
#if NODE_MAJOR_VERSION>0
		return New<Array>(0);
#else
		return Array::New(0);
#endif
	}
}

NAN_METHOD(LocalizeImageBuffer) {
	if (info.Length() != 11 && info.Length() != 13) {
	  	Nan::ThrowError("Wrong number of arguments");
		return;
	}
	if (!info[0]->IsString() || !info[1]->IsString() || !info[2]->IsString()
		|| !info[3]->IsNumber() || !info[4]->IsString() || !info[5]->IsString()
		|| !info[6]->IsString() || !info[7]->IsString() || !info[8]->IsBoolean()
		|| !info[9]->IsBoolean() || !info[10]->IsObject()) {
	 	Nan::ThrowError("Wrong arguments");
		return;
	}

	String::Utf8Value userID(info[0]->ToString());
	const char* userIDChar = (const char*)(*userID);

	String::Utf8Value kMatFile(info[1]->ToString());
	const char* kMatFileChar = (const char*)(*kMatFile);

	String::Utf8Value distMatFile(info[2]->ToString());
	const char* distMatFileChar = (const char*)(*distMatFile);

	double scaleImage = info[3]->NumberValue();

	String::Utf8Value mapID(info[4]->ToString());
	const char* mapIDChar = (const char*)(*mapID);

	String::Utf8Value sfmDataDir(info[5]->ToString());
	const char* sfmDataDirChar = (const char*)(*sfmDataDir);

	String::Utf8Value matchDir(info[6]->ToString());
	const char* matchDirChar = (const char*)(*matchDir);

	String::Utf8Value aMatFile(info[7]->ToString());
	const char* aMatFileChar = (const char*)(*aMatFile);

	bool bReturnKeypoints = info[8]->ToBoolean()->Value();
	bool bReturnTime = info[9]->ToBoolean()->Value();

	Local<Object> imageBuffer = info[10]->ToObject();
	char* imageData    = node::Buffer::Data(imageBuffer);
	size_t imageDataLen = node::Buffer::Length(imageBuffer);
	cv::Mat image = cv::imdecode(cv::_InputArray(imageData, imageDataLen), cv::IMREAD_COLOR);
	if (image.empty() || image.rows==0 || image.cols==0) {
	  	Nan::ThrowError("Input image is empty");
		return;
	}

	Local<Array> result;
	if (info.Length()==11) {
		result = execLocalizeImage(std::string(userIDChar), std::string(kMatFileChar), std::string(distMatFileChar),
					std::string(mapIDChar), std::string(sfmDataDirChar), std::string(matchDirChar), std::string(aMatFileChar),
					scaleImage, image, "", bReturnKeypoints, bReturnTime);
	} else {
#if NODE_MAJOR_VERSION>0
	 	Local<Array> center = Local<Array>::Cast(info[11]);
#else
	 	Local<Array> center = Array::Cast(*info[11]);
#endif
		std::vector<double> centerVec;
	        for(int i = 0; i < center->Length(); i++) {
		    centerVec.push_back(center->Get(i)->NumberValue());
		}
		double radius = info[12]->NumberValue();
		result = execLocalizeImage(std::string(userIDChar), std::string(kMatFileChar), std::string(distMatFileChar),
					   std::string(mapIDChar), std::string(sfmDataDirChar), std::string(matchDirChar), std::string(aMatFileChar),
					   scaleImage, image, "", bReturnKeypoints, bReturnTime, centerVec, radius);
	}
	info.GetReturnValue().Set(result);
}

NAN_METHOD(LocalizeImagePath) {
	if (info.Length() != 11 && info.Length() != 13) {
	  	Nan::ThrowError("Wrong number of arguments");
		return;
	}
	if (!info[0]->IsString() || !info[1]->IsString() || !info[2]->IsString()
		|| !info[3]->IsNumber() || !info[4]->IsString() || !info[5]->IsString()
		|| !info[6]->IsString() || !info[7]->IsString() || !info[8]->IsBoolean()
		|| !info[9]->IsBoolean() || !info[10]->IsString()) {
	 	Nan::ThrowError("Wrong arguments");
		return;
	}

	String::Utf8Value userID(info[0]->ToString());
	const char* userIDChar = (const char*)(*userID);

	String::Utf8Value kMatFile(info[1]->ToString());
	const char* kMatFileChar = (const char*)(*kMatFile);

	String::Utf8Value distMatFile(info[2]->ToString());
	const char* distMatFileChar = (const char*)(*distMatFile);

	double scaleImage = info[3]->NumberValue();

	String::Utf8Value mapID(info[4]->ToString());
	const char* mapIDChar = (const char*)(*mapID);

	String::Utf8Value sfmDataDir(info[5]->ToString());
	const char* sfmDataDirChar = (const char*)(*sfmDataDir);

	String::Utf8Value matchDir(info[6]->ToString());
	const char* matchDirChar = (const char*)(*matchDir);

	String::Utf8Value aMatFile(info[7]->ToString());
	const char* aMatFileChar = (const char*)(*aMatFile);

	bool bReturnKeypoints = info[8]->ToBoolean()->Value();
	bool bReturnTime = info[9]->ToBoolean()->Value();

	String::Utf8Value imagePath(info[10]->ToString());
	const char* imagePathChar = (const char*)(*imagePath);
	cv::Mat image = cv::imread(std::string(imagePathChar), cv::IMREAD_COLOR);
	if (image.empty() || image.rows==0 || image.cols==0) {
	   	Nan::ThrowError("Input image is empty");
		return;
	}

	Local<Array> result;
	if (info.Length() == 11) {
		result = execLocalizeImage(std::string(userIDChar), std::string(kMatFileChar), std::string(distMatFileChar),
					std::string(mapIDChar), std::string(sfmDataDirChar), std::string(matchDirChar), std::string(aMatFileChar),
					scaleImage, image, "", bReturnKeypoints, bReturnTime);
	} else {
#if NODE_MAJOR_VERSION>0
	  	Local<Array> center = Local<Array>::Cast(info[11]);
#else
		Local<Array> center = Array::Cast(*info[11]);
#endif
		std::vector<double> centerVec;
	    for(int i = 0; i < center->Length(); i++) {
	    	centerVec.push_back(center->Get(i)->NumberValue());
	    }
	    double radius = info[12]->NumberValue();
	    result = execLocalizeImage(std::string(userIDChar), std::string(kMatFileChar), std::string(distMatFileChar),
	    			std::string(mapIDChar), std::string(sfmDataDirChar), std::string(matchDirChar), std::string(aMatFileChar),
					scaleImage, image, "", bReturnKeypoints, bReturnTime, centerVec, radius);
	}
	info.GetReturnValue().Set(result);
}

NAN_METHOD(LocalizeImageBufferBeacon) {
	if (info.Length() != 12 && info.Length() != 14) {
	   	Nan::ThrowError("Wrong number of arguments");
		return;
	}
	if (!info[0]->IsString() || !info[1]->IsString() || !info[2]->IsString()
		|| !info[3]->IsNumber() || !info[4]->IsString() || !info[5]->IsString()
		|| !info[6]->IsString() || !info[7]->IsString() || !info[8]->IsBoolean()
		|| !info[9]->IsBoolean() || !info[10]->IsObject() || !info[11]->IsString()) {
	   	Nan::ThrowError("Wrong arguments");
		return;
	}

	String::Utf8Value userID(info[0]->ToString());
	const char* userIDChar = (const char*)(*userID);

	String::Utf8Value kMatFile(info[1]->ToString());
	const char* kMatFileChar = (const char*)(*kMatFile);

	String::Utf8Value distMatFile(info[2]->ToString());
	const char* distMatFileChar = (const char*)(*distMatFile);

	double scaleImage = info[3]->NumberValue();

	String::Utf8Value mapID(info[4]->ToString());
	const char* mapIDChar = (const char*)(*mapID);

	String::Utf8Value sfmDataDir(info[5]->ToString());
	const char* sfmDataDirChar = (const char*)(*sfmDataDir);

	String::Utf8Value matchDir(info[6]->ToString());
	const char* matchDirChar = (const char*)(*matchDir);

	String::Utf8Value aMatFile(info[7]->ToString());
	const char* aMatFileChar = (const char*)(*aMatFile);

	bool bReturnKeypoints = info[8]->ToBoolean()->Value();
	bool bReturnTime = info[9]->ToBoolean()->Value();

	Local<Object> imageBuffer = info[10]->ToObject();
	char* imageData    = node::Buffer::Data(imageBuffer);
	size_t imageDataLen = node::Buffer::Length(imageBuffer);
	cv::Mat image = cv::imdecode(cv::_InputArray(imageData, imageDataLen), cv::IMREAD_COLOR);
	if (image.empty() || image.rows==0 || image.cols==0) {
	 	Nan::ThrowError("Input image is empty");
		return;
	}

	String::Utf8Value beaconStr(info[11]->ToString());
	const char* beaconStrChar = (const char*)(*beaconStr);

	Local<Array> result;
	if (info.Length()==12) {
		result = execLocalizeImage(std::string(userIDChar), std::string(kMatFileChar), std::string(distMatFileChar),
					std::string(mapIDChar), std::string(sfmDataDirChar), std::string(matchDirChar), std::string(aMatFileChar),
					scaleImage, image, std::string(beaconStrChar), bReturnKeypoints, bReturnTime);
	} else {
#if NODE_MAJOR_VERSION>0
	   	Local<Array> center = Local<Array>::Cast(info[12]);
#else
		Local<Array> center = Array::Cast(*info[12]);
#endif
		std::vector<double> centerVec;
	    for(int i = 0; i < center->Length(); i++) {
	    	centerVec.push_back(center->Get(i)->NumberValue());
	    }
	    double radius = info[13]->NumberValue();
	    result = execLocalizeImage(std::string(userIDChar), std::string(kMatFileChar), std::string(distMatFileChar),
	    			std::string(mapIDChar), std::string(sfmDataDirChar), std::string(matchDirChar), std::string(aMatFileChar),
					scaleImage, image, std::string(beaconStrChar), bReturnKeypoints, bReturnTime, centerVec, radius);
	}
	info.GetReturnValue().Set(result);
}

NAN_METHOD(LocalizeImagePathBeacon) {
	if (info.Length() != 12 && info.Length() != 14) {
	 	Nan::ThrowError("Wrong number of arguments");
		return;
	}
	if (!info[0]->IsString() || !info[1]->IsString() || !info[2]->IsString()
		|| !info[3]->IsNumber() || !info[4]->IsString() || !info[5]->IsString()
		|| !info[6]->IsString() || !info[7]->IsString() || !info[8]->IsBoolean()
		|| !info[9]->IsBoolean() || !info[10]->IsString() || !info[11]->IsString()) {
	  	Nan::ThrowError("Wrong arguments");
		return;
	}

	String::Utf8Value userID(info[0]->ToString());
	const char* userIDChar = (const char*)(*userID);

	String::Utf8Value kMatFile(info[1]->ToString());
	const char* kMatFileChar = (const char*)(*kMatFile);

	String::Utf8Value distMatFile(info[2]->ToString());
	const char* distMatFileChar = (const char*)(*distMatFile);

	double scaleImage = info[3]->NumberValue();

	String::Utf8Value mapID(info[4]->ToString());
	const char* mapIDChar = (const char*)(*mapID);

	String::Utf8Value sfmDataDir(info[5]->ToString());
	const char* sfmDataDirChar = (const char*)(*sfmDataDir);

	String::Utf8Value matchDir(info[6]->ToString());
	const char* matchDirChar = (const char*)(*matchDir);

	String::Utf8Value aMatFile(info[7]->ToString());
	const char* aMatFileChar = (const char*)(*aMatFile);

	bool bReturnKeypoints = info[8]->ToBoolean()->Value();
	bool bReturnTime = info[9]->ToBoolean()->Value();

	String::Utf8Value imagePath(info[10]->ToString());
	const char* imagePathChar = (const char*)(*imagePath);
	cv::Mat image = cv::imread(std::string(imagePathChar), cv::IMREAD_COLOR);
	if (image.empty() || image.rows==0 || image.cols==0) {
	 	Nan::ThrowError("Input image is empty");
		return;
	}

	String::Utf8Value beaconStr(info[11]->ToString());
	const char* beaconStrChar = (const char*)(*beaconStr);

	Local<Array> result;
	if (info.Length() == 12) {
	    result = execLocalizeImage(std::string(userIDChar), std::string(kMatFileChar), std::string(distMatFileChar),
				       std::string(mapIDChar), std::string(sfmDataDirChar), std::string(matchDirChar), std::string(aMatFileChar),
				       scaleImage, image, std::string(beaconStrChar), bReturnKeypoints, bReturnTime);
	} else {
#if NODE_MAJOR_VERSION>0
	    Local<Array> center = Local<Array>::Cast(info[12]);
#else
	    Local<Array> center = Array::Cast(*info[12]);
#endif
	    std::vector<double> centerVec;
	    for(int i = 0; i < center->Length(); i++) {
	    	centerVec.push_back(center->Get(i)->NumberValue());
	    }
	    double radius = info[13]->NumberValue();
	    result = execLocalizeImage(std::string(userIDChar), std::string(kMatFileChar), std::string(distMatFileChar),
	    			std::string(mapIDChar), std::string(sfmDataDirChar), std::string(matchDirChar), std::string(aMatFileChar),
					scaleImage, image, std::string(beaconStrChar), bReturnKeypoints, bReturnTime, centerVec, radius);
	}
	info.GetReturnValue().Set(result);
}

NAN_MODULE_INIT(Init) {
#if NODE_MAJOR_VERSION>0
  	Nan::Set(target, New<String>("localizeImageBuffer").ToLocalChecked(),
  			GetFunction(New<FunctionTemplate>(LocalizeImageBuffer)).ToLocalChecked());
	Nan::Set(target, New<String>("localizeImagePath").ToLocalChecked(),
			GetFunction(New<FunctionTemplate>(LocalizeImagePath)).ToLocalChecked());
	Nan::Set(target, New<String>("localizeImageBufferBeacon").ToLocalChecked(),
			GetFunction(New<FunctionTemplate>(LocalizeImageBufferBeacon)).ToLocalChecked());
	Nan::Set(target, New<String>("localizeImagePathBeacon").ToLocalChecked(),
			GetFunction(New<FunctionTemplate>(LocalizeImagePathBeacon)).ToLocalChecked());
#else
   	Nan::SetMethod(target, "localizeImageBuffer", LocalizeImageBuffer);
	Nan::SetMethod(target, "localizeImagePath", LocalizeImagePath);
	Nan::SetMethod(target, "localizeImageBufferBeacon", LocalizeImageBufferBeacon);
	Nan::SetMethod(target, "localizeImagePathBeacon", LocalizeImagePathBeacon);
#endif
}

NODE_MODULE(localizeImage, Init)
