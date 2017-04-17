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

#include <openMVG/version.hpp>
#include <openMVG/sfm/pipelines/sfm_regions_provider.hpp>

namespace hulo {

	struct HuloSfMRegionsProvider : public openMVG::sfm::Regions_Provider {
	public:
		void set(const openMVG::IndexT x, std::unique_ptr<openMVG::features::Regions>& regions_ptr) {
#if (OPENMVG_VERSION_MAJOR<1 || OPENMVG_VERSION_MINOR<1)
			regions_per_view[x] = std::move(regions_ptr);
#else
			cache_[x] = std::move(regions_ptr);
#endif
		}

		void erase(const openMVG::IndexT x) {
#if (OPENMVG_VERSION_MAJOR<1 || OPENMVG_VERSION_MINOR<1)
			regions_per_view.erase(x);
#else
			cache_.erase(x);
#endif
		}
	};

}