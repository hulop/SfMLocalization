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

#include "SfMDataUtils.h"

using namespace std;
using namespace openMVG;
using namespace openMVG::sfm;
using namespace openMVG::matching;

// convert sfm_data.getLandmarks() to MapViewFeatTo3D
void hulo::structureToMapViewFeatTo3D(const Landmarks &landmarks, hulo::MapViewFeatTo3D &map){

	// iterate over landmark in landmarks
	for(Landmarks::const_iterator iter = landmarks.begin();	iter != landmarks.end(); iter++){
		const Landmark &lnmk = iter->second;

		// iterate over observation in each landmark
		for(Observations::const_iterator iterObs = lnmk.obs.begin(); iterObs!=lnmk.obs.end(); iterObs++){

			// insert 3D points into map
			map[iterObs->first][iterObs->second.id_feat] = iter->first;
		}
	}
}

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
void hulo::matchProviderToMatchSet(const std::shared_ptr<Matches_Provider> &match_provider,
		const hulo::MapViewFeatTo3D &mapViewFeatTo3D,
		hulo::QFeatTo3DFeat &map23,
		std::map<std::pair<std::size_t, std::size_t>, std::map<std::size_t, int> > &featDist)
{
	std::map<std::size_t, std::pair<std::size_t,float> > mapTmp;

	// iterate over view pairs
	for(PairWiseMatches::const_iterator iter = (match_provider.get()->_pairWise_matches).begin();
			iter != (match_provider.get()->_pairWise_matches).end(); iter++){

		// iterate over match pairs in each view pair
		for(IndMatches::const_iterator iterMatch = (iter->second).begin();
				iterMatch!=(iter->second).end();
				iterMatch++){

			// check that the view exists in mapViewFeatTo3D
			if(mapViewFeatTo3D.find(iter->first.first)!=mapViewFeatTo3D.end()){

				// if the view exists, check that the feature exists in that view
				if(mapViewFeatTo3D.find(iter->first.first)->second.find(iterMatch->_i)
						!=mapViewFeatTo3D.find(iter->first.first)->second.end()){

					// get feature key from query image
					std::size_t qFeatKey = iterMatch->_j;

					// get feature key of 3d reconstruction
					std::size_t reconFeatKey = mapViewFeatTo3D.find(iter->first.first)->second.find(iterMatch->_i)->second;

					// add as a pair
					std::pair<std::size_t, std::size_t> pairInd = std::make_pair(iter->first.first, iter->first.second);
					if(featDist.find(pairInd) != featDist.end()){
						if(featDist[pairInd].find(qFeatKey) != featDist[pairInd].end()){
							// add if this is a new match or the old match is farther than this one
							if(mapTmp.find(qFeatKey) == mapTmp.end() || mapTmp[qFeatKey].second > featDist[pairInd][qFeatKey]){
								mapTmp[qFeatKey] = std::make_pair(reconFeatKey, featDist[pairInd][qFeatKey]);
							}
						}
					}
				}

			}
		}
	}

	// convert map to vector
	for(std::map<std::size_t, std::pair<std::size_t, float> >::const_iterator iterMap = mapTmp.begin();
			iterMap != mapTmp.end(); ++iterMap){
		map23.push_back(std::make_pair(iterMap->first, iterMap->second.first));
	}
}

// generate list of pairs (i,j) where i < j for i = 0 ... n-1 , j = 0 ... n-1
void hulo::generateAllPairs(const SfM_Data &sfm_data,
		vector<pair<size_t, size_t>> &pairs) {
	int n = sfm_data.views.size();
	for (int i = 0; i < n; i++) {
		Views::const_iterator iterI = sfm_data.views.begin();
		advance(iterI, i); // move iterator to the i^th element in views, which may not be view i
		for (int j = i + 1; j < n; j++) {
			Views::const_iterator iterJ = sfm_data.views.begin();
			advance(iterJ, j); // move iterator to the j^th element in views, which may not be view j
			pairs.push_back(
					make_pair(iterI->second->id_view, iterJ->second->id_view)); // make pair from view indices
		}
	}
}

// generate list of pairs (i,j) where i < j for i = 0 ... n-1 , j = 0 ... min(i+frame,n)
void hulo::generateVideoMatchPairs(const SfM_Data &sfm_data,
		vector<pair<size_t, size_t>> &pairs, int frame) {
	int n = sfm_data.views.size();
	for (int i = 0; i < n; i++) {
		Views::const_iterator iterI = sfm_data.views.begin();
		advance(iterI, i); // move iterator to the i^th element in views, which may not be view i
		for (int j = i + 1; j < n && j <= i + frame; j++) {
			Views::const_iterator iterJ = sfm_data.views.begin();
			advance(iterJ, j); // move iterator to the j^th element in views, which may not be view j
			pairs.push_back(
					make_pair(iterI->second->id_view, iterJ->second->id_view)); // make pair from view indices
		}
	}
}

void hulo::orderPair(pair<size_t,size_t> &p){
	if(p.first > p.second){
		size_t tmp = p.first;
		p.first = p.second;
		p.second = tmp;
	}
}

// remove duplicate pair
void hulo::removeDupPairs(vector<pair<size_t,size_t>> &pairs){

	// find duplicate
	vector<size_t> dup;
	for(int i = pairs.size()-1; i > 0; i--){
		for(int j = i-1; j >= 0; j--){
			orderPair(pairs[j]);
			if((pairs[i].first==pairs[j].first && pairs[i].second==pairs[j].second) ||
					(pairs[i].first==pairs[j].second && pairs[i].second==pairs[j].first)){
				dup.push_back(i);
				break;
			}
		}
	}

	// delete duplicate
	for(size_t i = 0; i < dup.size(); i++){
		pairs.erase(pairs.begin()+dup[i]);
	}
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// 2015/07/01 accelerate matching by match 3D features to query image
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// get list of features in reconstruction in each image
void hulo::listReconFeat(const SfM_Data &sfm_data,
		map<int, vector<int>> &imgFeatList) {
	const Landmarks landmarks = sfm_data.structure;
	for (Landmarks::const_iterator iterL = landmarks.begin();
			iterL != landmarks.end(); iterL++) {
		// iterL->second.obs is Observations which is Hash_Map<IndexT, Observation>
		for (Observations::const_iterator iterO = iterL->second.obs.begin();
				iterO != iterL->second.obs.end(); iterO++) {
			imgFeatList[iterO->first].push_back(iterO->second.id_feat);
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////
// 2015 / 07 / 09 Code for search within local area of a position

void hulo::getLocalViews(SfM_Data &sfm_data, const Vec3 &loc,
		const double radius, set<IndexT> &localViews) {

	// iterate through sfm_data
	cout << "delete: ";
	for (Views::const_iterator iter = sfm_data.views.begin();
			iter != sfm_data.views.end(); iter++) {
		if ((sfm_data.poses.find(iter->second->id_pose) != sfm_data.poses.end())
				&& (sfm_data.poses.at(iter->second->id_pose).center() - loc).squaredNorm()
						<= radius) {
			localViews.insert(iter->first);
		} else {
			sfm_data.views.erase(iter->first);
			cout << iter->first << " ";
		}
	}
	cout << endl;
}
