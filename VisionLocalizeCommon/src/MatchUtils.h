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

#include <vector>
#include <omp.h>

#include <openMVG/sfm/sfm_data.hpp>
#include <openMVG/matching/indMatch.hpp>

namespace hulo {

// match pairs from a list
// filenames : list of filenames of descriptor file
// pairs : list of pairs to match
// matches : list of matches for each pair
extern void matchAKAZE(const openMVG::sfm::SfM_Data &sfm_data,
		const std::string &sMatchesDir,
		const std::vector<std::pair<std::size_t, std::size_t> > &pairs,
		const float fDistRatio, openMVG::matching::PairWiseMatches &matches);

// perform tracking on AKAZE and return as match
// maxFrameDist : maximum distance between frame to write as match (though track length can be longer)
extern void trackAKAZE(const openMVG::sfm::SfM_Data &sfm_data,
		const std::string &sMatchesDir, const std::size_t maxFrameDist,
		const float fDistRatio, openMVG::matching::PairWiseMatches &matches);

// match pairs from a list
// filenames : list of filenames of descriptor file
// pairs : list of pairs to match
// matches : list of matches for each pair
extern void matchAKAZEToQuery(const openMVG::sfm::SfM_Data &sfm_data,
		const std::string &sMatchesDir,
		const std::vector<std::size_t> &pairs, // list of viewID to match to
		const std::size_t queryInd, // index of query
		const float fDistRatio, openMVG::matching::PairWiseMatches &matches,
		std::map<std::pair<std::size_t, std::size_t>, std::map<std::size_t, int>> &featDist // map from pair of (viewID,viewID) to map of featID of queryImg to distance between the feature and its match in reconstructed images
		);

// perform geometric match
// use openMVG function
// inputting sfm_data instead of list of files because also need size of each image
extern void geometricMatch(openMVG::sfm::SfM_Data &sfm_dataFull, // list of file names
		const std::string &matchesDir, // location of .feat files
		openMVG::matching::PairWiseMatches &map_putativeMatches, // reference to putative matches
		openMVG::matching::PairWiseMatches &map_geometricMatches, // reference to output
		int ransacRound, // number of ransacRound
		double geomPrec //precision of geometric matching
		);

// match pairs from a list to a query file
// sfm_data : list of filenames of descriptor file
// pairs : list of pairs to match
// matches : list of matches for each pair
extern void matchAKAZE3DToQuery(const openMVG::sfm::SfM_Data &sfm_data,
		const std::string &sMatchesDir,
		int qIndex, // index of query image in sfm_data
		const float fDistRatio, openMVG::matching::PairWiseMatches &matches,
		std::set<openMVG::IndexT> &localViews);

}
