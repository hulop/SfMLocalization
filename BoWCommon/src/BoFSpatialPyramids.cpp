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

#include "BoFSpatialPyramids.h"

#include <omp.h>

using namespace hulo;

static const int KMEANS_ITERATION = 100;

BoFSpatialPyramids::BoFSpatialPyramids() {
}

BoFSpatialPyramids::BoFSpatialPyramids(const cv::Mat &kmeansTrainFeatures, int _K, int _RESIZED_IMAGE_SIZE,
		NormBofFeatureType _NORM_BOF_FEATURE_TYPE, bool _USE_SPATIAL_PYRAMID,
		int _PYRAMID_LEVEL) {
	K = _K;
	RESIZED_IMAGE_SIZE = _RESIZED_IMAGE_SIZE;
	NORM_BOF_FEATURE_TYPE = _NORM_BOF_FEATURE_TYPE;
	USE_SPATIAL_PYRAMID = _USE_SPATIAL_PYRAMID;
	PYRAMID_LEVEL = _PYRAMID_LEVEL;

	cout << "Start train kmeans...." << endl;
	centers = trainKMeans(kmeansTrainFeatures);

	int maxThreadNum = omp_get_max_threads();
	flannIndexParamsArray = new cv::flann::KDTreeIndexParams*[maxThreadNum];
	flannIndexArray = new cv::flann::Index*[maxThreadNum];
	for (int i = 0; i < maxThreadNum; i++) {
		flannIndexParamsArray[i] = new cv::flann::KDTreeIndexParams();
		flannIndexArray[i] = new cv::flann::Index(centers,
				*flannIndexParamsArray[i]);
	}
	cout << "End train kmeans. Number of cluster is " << K << endl;
}

void BoFSpatialPyramids::write(cv::FileStorage& fs) const
{
	fs << "K" << K;
	fs << "ResizedImageSize" << RESIZED_IMAGE_SIZE;
	fs << "NormBofFeatureType" << (NORM_BOF_FEATURE_TYPE == NormBofFeatureType::NONE ? string("NONE") :
				NORM_BOF_FEATURE_TYPE == NormBofFeatureType::L2_NORM ? string("L2") :
						NORM_BOF_FEATURE_TYPE == NormBofFeatureType::L1_NORM_SQUARE_ROOT ? string("L1") :
								"UNKNOWN");
	fs << "UseSpatialPyramid" << USE_SPATIAL_PYRAMID;
	fs << "PyramidLevel" << PYRAMID_LEVEL;
	fs << "Centers" << centers;
}

void BoFSpatialPyramids::read(const cv::FileNode& node)
{
	node["K"] >> K;
	node["ResizedImageSize"] >> RESIZED_IMAGE_SIZE;
	string normBofFeatureTypeStr;
	node["NormBofFeatureType"] >> normBofFeatureTypeStr;
	NORM_BOF_FEATURE_TYPE = normBofFeatureTypeStr == "NONE" ? NormBofFeatureType::NONE :
			normBofFeatureTypeStr == "L2" ? NormBofFeatureType::L2_NORM :
					normBofFeatureTypeStr == "L1" ? NormBofFeatureType::L1_NORM_SQUARE_ROOT :
							NormBofFeatureType::UNKNOWN;
	CV_Assert(NORM_BOF_FEATURE_TYPE>=0);
	node["UseSpatialPyramid"] >> USE_SPATIAL_PYRAMID;
	node["PyramidLevel"] >> PYRAMID_LEVEL;
	node["Centers"] >> centers;

	int maxThreadNum = omp_get_max_threads();
	flannIndexParamsArray = new cv::flann::KDTreeIndexParams*[maxThreadNum];
	flannIndexArray = new cv::flann::Index*[maxThreadNum];
	for (int i = 0; i < maxThreadNum; i++) {
		flannIndexParamsArray[i] = new cv::flann::KDTreeIndexParams();
		flannIndexArray[i] = new cv::flann::Index(centers,
				*flannIndexParamsArray[i]);
	}
}

cv::Mat BoFSpatialPyramids::trainKMeans(const cv::Mat &features) {
	cv::Mat centers;
	cv::Mat labels;

	int clusterCount = min(K, features.rows);
	cv::kmeans(features, clusterCount, labels,
			cv::TermCriteria(cv::TermCriteria::COUNT + cv::TermCriteria::EPS,
					KMEANS_ITERATION, FLT_EPSILON), 3, cv::KMEANS_PP_CENTERS,
			centers);

	return centers;
}

