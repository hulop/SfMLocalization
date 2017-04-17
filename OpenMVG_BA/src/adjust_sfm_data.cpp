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

#include <opencv2/opencv.hpp>
#include <openMVG/version.hpp>
#include <openMVG/sfm/sfm_data.hpp>
#include <openMVG/sfm/sfm_data_io.hpp>
#include <openMVG/sfm/pipelines/localization/SfM_Localizer.hpp>
#include <openMVG/sfm/pipelines/sfm_robust_model_estimation.hpp>
#include <openMVG/sfm/sfm_data_BA_ceres.hpp>
#include <openMVG/sfm/sfm_data_filters.hpp>

using namespace std;
using namespace cv;
using namespace openMVG;
using namespace openMVG::sfm;
using namespace openMVG::cameras;
using namespace openMVG::geometry;

#define MINIMUM_VIEW_NUM_TO_ESTIMATAE_CAMERA_POSE  10
#define STRUCTURE_CLEANUP_RESIDUAL_ERROR 4.0
#define STRUCTURE_CLEANUP_ANGLE_ERROR 2.0

const String keys =
		"{h help 			|| print this message}"
		"{@sfm_data			||Input sfm_data json file}"
		"{@sfm_data_out		||Output sfm_data json file}"
		"{c command     	||Command for order of bundle adjustment (BD) [default=no BD] [options:r = rotation, t = translation, i = intrinsic, s = structure, c = clean]. Usage example: c=r,tic,rts means BD with rotation only, then BD with translation and intrinsics follow by cleaning, then BD with rotation translation and structure.}"
		"{r rm_unstable 	|0|Remove unstable pose and observation}";

