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
		return Array::New(0);
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
		Local<Array> result = Array::New(5);

		Local<Array> posArray = Array::New(12);
		for (int i=0; i<12; i++) {
			posArray->Set(Number::New(i), Number::New(pos[i]));
		}
		result->Set(Number::New(0), posArray);

		if (bReturnKeypoints) {
			Local<Array> points2dArray = Array::New(points2D.rows);
			for (int i=0; i<points2D.rows; i++) {
				Local<Array> points2dRow = Array::New(points2D.cols);
				for (int j=0; j<points2D.cols; j++) {
					if (scaleImage==1.0) {
						points2dRow->Set(Number::New(j), Number::New(points2D.at<double>(i,j)));
					} else {
						points2dRow->Set(Number::New(j), Number::New(points2D.at<double>(i,j)/scaleImage));
					}
				}
				cout << endl;
				points2dArray->Set(Number::New(i), points2dRow);
			}
			result->Set(Number::New(1), points2dArray);

			Local<Array> points3dArray = Array::New(points3D.rows);
			for (int i=0; i<points3D.rows; i++) {
				Local<Array> points3dRow = Array::New(points3D.cols);
				for (int j=0; j<points3D.cols; j++) {
					points3dRow->Set(Number::New(j), Number::New(points3D.at<double>(i,j)));
				}
				points3dArray->Set(Number::New(i), points3dRow);
			}
			result->Set(Number::New(2), points3dArray);

			Local<Array> inlierArray = Array::New(pointsInlier.size());
			for (int i=0; i<pointsInlier.size(); i++) {
				inlierArray->Set(Number::New(i), Number::New(pointsInlier[i]));
			}
			result->Set(Number::New(3), inlierArray);
		}
		if (bReturnTime) {
			double timeOthers = (endLocalizeTime - startLocalizeTime) / cv::getTickFrequency();
			for (int i=0; i<times.size(); i++) {
				timeOthers -= times[i];
			}

			Local<Array> timesArray = Array::New(times.size()+1);
			for (int i=0; i<times.size(); i++) {
				timesArray->Set(Number::New(i), Number::New(times[i]));
			}
			timesArray->Set(Number::New(times.size()), Number::New(timeOthers));

			result->Set(Number::New(4), timesArray);
		}
		return result;
	} else {
		return Array::New(0);
	}
}

Handle<Value> LocalizeImageBuffer(const Arguments& args) {
	HandleScope scope;

	if (args.Length() != 11 && args.Length() != 13) {
		ThrowException(
				Exception::TypeError(String::New("Wrong number of arguments")));
		return scope.Close(Undefined());
	}
	if (!args[0]->IsString() || !args[1]->IsString() || !args[2]->IsString()
		|| !args[3]->IsNumber() || !args[4]->IsString() || !args[5]->IsString()
		|| !args[6]->IsString() || !args[7]->IsString() || !args[8]->IsBoolean()
		|| !args[9]->IsBoolean() || !args[10]->IsObject()) {
		ThrowException(Exception::TypeError(String::New("Wrong arguments")));
		return scope.Close(Undefined());
	}

	String::Utf8Value userID(args[0]->ToString());
	const char* userIDChar = (const char*)(*userID);

	String::Utf8Value kMatFile(args[1]->ToString());
	const char* kMatFileChar = (const char*)(*kMatFile);

	String::Utf8Value distMatFile(args[2]->ToString());
	const char* distMatFileChar = (const char*)(*distMatFile);

	double scaleImage = args[3]->NumberValue();

	String::Utf8Value mapID(args[4]->ToString());
	const char* mapIDChar = (const char*)(*mapID);

	String::Utf8Value sfmDataDir(args[5]->ToString());
	const char* sfmDataDirChar = (const char*)(*sfmDataDir);

	String::Utf8Value matchDir(args[6]->ToString());
	const char* matchDirChar = (const char*)(*matchDir);

	String::Utf8Value aMatFile(args[7]->ToString());
	const char* aMatFileChar = (const char*)(*aMatFile);

	bool bReturnKeypoints = args[8]->ToBoolean()->Value();
	bool bReturnTime = args[9]->ToBoolean()->Value();

	Local<Object> imageBuffer = args[10]->ToObject();
	char* imageData    = node::Buffer::Data(imageBuffer);
	size_t imageDataLen = node::Buffer::Length(imageBuffer);
	cv::Mat image = cv::imdecode(cv::_InputArray(imageData, imageDataLen), cv::IMREAD_COLOR);
	if (image.empty() || image.rows==0 || image.cols==0) {
		ThrowException(Exception::TypeError(String::New("Input image is empty")));
		return scope.Close(Undefined());
	}

	Local<Array> result;
	if (args.Length()==11) {
		result = execLocalizeImage(std::string(userIDChar), std::string(kMatFileChar), std::string(distMatFileChar),
					std::string(mapIDChar), std::string(sfmDataDirChar), std::string(matchDirChar), std::string(aMatFileChar),
					scaleImage, image, "", bReturnKeypoints, bReturnTime);
	} else {
		Local<Array> center = Array::Cast(*args[11]);
		std::vector<double> centerVec;
	    for(int i = 0; i < center->Length(); i++) {
	    	centerVec.push_back(center->Get(i)->NumberValue());
	    }
	    double radius = args[12]->NumberValue();
	    result = execLocalizeImage(std::string(userIDChar), std::string(kMatFileChar), std::string(distMatFileChar),
	    			std::string(mapIDChar), std::string(sfmDataDirChar), std::string(matchDirChar), std::string(aMatFileChar),
					scaleImage, image, "", bReturnKeypoints, bReturnTime, centerVec, radius);
	}
	return scope.Close(result);
}