cv::Mat BoFSpatialPyramids::calcBoF(const cv::Mat &descriptors,
		const std::vector<cv::KeyPoint> &keypoints) {
	CV_Assert(K > 0);
	CV_Assert(PYRAMID_LEVEL <= 3);

	int flannIndex = omp_get_thread_num();
	cout << "calc bof, thread id=" << flannIndex << endl;

	cv::Mat indices(descriptors.rows, 1, CV_32SC1);
	cv::Mat dists(descriptors.rows, 1, CV_32FC1);
	flannIndexArray[flannIndex]->knnSearch(descriptors, indices, dists, 1,
			cv::flann::SearchParams());

	cv::Mat bof;
	if (USE_SPATIAL_PYRAMID) {
		int cellNums = 0;
		for (int level = 0; level < PYRAMID_LEVEL; level++) {
			if (level == 0) {
				cellNums += 1;
			} else if (level == 2) {
				cellNums += 3;
			} else {
				cellNums += pow(level + 1, 2.0);
			}
		}

		bof = cv::Mat::zeros(K * cellNums, 1, CV_64F);
#pragma omp parallel for
		for (int level = 0; level < PYRAMID_LEVEL; level++) {
			if (level == 2) {
				int cellLength = level + 1;
				for (int cellY = 0; cellY < cellLength; cellY++) {
					int cellIdx = 0;
					for (int l = 0; l < level; l++) {
						if (l == 0) {
							cellIdx += 1;
						} else if (l == 2) {
							cellIdx += 3;
						} else {
							cellIdx += pow(l + 1, 2.0);
						}
					}
					cellIdx += cellY;

					int xmin = 0;
					int xmax = RESIZED_IMAGE_SIZE;
					int ymin = (RESIZED_IMAGE_SIZE / cellLength) * cellY;
					int ymax = (RESIZED_IMAGE_SIZE / cellLength) * (cellY + 1);

					for (int i = 0; i < indices.rows; i++) {
						if (keypoints[i].pt.x >= xmin
								&& keypoints[i].pt.x < xmax
								&& keypoints[i].pt.y >= ymin
								&& keypoints[i].pt.y < ymax) {
							int idx = indices.at<int>(i, 0);
							bof.at<double>(K * cellIdx + idx) = bof.at<double>(
									K * cellIdx + idx) + 1.0;
						}
					}
				}
			} else {
				int cellLength = level + 1;
				for (int cellX = 0; cellX < cellLength; cellX++) {
					for (int cellY = 0; cellY < cellLength; cellY++) {
						int cellIdx = 0;
						for (int l = 0; l < level; l++) {
							if (l == 0) {
								cellIdx += 1;
							} else if (l == 2) {
								cellIdx += 3;
							} else {
								cellIdx += pow(l + 1, 2.0);
							}
						}
						cellIdx += (cellY * cellLength + cellX);

						int xmin = (RESIZED_IMAGE_SIZE / cellLength) * cellX;
						int xmax = (RESIZED_IMAGE_SIZE / cellLength)
								* (cellX + 1);
						int ymin = (RESIZED_IMAGE_SIZE / cellLength) * cellY;
						int ymax = (RESIZED_IMAGE_SIZE / cellLength)
								* (cellY + 1);

						for (int i = 0; i < indices.rows; i++) {
							if (keypoints[i].pt.x >= xmin
									&& keypoints[i].pt.x < xmax
									&& keypoints[i].pt.y >= ymin
									&& keypoints[i].pt.y < ymax) {
								int idx = indices.at<int>(i, 0);
								bof.at<double>(K * cellIdx + idx) = bof.at<
										double>(K * cellIdx + idx) + 1.0;
							}
						}
					}
				}
			}
		}

#pragma omp parallel for
		for (int c = 0; c < cellNums; c++) {
			for (int i = 0; i < K; i++) {
				bof.at<double>(K * c + i) = bof.at<double>(K * c + i)
						/ (double) (descriptors.rows);
			}
		}

		switch (NORM_BOF_FEATURE_TYPE) {
		case L2_NORM: {
#pragma omp parallel for
			for (int c = 0; c < cellNums; c++) {
				double norm = 0.0;
				for (int i = 0; i < K; i++) {
					norm += pow(bof.at<double>(K * c + i), 2.0);
				}
				norm = sqrt(norm);

				if (norm > 0.0) {
					for (int i = 0; i < K; i++) {
						bof.at<double>(K * c + i) = bof.at<double>(K * c + i)
								/ norm;
					}
				}
			}
			break;
		}
		case L1_NORM_SQUARE_ROOT: {
#pragma omp parallel for
			for (int c = 0; c < cellNums; c++) {
				double norm = 0.0;
				for (int i = 0; i < K; i++) {
					norm += bof.at<double>(K * c + i);
				}

				if (norm > 0.0) {
					for (int i = 0; i < K; i++) {
						bof.at<double>(K * c + i) = sqrt(
								bof.at<double>(K * c + i) / norm);
					}
				}
			}
			break;
		}
		case NONE: {
			break;
		}
		default: {
			CV_Error(CV_StsBadArg, "invalid norm feature type");
			break;
		}
		}
	} else {
		bof = cv::Mat::zeros(K, 1, CV_64F);
		for (int i = 0; i < indices.rows; i++) {
			int idx = indices.at<int>(i, 0);
			bof.at<double>(idx) = bof.at<double>(idx) + 1.0;
		}

		for (int i = 0; i < K; i++) {
			bof.at<double>(i) = bof.at<double>(i) / (double) (descriptors.rows);
		}

		switch (NORM_BOF_FEATURE_TYPE) {
		case L2_NORM: {
			double norm = cv::norm(bof);
			if (norm > 0.0) {
				for (int i = 0; i < K; i++) {
					bof.at<double>(i) = bof.at<double>(i) / norm;
				}
			}
			break;
		}
		case L1_NORM_SQUARE_ROOT: {
			double norm = 0.0;
			for (int i = 0; i < K; i++) {
				norm += bof.at<double>(i);
			}
			if (norm > 0.0) {
				for (int i = 0; i < K; i++) {
					bof.at<double>(i) = sqrt(bof.at<double>(i) / norm);
				}
			}
			break;
		}
		case NONE: {
			break;
		}
		default: {
			CV_Error(CV_StsBadArg, "invalid norm feature type");
			break;
		}
		}
	}

	return bof;
}

int BoFSpatialPyramids::getDim() {
	CV_Assert(K > 0);
	CV_Assert(PYRAMID_LEVEL <= 3);

	if (USE_SPATIAL_PYRAMID) {
		int cellNums = 0;
		for (int level = 0; level < PYRAMID_LEVEL; level++) {
			if (level == 0) {
				cellNums += 1;
			} else if (level == 2) {
				cellNums += 3;
			} else {
				cellNums += pow(level + 1, 2.0);
			}
		}

		return K * cellNums;
	} else {
		return K;
	}
}
