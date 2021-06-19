#ifndef HPE_PROBLEM_H
#define HPE_PROBLEM_H

#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing/render_face_detections.h>
#include <dlib/image_processing.h>
#include <dlib/gui_widgets.h>
#include <dlib/image_io.h>
#include <dlib/opencv.h>

#include <opencv2/highgui/highgui.hpp>

#include <iostream>
#include <fstream>
#include <assert.h>
#include <memory>
#include <chrono>

#include "ceres/ceres.h"

#include "tiny_progress.hpp"

#include "glog/logging.h"

#include "string_utils.h"
#include "io_utils.h"
#include "functor/functor.h"
#include "texture.h"
#include "data_manager.h"


using Eigen::Matrix;
using Eigen::Vector3d;
using Eigen::VectorXd;
using Summary = ceres::Solver::Summary;


class MHPEProblem
{


public:


	/* Initialization 
	 * Usage:
	 *     HeadPoseEstimationProblem hpe_problem;
	 *     HeadPoseEstimationProblem hpe_problem(filename);
	 * Parameters:
	 * 	   @filename: Filename for Basel Face Model loader.
	 *******************************************************************************
	 * Init a head pose estimation problem with input filename as bfm input filename.
	 */

	MHPEProblem(
		const std::string& sProjectPath,
        const std::string& sBfmH5Path,
        const std::string& sLandmarkIdxPath,
		const std::string& sDlibDetPath
	);
	Status init(
		const std::string& sProjectPath,
        const std::string& sBfmH5Path,
        const std::string& sLandmarkIdxPath,
		const std::string& sDlibDetPath
	);

	inline std::shared_ptr<BfmManager>& getBfmManager() { return m_pBfmManager; }

	void solve(SolveExtParamsMode mode, double dShapeWeight, double dExprWeight);


private:


	bool is_close_enough(double *ext_params, double rotation_eps = 0, double translation_eps = 0);

	std::vector<double> estInit3dPts(const std::vector<DetPair>& vDetPairs);
	double estInitSc(const std::vector<double>& vPts);
	void estInitExtParams(std::vector<double>& vPts);
	Summary estExtParams(const DetPairVector&  aObjDets, double scMean);
	Summary estShapeCoef(const DetPairVector&  aObjDets, double dShapeWeight);
	Summary estExprCoef(const DetPairVector&  aObjDets, double dExprWeight);
	void initWin(
		dlib::image_window& window, 
		const std::string& sTitle, 
		const Arr2d& img,
		const ObjDet& objDet,
		bool bIsValid
	);
	void rmOutliers();
	void showRes(std::vector<dlib::image_window>& vWins);

	shared_ptr<BfmManager> m_pBfmManager;
	shared_ptr<DataManager> m_pDataManager;
};


#endif // HPE_PROBLEM_H