Handle<Value> LocalizeImagePath(const Arguments& args) {
	HandleScope scope;

	if (args.Length() != 11 && args.Length() != 13) {
		ThrowException(
				Exception::TypeError(String::New("Wrong number of arguments")));
		return scope.Close(Undefined());
	}
	if (!args[0]->IsString() || !args[1]->IsString() || !args[2]->IsString()
		|| !args[3]->IsNumber() || !args[4]->IsString() || !args[5]->IsString()
		|| !args[6]->IsString() || !args[7]->IsString() || !args[8]->IsBoolean()
		|| !args[9]->IsBoolean() || !args[10]->IsString()) {
		ThrowException(Exception::TypeError(String::New("Wrong arguments")));
		return scope.Close(Undefined());
	}

	String::Utf8Value userID(args[0]->ToString());
	const char* userIDChar = (const char*)(*userID);

	String::Utf8Value kMatFile(args[1]->ToString());
	const char* kMatFileChar = (const char*)(*kMatFile);

	String::Utf8Value distMatFile(args[2]->ToString());
	const char* distMatFileChar = (const char*)(*distMatFile);

	double scaleImage = args[3]->NumberValue();

	String::Utf8Value mapID(args[4]->ToString());
	const char* mapIDChar = (const char*)(*mapID);

	String::Utf8Value sfmDataDir(args[5]->ToString());
	const char* sfmDataDirChar = (const char*)(*sfmDataDir);

	String::Utf8Value matchDir(args[6]->ToString());
	const char* matchDirChar = (const char*)(*matchDir);

	String::Utf8Value aMatFile(args[7]->ToString());
	const char* aMatFileChar = (const char*)(*aMatFile);

	bool bReturnKeypoints = args[8]->ToBoolean()->Value();
	bool bReturnTime = args[9]->ToBoolean()->Value();

	String::Utf8Value imagePath(args[10]->ToString());
	const char* imagePathChar = (const char*)(*imagePath);
	cv::Mat image = cv::imread(std::string(imagePathChar), cv::IMREAD_COLOR);
	if (image.empty() || image.rows==0 || image.cols==0) {
		ThrowException(Exception::TypeError(String::New("Input image is empty")));
		return scope.Close(Undefined());
	}

	Local<Array> result;
	if (args.Length() == 11) {
		result = execLocalizeImage(std::string(userIDChar), std::string(kMatFileChar), std::string(distMatFileChar),
					std::string(mapIDChar), std::string(sfmDataDirChar), std::string(matchDirChar), std::string(aMatFileChar),
					scaleImage, image, "", bReturnKeypoints, bReturnTime);
	} else {
		Local<Array> center = Array::Cast(*args[11]);
		std::vector<double> centerVec;
	    for(int i = 0; i < center->Length(); i++) {
	    	centerVec.push_back(center->Get(i)->NumberValue());
	    }
	    double radius = args[12]->NumberValue();
	    result = execLocalizeImage(std::string(userIDChar), std::string(kMatFileChar), std::string(distMatFileChar),
	    			std::string(mapIDChar), std::string(sfmDataDirChar), std::string(matchDirChar), std::string(aMatFileChar),
					scaleImage, image, "", bReturnKeypoints, bReturnTime, centerVec, radius);
	}
	return scope.Close(result);
}

