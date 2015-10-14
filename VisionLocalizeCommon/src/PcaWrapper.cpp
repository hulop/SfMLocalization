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

#include "PcaWrapper.h"

using namespace hulo;

PcaWrapper::PcaWrapper() {
	pca = new cv::PCA();
}

PcaWrapper::PcaWrapper(const cv::Mat &pcaTrainFeatures, int dimPCA) {
	pca = new cv::PCA(pcaTrainFeatures, cv::Mat(), CV_PCA_DATA_AS_ROW, 0);
	mDimPCA = dimPCA;

	cv::Mat evalues = pca->eigenvalues;
	float sum = 0.0;
	for (int i = 0; i < pca->eigenvalues.rows; i++) {
		sum += ((float*) evalues.data)[i];
	}
	float contribution = 0.0;
	for (int i = 0; i < mDimPCA; i++) {
		contribution += ((float*) evalues.data)[i] / sum;
		std::cout << "PCA Dim = " << i + 1 << ", contribution rate : "
				<< contribution << std::endl;
	}
}

PcaWrapper::~PcaWrapper() {
	if (pca)
		delete pca;
}

void PcaWrapper::write(cv::FileStorage& fs) const {
	fs << "DimPCA" << mDimPCA;
	fs << "EigenVectorsPCA" << pca->eigenvectors;
	fs << "EigenValuesPCA" << pca->eigenvalues;
	fs << "MeanPCA" << pca->mean;
}

void PcaWrapper::read(const cv::FileNode& node) {
	node["DimPCA"] >> mDimPCA;
	node["EigenVectorsPCA"] >> pca->eigenvectors;
	node["EigenValuesPCA"] >> pca->eigenvalues;
	node["MeanPCA"] >> pca->mean;
}

cv::Mat PcaWrapper::calcPcaProject(const cv::Mat &descriptors) {
	CV_Assert(descriptors.type() == CV_32F);

	/*
	 // slow for large dimensional data
	 Mat results = pca->project(descriptors).colRange(0, mDimPCA).clone();
	 for (int d=0; d<mDimPCA; d++){
	 results.col(d) = results.col(d)/((float*)pca->eigenvalues.data)[d];
	 }
	 return results;
	 */
	cv::Mat results(descriptors.rows, mDimPCA, CV_32F);
#pragma omp parallel for
	for (int i = 0; i < descriptors.rows; i++) {
		cv::Mat pcaDesc =
				pca->project(descriptors.row(i)).colRange(0, mDimPCA).clone();
		for (int d = 0; d < mDimPCA; d++) {
			results.at<float>(i, d) = pcaDesc.at<float>(0, d)
					/ ((float*) pca->eigenvalues.data)[d];
		}
	}
	return results;
}
