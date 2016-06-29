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

#include <types.hpp>
#include <cstdlib>
#include <fstream>
#include <iostream>

#include <opencv2/opencv.hpp>
#include <openMVG/sfm/sfm_data.hpp>
#include <openMVG/matching/indMatch_utils.hpp>
#include <openMVG/features/regions.hpp>
#include <openMVG/features/regions_factory.hpp>
#include <sfm/sfm_data_io.hpp>

#include "AKAZEOption.h"
#include "AKAZEOpenCV.h"
#include "SfMDataUtils.h"
#include "MatchUtils.h"
#include "FileUtils.h"

using namespace std;
using namespace cv;
using namespace openMVG::sfm;
using namespace openMVG::features;
using namespace openMVG::matching;
using namespace hulo;

const String keys =
		"{h help 			|| print this message}"
		"{@matchdir			||Folder of descriptor/feature/match files}"
		"{c akazeChannel	|3|AKAZE channel}"
		"{t akazeThreshold	|0.001|AKAZE threshold}"
		"{o akazeNOctave	|4|AKAZE octaves}"
		"{l akazeOctaveLayer|4|AKAZE layers per octave}"
		"{f fdistratio		|0.6|Ratio for putative matching ratio test}"
		"{r ransacround		|4096|RANSAC round for geometric matching}"
		"{v videoMatchFrame	|0|match using between close frames (video mode)}"
		"{p pairfile		||pair file for matching, overwritten by tracking by matching}"
		"{mf maxFrameDist	|0|maximum number of frame to extend match via tracking to}"
		"{mm minMatch		|60|minimum number of matches between frames to keep them connected}"
		"{g geomError		|4.0|geometric error}"
		"{sm skipMathing	|false|run only feature extraction and skip matching}";

