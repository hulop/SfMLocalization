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

#pragma once

#include <opencv2/opencv.hpp>
#include <openMVG/matching/indMatch.hpp>

namespace hulo {

extern void saveMat(std::string filename, cv::Mat& mat);
extern void readMat(std::string filename, cv::Mat &mat);
extern void saveMatBin(std::string filename, cv::Mat& mat);
extern void readMatBin(std::string filename, cv::Mat& mat);
extern void saveAKAZEBin(std::string filename, cv::Mat& mat);
extern void readAKAZEBin(std::string filename, cv::Mat& mat);

// export geometric map out
extern void exportPairWiseMatches(
		openMVG::matching::PairWiseMatches &map_geometricMatches,
		std::string filename);

// export match file
extern void exportMatch(
		std::map<std::pair<size_t, size_t>, std::vector<cv::DMatch>> &matches,
		std::string filename);

// read location from .feat file
extern void readFeatLoc(const std::string &readFeatLoc, cv::Mat &mat);

// read file which has list of pair numbers
extern void readPairFile(const std::string &sPairFile,
		std::vector<std::pair<size_t, size_t>> &pairs);

}
