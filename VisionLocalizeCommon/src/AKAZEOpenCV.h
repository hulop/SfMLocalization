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

#include <opencv2/opencv.hpp>
#include <openMVG/sfm/sfm_data.hpp>
#include <openMVG/features/regions.hpp>

#include "AKAZEOption.h"

namespace hulo {

// extract AKAZE from one image
// also return width and height of image
extern std::unique_ptr<openMVG::features::Regions> extractAKAZESingleImg(std::string &filename,
		std::string &outputFolder, const hulo::AKAZEOption &akazeOption,
		std::vector<std::pair<float, float> > &locFeat, std::size_t &w,
		std::size_t &h);

// extract AKAZE from all images listed in SfM data file
extern void extractAKAZE(const openMVG::sfm::SfM_Data &sfm_data,
		std::string &outFolder, const hulo::AKAZEOption &akazeOption);

}
