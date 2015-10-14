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

#include "MatUtils.h"

// calculate pairwise distance between rows of two matrices
void hulo::calPairWiseDistanceEuclidean(const cv::Mat &A, const cv::Mat &B,
		cv::Mat &D) {
	D = cv::Mat::zeros(A.rows, B.rows, CV_32F);

	for (int i = 0; i < A.rows; i++) {
		for (int j = 0; j < B.rows; j++) {
			D.at<float>(i, j) = cv::norm(A.row(i), B.row(j));
		}
	}
}

// copy selected rows from matrix
void hulo::copyRowsMat(const cv::Mat &inputMat,
		const std::vector<int> &listValidRows, cv::Mat &outputMat) {
	for (std::size_t i = 0; i < listValidRows.size(); i++) {
		outputMat.push_back(inputMat.row(listValidRows[i]));
	}
}
