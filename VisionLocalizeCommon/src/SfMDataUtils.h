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
#include <openMVG/sfm/sfm_data.hpp>
#include <openMVG/sfm/pipelines/sfm_matches_provider.hpp>

namespace hulo {

// Hash_Map is std::unordered_map
// Hash_Map<View key, <Feature key, 3D Pt key>>
typedef openMVG::Hash_Map<std::size_t, openMVG::Hash_Map<std::size_t, std::size_t> > MapViewFeatTo3D;

// set of matching indices of 2D (from query image) to 3D (reconstructed model) matches
typedef std::vector<std::pair<std::size_t, std::size_t> > QFeatTo3DFeat;

// convert sfm_data.getLandmarks() to MapViewFeatTo3D
extern void structureToMapViewFeatTo3D(const openMVG::sfm::Landmarks &landmarks, MapViewFeatTo3D &map);

/*
 * Return a set of pairs where each pair <size_t,size_t> is <key of query image feature, key of 3D
 * reconstructed point>. This function takes matches_provider and landmarks (from SfM_Data.getLandmarks())
 * as inputs. Note that the second member of each pair of matches in matches_provider must be key of
 * feature from query image.
 *
 * featDist is a map from (viewID1,viewID2) to a map from featID of viewID1 to distance to its nearest neighbor in viewID2
 *
 * In this function, if a 2D feature matches to multiple 3D feature then the only one closest by NN distance is retained
 *
 */
extern void matchProviderToMatchSet(const std::shared_ptr<openMVG::sfm::Matches_Provider> &match_provider,
		const MapViewFeatTo3D &mapViewFeatTo3D,
		QFeatTo3DFeat &map23,
		std::map<std::pair<std::size_t, std::size_t>, std::map<std::size_t, int> > &featDist);

// generate list of pairs (i,j) where i < j for i = 0 ... n-1 , j = 0 ... n-1
extern void generateAllPairs(const openMVG::sfm::SfM_Data &sfm_data,
		std::vector<std::pair<std::size_t, std::size_t>> &pairs);

// generate list of pairs (i,j) where i < j for i = 0 ... n-1 , j = 0 ... min(i+frame,n)
extern void generateVideoMatchPairs(const openMVG::sfm::SfM_Data &sfm_data,
		std::vector<std::pair<std::size_t, std::size_t>> &pairs, int frame);

extern void orderPair(std::pair<size_t, size_t> &p);

// remove duplicate pair
extern void removeDupPairs(std::vector<std::pair<size_t, size_t>> &pairs);

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 2015/07/01 accelerate matching by match 3D features to query image
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// get list of features in reconstruction in each image
extern void listReconFeat(const openMVG::sfm::SfM_Data &sfm_data,
		std::map<int, std::vector<int>> &imgFeatList);

///////////////////////////////////////////////////////////////////////////////////
// 2015 / 07 / 09 Code for search within local area of a position
extern void getLocalViews(openMVG::sfm::SfM_Data &sfm_data,
		const openMVG::Vec3 &loc, const double radius,
		std::set<openMVG::IndexT> &localViews);

}
