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

#include "FileUtils.h"

#include <fstream>
#include <openMVG/matching/indMatch_utils.hpp>
#include "StringUtils.h"

void hulo::saveMat(std::string filename, cv::Mat& mat) {
	cv::FileStorage storage(filename, cv::FileStorage::WRITE);
	storage << "mat" << mat;
	storage.release();
}

void hulo::readMat(std::string filename, cv::Mat &mat) {
	cv::FileStorage storage(filename, cv::FileStorage::READ);
	storage["mat"] >> mat;
	storage.release();
}

void hulo::saveMatBin(std::string filename, cv::Mat& mat) {
	std::ofstream ofs(filename, std::ios::binary);
	CV_Assert(ofs.is_open());

	if (mat.empty()) {
		int s = 0;
		ofs.write((const char*) (&s), sizeof(int));
		return;
	}

	int type = mat.type();
	ofs.write((const char*) (&mat.rows), sizeof(int));
	ofs.write((const char*) (&mat.cols), sizeof(int));
	ofs.write((const char*) (&type), sizeof(int));
	ofs.write((const char*) (mat.data), mat.elemSize() * mat.total());
}

void hulo::readMatBin(std::string filename, cv::Mat& mat) {
	std::ifstream ifs(filename, std::ios::binary);
	CV_Assert(ifs.is_open());

	int rows, cols, type;
	ifs.read((char*) (&rows), sizeof(int));
	if (rows == 0) {
		return;
	}
	ifs.read((char*) (&cols), sizeof(int));
	ifs.read((char*) (&type), sizeof(int));

	mat.release();
	mat.create(rows, cols, type);
	ifs.read((char*) (mat.data), mat.elemSize() * mat.total());
}

// export geometric map out
void hulo::exportPairWiseMatches(
		openMVG::matching::PairWiseMatches &map_geometricMatches,
		std::string filename) {
	// write to file
	std::ofstream fileOut(filename);
	if (fileOut.is_open()) {
		PairedIndMatchToStream(map_geometricMatches, fileOut);
	} else {
		std::cerr << "Cannot write geometric matches file" << filename
				<< std::endl;
	}
	fileOut.close();
}

// export match file
void hulo::exportMatch(
		std::map<std::pair<size_t, size_t>, std::vector<cv::DMatch>> &matches,
		std::string filename) {
	std::ofstream outStream(filename);
	if (outStream.is_open()) {

		for (std::map<std::pair<size_t, size_t>, std::vector<cv::DMatch>>::const_iterator iter =
				matches.begin(); iter != matches.end(); iter++) {
			outStream << iter->first.first << " " << iter->first.second
					<< std::endl;
			outStream << iter->second.size() << std::endl;

			// write each match
			for (std::vector<cv::DMatch>::const_iterator iterM =
					iter->second.begin(); iterM != iter->second.end();
					iterM++) {
				outStream << iterM->queryIdx << " " << iterM->trainIdx
						<< std::endl;
			}
		}

		outStream.close();
	} else {
		std::cerr << "Cannot write match results to " << filename << std::endl;
	}
}

// read location from .feat file
void hulo::readFeatLoc(const std::string &readFeatLoc, cv::Mat &mat) {
	std::ifstream fileRead(readFeatLoc);
	std::vector<std::pair<float, float>> loc;

	if (fileRead.is_open()) {
		float a;
		float b;
		float c;
		float d;

		// read lines
		while (fileRead >> a >> b >> c >> d) {
			loc.push_back(std::make_pair(a, b));
		}

		// put in matrix
		mat = cv::Mat::zeros(loc.size(), 2, CV_32F);

		for (int i = 0; i < loc.size(); i++) {
			mat.at<float>(i, 0) = loc.at(i).first;
			mat.at<float>(i, 1) = loc.at(i).second;
		}
	} else {
		std::cout << "Warning: Cannot read " << readFeatLoc
				<< " for putative matching" << std::endl;
	}
}

// read file which has list of pair numbers
void hulo::readPairFile(const std::string &sPairFile, std::vector<std::pair<std::size_t, std::size_t>> &pairs){
	std::ifstream pairfile(sPairFile);

	if(pairfile.is_open()){
		std::string line;
		while(std::getline(pairfile,line)){
			size_t first = (size_t) stoi(hulo::stringTok(line));
			size_t second = (size_t) stoi(hulo::stringTok(line));
			pairs.push_back(std::make_pair(first, second));
		}
	}

	pairfile.close();
}
