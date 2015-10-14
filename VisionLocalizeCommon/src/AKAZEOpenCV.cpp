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

#include "AKAZEOpenCV.h"
#include "FileUtils.h"

#include <fstream>

using namespace std;
using namespace cv;
using namespace openMVG::sfm;

// extract AKAZE from one image
// also return width and height of image
void hulo::extractAKAZESingleImg(string &filename, string &outputFolder,
		const AKAZEOption &akazeOption, vector<pair<float, float>> &locFeat,
		size_t &w, size_t &h) {

	cout << "Extracting AKAZE" << endl;

	// generate AKAZE object
	Ptr < AKAZE > akaze = AKAZE::create(
			AKAZE::DESCRIPTOR_MLDB, 0, akazeOption.desc_ch, akazeOption.thres,
			akazeOption.nOct, akazeOption.nOctLay);

	// loop over files
	cv::Mat img;
	vector<KeyPoint> kpts;
	cv::Mat desc;

	// write out to file
	string filebase = stlplus::create_filespec(outputFolder,
			stlplus::basename_part(filename));
	string sFeat = filebase + ".feat";
	string sDesc = filebase + ".desc";

	// read image
	img = imread(filename, IMREAD_GRAYSCALE);
	h = (size_t) img.rows;
	w = (size_t) img.cols;

	if (!stlplus::file_exists(sFeat) || !stlplus::file_exists(sDesc)) {

		// extract features
		akaze->detectAndCompute(img, noArray(), kpts, desc);

		// write out
		ofstream fileFeat(sFeat);
		FileStorage fileDesc(sDesc, FileStorage::WRITE);

		if (fileFeat.is_open() && fileDesc.isOpened()) {
			// save each feature write each feature to file
			for (vector<KeyPoint>::const_iterator iterKP = kpts.begin();
					iterKP != kpts.end(); iterKP++) {
				locFeat.push_back(
						make_pair(static_cast<float>(iterKP->pt.x),
								static_cast<float>(iterKP->pt.y)));
				fileFeat << iterKP->pt.x << " " << iterKP->pt.y << " "
						<< iterKP->size << " " << iterKP->angle << endl;
			}
			fileFeat.close();

			// write descriptor to file
			if (SAVE_DESC_BINARY) {
				fileDesc.release();
				hulo::saveMatBin(sDesc, desc);
			} else {
				fileDesc << "Descriptors" << desc;
				fileDesc.release();
			}
		} else {
			cerr << "cannot open file to write features for " << filebase
					<< endl;
		}
	} else {
		// read feature to fill locFeat
		ifstream fileFeat(sFeat);
		string line;
		while (getline(fileFeat, line)) {
			stringstream linestream(line);
			float x, y, s, o;
			linestream >> x;
			linestream >> y;
			linestream >> s;
			linestream >> o;
			locFeat.push_back(make_pair(x, y));
		}
	}
}

// extract AKAZE from all images listed in SfM data file
void hulo::extractAKAZE(const SfM_Data &sfm_data, string &outFolder,
		const AKAZEOption &akazeOption) {
	double start = (double) getTickCount();
	cout << "Extracting AKAZE" << endl;

	// generate AKAZE object
	Ptr < AKAZE > akaze = AKAZE::create(
			AKAZE::DESCRIPTOR_MLDB, 0, akazeOption.desc_ch, akazeOption.thres,
			akazeOption.nOct, akazeOption.nOctLay);

	// loop over files in parallel
	vector<string> imageFiles;
	vector<string> featFiles;
	vector<string> descFiles;
	for (Views::const_iterator iter = sfm_data.views.begin();
			iter != sfm_data.views.end(); iter++) {

		string filename = stlplus::create_filespec(sfm_data.s_root_path,
				stlplus::basename_part(iter->second->s_Img_path),
				stlplus::extension_part(iter->second->s_Img_path));
		cout << filename << endl;

		// write out to file
		string filebase = stlplus::create_filespec(outFolder,
				stlplus::basename_part(filename));
		string sFeat = filebase + ".feat";
		string sDesc = filebase + ".desc";

		imageFiles.push_back(filename);
		featFiles.push_back(sFeat);
		descFiles.push_back(sDesc);
	}

#pragma omp parallel for
	for (int i = 0; i < imageFiles.size(); i++) {
		// if desc or feat not exist, compute feature
		if (!stlplus::file_exists(featFiles[i])
				|| !stlplus::file_exists(descFiles[i])) {
			// read image
			cv::Mat img = imread(imageFiles[i], IMREAD_GRAYSCALE);

			// extract features
			vector<KeyPoint> kpts;
			cv::Mat desc;
			akaze->detectAndCompute(img, noArray(), kpts, desc);

			// write out
			ofstream fileFeat(featFiles[i]);
			FileStorage fileDesc(descFiles[i], FileStorage::WRITE);

			if (fileFeat.is_open() && fileDesc.isOpened()) {

				// write each feature to file
				for (vector<KeyPoint>::const_iterator iterKP = kpts.begin();
						iterKP != kpts.end(); iterKP++) {
					fileFeat << iterKP->pt.x << " " << iterKP->pt.y << " "
							<< iterKP->size << " " << iterKP->angle << endl;
				}
				fileFeat.close();

				// write descriptor to file
				if (SAVE_DESC_BINARY) {
					fileDesc.release();
					hulo::saveMatBin(descFiles[i], desc);
				} else {
					fileDesc << "Descriptors" << desc;
					fileDesc.release();
				}
			} else {
				cerr << "cannot open file to write features for "
						<< imageFiles[i] << endl;
			}
		}
	}
	double end = (double) getTickCount();
	cout << "Time to calculate AKAZE : " << (end - start) / getTickFrequency() << " s" << endl;
}

