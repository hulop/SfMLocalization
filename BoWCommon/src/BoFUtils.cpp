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

#include "BoFUtils.h"

#include <FileUtils.h>

void hulo::selectViewByBoF(const cv::Mat& bow, string& matchDir,
		set<openMVG::IndexT> &viewList, openMVG::sfm::SfM_Data &sfm_data, int knn,
		set<openMVG::IndexT> &selectedViewList) {
	CV_Assert(knn < viewList.size());

	cv::Mat bofMat;
	for(set<openMVG::IndexT>::const_iterator iter = viewList.begin(); iter != viewList.end(); iter++){
		openMVG::IndexT viewIndex = *iter;
		const openMVG::sfm::View* view = sfm_data.views.at(viewIndex).get();
		string bofFile = stlplus::create_filespec(matchDir,stlplus::basename_part(view->s_Img_path),"bow");
		cv::Mat bofVec;
		//hulo::readMat(bofFile, bofVec);
		hulo::readMatBin(bofFile, bofVec);
		cv::transpose(bofVec, bofVec);
		bofMat.push_back(bofVec);
	}
	if (bofMat.type()!=CV_32FC1) {
		bofMat.convertTo(bofMat, CV_32FC1);
	}
	const cv::Ptr<cv::flann::IndexParams>& indexParams=new cv::flann::KDTreeIndexParams(4);
	const cv::Ptr<cv::flann::SearchParams>& searchParams=new cv::flann::SearchParams(64);
	cv::FlannBasedMatcher matcher(indexParams, searchParams);
	matcher.add(bofMat);
	matcher.train();

	cv::Mat queryBow;
	cv::transpose(bow, queryBow);
	if (queryBow.type()!=CV_32FC1) {
		queryBow.convertTo(queryBow, CV_32FC1);
	}

	vector<vector<cv::DMatch>> matchesOut;
	matcher.knnMatch(queryBow, matchesOut, knn);

	selectedViewList.clear();
	for (int i=0; i<matchesOut[0].size(); i++) {
		int viewId = matchesOut[0][i].trainIdx;
		std::set<openMVG::IndexT>::iterator it = viewList.begin();
		std::advance(it, viewId);
		selectedViewList.insert(*it);
	}
}