Handle<Value> LocalizeImageBufferBeacon(const Arguments& args) {
	HandleScope scope;

	if (args.Length() != 12 && args.Length() != 14) {
		ThrowException(
				Exception::TypeError(String::New("Wrong number of arguments")));
		return scope.Close(Undefined());
	}
	if (!args[0]->IsString() || !args[1]->IsString() || !args[2]->IsString()
		|| !args[3]->IsNumber() || !args[4]->IsString() || !args[5]->IsString()
		|| !args[6]->IsString() || !args[7]->IsString() || !args[8]->IsBoolean()
		|| !args[9]->IsBoolean() || !args[10]->IsObject() || !args[11]->IsString()) {
		ThrowException(Exception::TypeError(String::New("Wrong arguments")));
		return scope.Close(Undefined());
	}

	String::Utf8Value userID(args[0]->ToString());
	const char* userIDChar = (const char*)(*userID);

	String::Utf8Value kMatFile(args[1]->ToString());
	const char* kMatFileChar = (const char*)(*kMatFile);

	String::Utf8Value distMatFile(args[2]->ToString());
	const char* distMatFileChar = (const char*)(*distMatFile);

	double scaleImage = args[3]->NumberValue();

	String::Utf8Value mapID(args[4]->ToString());
	const char* mapIDChar = (const char*)(*mapID);

	String::Utf8Value sfmDataDir(args[5]->ToString());
	const char* sfmDataDirChar = (const char*)(*sfmDataDir);

	String::Utf8Value matchDir(args[6]->ToString());
	const char* matchDirChar = (const char*)(*matchDir);

	String::Utf8Value aMatFile(args[7]->ToString());
	const char* aMatFileChar = (const char*)(*aMatFile);

	bool bReturnKeypoints = args[8]->ToBoolean()->Value();
	bool bReturnTime = args[9]->ToBoolean()->Value();

	Local<Object> imageBuffer = args[10]->ToObject();
	char* imageData    = node::Buffer::Data(imageBuffer);
	size_t imageDataLen = node::Buffer::Length(imageBuffer);
	cv::Mat image = cv::imdecode(cv::_InputArray(imageData, imageDataLen), cv::IMREAD_COLOR);
	if (image.empty() || image.rows==0 || image.cols==0) {
		ThrowException(Exception::TypeError(String::New("Input image is empty")));
		return scope.Close(Undefined());
	}

	String::Utf8Value beaconStr(args[11]->ToString());
	const char* beaconStrChar = (const char*)(*beaconStr);

	Local<Array> result;
	if (args.Length()==12) {
		result = execLocalizeImage(std::string(userIDChar), std::string(kMatFileChar), std::string(distMatFileChar),
					std::string(mapIDChar), std::string(sfmDataDirChar), std::string(matchDirChar), std::string(aMatFileChar),
					scaleImage, image, std::string(beaconStrChar), bReturnKeypoints, bReturnTime);
	} else {
		Local<Array> center = Array::Cast(*args[12]);
		std::vector<double> centerVec;
	    for(int i = 0; i < center->Length(); i++) {
	    	centerVec.push_back(center->Get(i)->NumberValue());
	    }
	    double radius = args[13]->NumberValue();
	    result = execLocalizeImage(std::string(userIDChar), std::string(kMatFileChar), std::string(distMatFileChar),
	    			std::string(mapIDChar), std::string(sfmDataDirChar), std::string(matchDirChar), std::string(aMatFileChar),
					scaleImage, image, std::string(beaconStrChar), bReturnKeypoints, bReturnTime, centerVec, radius);
	}
	return scope.Close(result);
}