int main(int argc, char **argv) {
	CommandLineParser parser(argc, argv, keys);
	parser.about("Extract feature and compute matching for all images in SfM project");
	if (argc < 2) {
		parser.printMessage();
		return 1;
	}
	if (parser.has("h")) {
		parser.printMessage();
		return 1;
	}
	std::string sMatchesDir = parser.get<string>(0);
	int akazeChannel = parser.get<int>("c");
	float akazeThres = parser.get<float>("t");
	int akazeNOct = parser.get<int>("o");
	int akazeNOctLay = parser.get<int>("l");
	float fDistRatio = parser.get<float>("f");
	int ransacRound = parser.get<int>("r");
	int videoMatchFrame = parser.get<int>("v");
	std::string sPairFile = parser.get<std::string>("p"); // pair file for matching, overwritten by tracking by matching
	size_t maxFrameDist = parser.get<size_t>("mf"); // number of frame to extend match via tracking to
	int minMatch = parser.get<int>("mm"); // minimum number of matches between frames to keep them connected
	int geomError = parser.get<int>("g");
	bool bSkipMatching = parser.get<bool>("sm");
	bool bGuided_matching = false;

    if (!parser.check() || sMatchesDir.size()==0) {
        parser.printMessage();
        return 1;
    }

	// You cannot specify more than two parameters from the three parameters (pair file, number of video frame to make pair, number of frame to track)
	CV_Assert((sPairFile.size()==0 && videoMatchFrame==0 && maxFrameDist==0)
			|| (sPairFile.size()>0 && videoMatchFrame==0 && maxFrameDist==0)
			|| (sPairFile.size()==0 && videoMatchFrame>0 && maxFrameDist==0)
			|| (sPairFile.size()==0 && videoMatchFrame==0 && maxFrameDist>0));

	cout << "Matches directory : " << sMatchesDir << endl;

	// load sfm_data to list image files
	SfM_Data sfm_data;
	if (!Load(sfm_data, stlplus::create_filespec(sMatchesDir, "sfm_data.json"),
			ESfM_Data(VIEWS | INTRINSICS))) {
		cerr << "Cannot load "
				<< stlplus::create_filespec(sMatchesDir, "sfm_data.json")
				<< endl;
		return EXIT_FAILURE;
	}

	// list image files and generate paths for desc and feat files
	vector<string> listFile;
	vector<string> listFileD;
	vector<string> listFileF;
	for (Views::const_iterator iter = sfm_data.views.begin();
			iter != sfm_data.views.end(); iter++) {
		listFile.push_back(
				stlplus::create_filespec(sfm_data.s_root_path,
						stlplus::basename_part(iter->second->s_Img_path),
						stlplus::extension_part(iter->second->s_Img_path)));
		listFileD.push_back(
				stlplus::create_filespec(sMatchesDir,
						stlplus::basename_part(iter->second->s_Img_path),
						"desc"));
		listFileF.push_back(
				stlplus::create_filespec(sMatchesDir,
						stlplus::basename_part(iter->second->s_Img_path),
						"feat"));
	}

	// create and save image describer file
	string sImageDesc = stlplus::create_filespec(sMatchesDir,
			"image_describer.txt");
	AKAZEOption akazeOption = AKAZEOption(akazeChannel, akazeThres, akazeNOct,
			akazeNOctLay);
	akazeOption.write(sImageDesc);

	// extract features
	hulo::extractAKAZE(sfm_data, sMatchesDir, akazeOption);

	if (bSkipMatching) {
		cout << "Skip matching option is set. Exit without feature matching." << endl;
		return 1;
	}

	// perform putative matching
	cout << "Start Putative Matching..." << endl;
	if (!stlplus::file_exists(
			stlplus::create_filespec(sMatchesDir, "matches.putative.txt"))) {
		PairWiseMatches matches;
		if (maxFrameDist != 0) {
			hulo::trackAKAZE(sfm_data, sMatchesDir, maxFrameDist, fDistRatio, matches);
		} else {
			vector<pair<size_t, size_t>> pairs;
			cout << "Generating pairs" << endl;

			// generate list of pairs via video match
			if (sPairFile != "") {
				hulo::readPairFile(sPairFile, pairs);
			} else {
				bool genPair = false;
				if (videoMatchFrame > 0) {
					hulo::generateVideoMatchPairs(sfm_data, pairs, videoMatchFrame);
					genPair = true;
				}

				if (!genPair) {
					// if no pairs generated, do all matches
					hulo::generateAllPairs(sfm_data, pairs);
				} else {
					// remove duplicate pairs
					hulo::removeDupPairs(pairs);
				}
			}

			for (vector<pair<size_t, size_t>>::const_iterator iter =
					pairs.begin(); iter != pairs.end(); iter++) {
				cout << "(" << iter->first << " " << iter->second << ") ";
			}
			cout << endl;
			cout << "Total number of pairs : " << pairs.size() << endl;

			hulo::matchAKAZE(sfm_data, sMatchesDir, pairs, fDistRatio, matches);
		}
		// export result
		hulo::exportPairWiseMatches(matches,
				stlplus::create_filespec(sMatchesDir, "matches.putative.txt"));
	}

	// perform geometric matching
	cout << "Start Geometric Matching..." << endl;
	if (!stlplus::file_exists(
			stlplus::create_filespec(sMatchesDir, "matches.f.txt"))) {
		PairWiseMatches map_geometricMatches;

		// load pair wise matches
		PairWiseMatches map_putativeMatches;
		if (stlplus::file_exists(
				stlplus::create_filespec(sMatchesDir,
						"matches.putative.txt"))) {

#if (OPENMVG_VERSION_MAJOR<1)
			PairedIndMatchImport(stlplus::create_filespec(sMatchesDir,"matches.putative.txt"),
					map_putativeMatches);
#else
			openMVG::matching::Load(map_putativeMatches,
					stlplus::create_filespec(sMatchesDir, "matches.putative.txt"));
#endif
		} else {
			cerr << "Cannot read putative matches file "
					<< stlplus::create_filespec(sMatchesDir,
							"matches.putative.txt") << endl;
			return EXIT_FAILURE;
		}

		for (auto iterMatch = map_putativeMatches.cbegin();
				iterMatch != map_putativeMatches.end();) {
			if (iterMatch->second.size() < minMatch) {
				cout << iterMatch->first.first << "," << iterMatch->first.second
						<< "," << iterMatch->second.size() << " ";
				map_putativeMatches.erase(iterMatch++);
			} else {
				++iterMatch;
			}
		}
		cout << endl;

		// construct feature providers for geometric matches
		unique_ptr<Regions> regions;
		regions.reset(new AKAZE_Binary_Regions);
		shared_ptr<Regions_Provider> regions_provider = make_shared<Regions_Provider>();
		if (!regions_provider->load(sfm_data, sMatchesDir, regions)) {
			cerr << "Cannot construct regions providers," << endl;
			return EXIT_FAILURE;
		}

		// compute geometric matches
		hulo::geometricMatch(sfm_data, regions_provider, map_putativeMatches,
				map_geometricMatches, ransacRound, geomError, bGuided_matching);
		hulo::exportPairWiseMatches(map_geometricMatches,
				stlplus::create_filespec(sMatchesDir, "matches.f.txt")); // export result
	}
}