//
// 1. load sfm_data
// 2. perform bundle adjustment
// 2.1. solve for intrinsic
// 2.2. solve rotation and translation
// 3. save model
//
int main(int argc, char **argv) {
	CommandLineParser parser(argc, argv, keys);
	parser.about("Execute bundle adjustment for sfm_data.json");
	if (argc < 2) {
		parser.printMessage();
		return 1;
	}
	if (parser.has("h")) {
		parser.printMessage();
		return 1;
	}
	string sSfM_data = parser.get<string>(0);
	string sSfM_data_out = parser.get<string>(1);
	string bd_command = parser.get<string>("c");
	bool rm_unstable = parser.get<int>("r") != 0;

    if (!parser.check() || sSfM_data.size()==0 || sSfM_data_out.size()==0) {
    	parser.printMessage();
        return 1;
    }

	cout << "Start bundle adjustment over sfm_data.json." << endl;

	// 1. load sfm_data
	cout << "Reading sfm_data.json file : " << sSfM_data << endl;
	SfM_Data sfm_data;
	if (!Load(sfm_data, sSfM_data,
			ESfM_Data(VIEWS | INTRINSICS | EXTRINSICS | STRUCTURE))) {
		cerr << endl << "The input sfm_data.json file \"" << sSfM_data << "\" cannot be read." << endl;
		return EXIT_FAILURE;
	}

	// list all 2D - 3D point pair for each view and adjust R,t
	bool warning = false; // boolean for warning that there is view not used in reconstruction
#pragma omp parallel for
	for (int i = 0; i < sfm_data.views.size(); ++i) {
		Views::const_iterator iterV = sfm_data.views.begin();
		advance(iterV, i);

		size_t viewID = iterV->second->id_view;

		vector<Vec2> vec2D;
		vector<Vec3> vec3D;
		for (Landmarks::const_iterator iterL = sfm_data.structure.begin();
				iterL != sfm_data.structure.end(); iterL++) {
			if (iterL->second.obs.find(viewID) != iterL->second.obs.end()) {
				Observation ob = iterL->second.obs.at(viewID);
				vec2D.push_back(ob.x);
				vec3D.push_back(iterL->second.X);
			}
		}

		Image_Localizer_Match_Data resection_data;
		resection_data.pt2D.resize(2, vec2D.size());
		resection_data.pt3D.resize(3, vec3D.size());
		for (int j=0; j<vec2D.size(); j++) {
			resection_data.pt2D.col(j) = vec2D[j];
			resection_data.pt3D.col(j) = vec3D[j];
		}

		// if list of match has adequate number to estimate camera
		if (vec2D.size() > MINIMUM_VIEW_NUM_TO_ESTIMATAE_CAMERA_POSE) {

			// get intrinsic
			const Intrinsics::const_iterator iterIntrinsic_I = sfm_data.GetIntrinsics().find(iterV->second->id_intrinsic);
			Pinhole_Intrinsic *cam_I = dynamic_cast<Pinhole_Intrinsic*>(iterIntrinsic_I->second.get());

			// copy to mat
			openMVG::Mat pt2D(2, vec2D.size());
			openMVG::Mat pt3D(3, vec3D.size());

			for (size_t ptInd = 0; ptInd < vec2D.size(); ptInd++) {
				pt2D.col(ptInd) = cam_I->get_ud_pixel(vec2D[ptInd]);
				pt3D.col(ptInd) = vec3D[ptInd];
			}

			// estimate R and T
			geometry::Pose3 pose;
			const bool bResection = sfm::SfM_Localizer::Localize(
					make_pair(iterV->second->ui_width, iterV->second->ui_height),
					cam_I, resection_data, pose);
			Mat3 K_, R_;
			Vec3 t_, t_out;
			KRt_From_P(resection_data.projection_matrix, &K_, &R_, &t_);
			t_out = -R_.transpose() * t_;
			sfm_data.poses[iterV->second->id_pose] = Pose3(R_, t_out);
		} else {
			warning = true;
		}
	}

	if (warning) {
		cout << "Warning: there is/are frames with too few matches." << endl;
	}

	string sSfM_data_bd2 = stlplus::create_filespec(
			stlplus::folder_part(sSfM_data), "sfm_data_b4bd", "json");
	Save(sfm_data, sSfM_data_bd2,
			ESfM_Data(VIEWS | INTRINSICS | EXTRINSICS | STRUCTURE));

	// split bundle adjustment command order
	stringstream ss(bd_command);
	string item;
	vector<string> commandBD;
	while (getline(ss, item, ',')) {
		commandBD.push_back(item);
	}

	if (commandBD.size() > 0) {
		// 2. perform bundle adjustment
		// Set bundle adjustment option, this option assumes you do not enable sparse matrix solver
		// http://ceres-solver.org/faqs.html#solving
#if (OPENMVG_VERSION_MAJOR<1)
		Bundle_Adjustment_Ceres::BA_options options(true, true);
		options._linear_solver_type = ceres::ITERATIVE_SCHUR;
		options._preconditioner_type = ceres::SCHUR_JACOBI;
#else
		Bundle_Adjustment_Ceres::BA_Ceres_options options(true, true);
		options.linear_solver_type_ = ceres::ITERATIVE_SCHUR;
		options.preconditioner_type_ = ceres::SCHUR_JACOBI;
#endif
		Bundle_Adjustment_Ceres bundle_adjustment_obj(options);

		// TODO : repeat bundle adjustment if lots of unstable points are found
		// 2.1. solve for selected parameters (rotation, translation, intrinsic, structure)
		for (vector<string>::const_iterator iterCommand = commandBD.begin();
				iterCommand != commandBD.end(); ++iterCommand) {
			bool bdRot = (*iterCommand).find("r") != string::npos;
			bool bdTrn = (*iterCommand).find("t") != string::npos;
			bool bdInt = (*iterCommand).find("i") != string::npos;
			bool bdStr = (*iterCommand).find("s") != string::npos;
			bool bdCln = (*iterCommand).find("c") != string::npos;

			cout << endl;
			cout << "Bundle adjustment over ";
			if (bdRot) {
				cout << "rotations, ";
			}
			if (bdTrn) {
				cout << "translations, ";
			}
			if (bdInt) {
				cout << "intrinsic params, ";
			}
			if (bdStr) {
				cout << "structure, ";
			}
			cout << endl;

#if (OPENMVG_VERSION_MAJOR<1)
			bundle_adjustment_obj.Adjust(sfm_data, bdRot, bdTrn, bdInt, bdStr); // pose rotation, pose translation, intrinsic, structure
#else
			openMVG::sfm::Optimize_Options optimizeOptions(cameras::Intrinsic_Parameter_Type::NONE,
					Extrinsic_Parameter_Type::NONE, Structure_Parameter_Type::NONE,
					Control_Point_Parameter(0.0, false));
			if (bdRot && bdTrn) {
				optimizeOptions.extrinsics_opt = Extrinsic_Parameter_Type::ADJUST_ALL;
			} else if (bdRot) {
					optimizeOptions.extrinsics_opt = Extrinsic_Parameter_Type::ADJUST_ROTATION;
			} else if (bdTrn) {
					optimizeOptions.extrinsics_opt = Extrinsic_Parameter_Type::ADJUST_TRANSLATION;
			}
			if (bdInt) {
				optimizeOptions.intrinsics_opt = cameras::Intrinsic_Parameter_Type::ADJUST_ALL;
			}
			if (bdStr) {
				optimizeOptions.structure_opt = Structure_Parameter_Type::ADJUST_ALL;
			}
			bundle_adjustment_obj.Adjust(sfm_data, optimizeOptions);
#endif
			
			if (bdCln) {
				// cleanup structures
				const size_t pointBeforeCleanup = sfm_data.structure.size();
				RemoveOutliers_PixelResidualError(sfm_data, STRUCTURE_CLEANUP_RESIDUAL_ERROR);
				const size_t pointAfterRessidualError = sfm_data.structure.size();
				RemoveOutliers_AngleError(sfm_data, STRUCTURE_CLEANUP_ANGLE_ERROR);
				const size_t pointAfterAngleError = sfm_data.structure.size();
				if (rm_unstable) {
					eraseUnstablePosesAndObservations(sfm_data);
				}
				const size_t pointAfterCleanup = sfm_data.structure.size();
				cout << "Number of points before cleanup : " << pointBeforeCleanup << endl;
				cout << "Number of points residual error : " << pointAfterRessidualError << endl;
				cout << "Number of points angle error : " << pointAfterAngleError << endl;
				cout << "Number of points after cleanup : " << pointAfterCleanup << endl;
			}
		}
	} else {
		// cleanup structures
		const size_t pointBeforeCleanup = sfm_data.structure.size();
		RemoveOutliers_PixelResidualError(sfm_data, STRUCTURE_CLEANUP_RESIDUAL_ERROR);
		const size_t pointAfterRessidualError =	sfm_data.structure.size();
		RemoveOutliers_AngleError(sfm_data, STRUCTURE_CLEANUP_ANGLE_ERROR);
		const size_t pointAfterAngleError = sfm_data.structure.size();
		if (rm_unstable) {
			eraseUnstablePosesAndObservations(sfm_data);
		}
		const size_t pointAfterCleanup = sfm_data.structure.size();
		cout << "Number of points before cleanup : " << pointBeforeCleanup << endl;
		cout << "Number of points residual error : " << pointAfterRessidualError << endl;
		cout << "Number of points angle error : " << pointAfterAngleError << endl;
		cout << "Number of points after cleanup : " << pointAfterCleanup << endl;
	}

	// 3. save model
	string sSfM_data_bd = sSfM_data_out;
	Save(sfm_data, sSfM_data_bd,
			ESfM_Data(VIEWS | INTRINSICS | EXTRINSICS | STRUCTURE));
}