Handle<Value> LocalizeImagePathBeacon(const Arguments& args) {
	HandleScope scope;

	if (args.Length() != 12 && args.Length() != 14) {
		ThrowException(
				Exception::TypeError(String::New("Wrong number of arguments")));
		return scope.Close(Undefined());
	}
	if (!args[0]->IsString() || !args[1]->IsString() || !args[2]->IsString()
		|| !args[3]->IsNumber() || !args[4]->IsString() || !args[5]->IsString()
		|| !args[6]->IsString() || !args[7]->IsString() || !args[8]->IsBoolean()
		|| !args[9]->IsBoolean() || !args[10]->IsString() || !args[11]->IsString()) {
		ThrowException(Exception::TypeError(String::New("Wrong arguments")));
		return scope.Close(Undefined());
	}

	String::Utf8Value userID(args[0]->ToString());
	const char* userIDChar = (const char*)(*userID);

	String::Utf8Value kMatFile(args[1]->ToString());
	const char* kMatFileChar = (const char*)(*kMatFile);

	String::Utf8Value distMatFile(args[2]->ToString());
	const char* distMatFileChar = (const char*)(*distMatFile);

	double scaleImage = args[3]->NumberValue();

	String::Utf8Value mapID(args[4]->ToString());
	const char* mapIDChar = (const char*)(*mapID);

	String::Utf8Value sfmDataDir(args[5]->ToString());
	const char* sfmDataDirChar = (const char*)(*sfmDataDir);

	String::Utf8Value matchDir(args[6]->ToString());
	const char* matchDirChar = (const char*)(*matchDir);

	String::Utf8Value aMatFile(args[7]->ToString());
	const char* aMatFileChar = (const char*)(*aMatFile);

	bool bReturnKeypoints = args[8]->ToBoolean()->Value();
	bool bReturnTime = args[9]->ToBoolean()->Value();

	String::Utf8Value imagePath(args[10]->ToString());
	const char* imagePathChar = (const char*)(*imagePath);
	cv::Mat image = cv::imread(std::string(imagePathChar), cv::IMREAD_COLOR);
	if (image.empty() || image.rows==0 || image.cols==0) {
		ThrowException(Exception::TypeError(String::New("Input image is empty")));
		return scope.Close(Undefined());
	}

	String::Utf8Value beaconStr(args[11]->ToString());
	const char* beaconStrChar = (const char*)(*beaconStr);

	Local<Array> result;
	if (args.Length() == 12) {
		result = execLocalizeImage(std::string(userIDChar), std::string(kMatFileChar), std::string(distMatFileChar),
					std::string(mapIDChar), std::string(sfmDataDirChar), std::string(matchDirChar), std::string(aMatFileChar),
					scaleImage, image, std::string(beaconStrChar), bReturnKeypoints, bReturnTime);
	} else {
		Local<Array> center = Array::Cast(*args[12]);
		std::vector<double> centerVec;
	    for(int i = 0; i < center->Length(); i++) {
	    	centerVec.push_back(center->Get(i)->NumberValue());
	    }
	    double radius = args[13]->NumberValue();
	    result = execLocalizeImage(std::string(userIDChar), std::string(kMatFileChar), std::string(distMatFileChar),
	    			std::string(mapIDChar), std::string(sfmDataDirChar), std::string(matchDirChar), std::string(aMatFileChar),
					scaleImage, image, std::string(beaconStrChar), bReturnKeypoints, bReturnTime, centerVec, radius);
	}
	return scope.Close(result);
}

void Init(Handle<Object> exports) {
	exports->Set(String::NewSymbol("localizeImageBuffer"),
			FunctionTemplate::New(LocalizeImageBuffer)->GetFunction());
	exports->Set(String::NewSymbol("localizeImagePath"),
			FunctionTemplate::New(LocalizeImagePath)->GetFunction());
	exports->Set(String::NewSymbol("localizeImageBufferBeacon"),
			FunctionTemplate::New(LocalizeImageBufferBeacon)->GetFunction());
	exports->Set(String::NewSymbol("localizeImagePathBeacon"),
			FunctionTemplate::New(LocalizeImagePathBeacon)->GetFunction());
}

NODE_MODULE(localizeImage, Init)
