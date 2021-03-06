#ifndef DATA_MANAGER
#define DATA_MANAGER

#include "db_params.h"
#include "file_utils.h"
#include "texture.h"
#include "utils/2d_render_utils.h"

#include "tinyxml2.h"

#include "tiny_progress.hpp"

#include <Eigen/Dense>

#include <boost/algorithm/string.hpp>
#include <boost/filesystem.hpp>

#include <dlib/image_io.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing/render_face_detections.h>
#include <dlib/image_processing.h>

#include "glog/logging.h"

#include <fstream>
#include <string>
#include <iostream>
#include <vector>
#include <iomanip>

using namespace tinyxml2;
using namespace boost::filesystem;

using ObjDet = dlib::full_object_detection;
using DetPair = std::pair<ObjDet, int>;
using Arr2d = dlib::array2d<dlib::rgb_pixel>;

/*
 * Directory:
 * 	-root
 * 		- image: Folder containing images;
 * 		- cam.xml: Camer<a information, exmaple format as follows:
 * 	
 * <?xml version="1.0" encoding="UTF-8"?>
 * <document version="1.4.0">
 *     <chunk>
 *         <transform>
 *             <rotation locked="0">-4 -1 -9 9 5 -5 6 -9 1</rotation>
 *             <translation locked="0">-6 -1 -4</translation>
 *         </transform>
 *         <sensors next_id="1">
 *             <calibration type="frame" class="adjusted">
 *                 <resolution width="6000" height="4000"/>
 *                 <f>13992</f>
 *                 <cx>-27</cx>
 *                 <cy>39</cy>
 *                 <k1>0</k1>
 *                 <k2>1</k2>
 *                 <k3>-2</k3>
 *                 <p1>-0</p1>
 *                 <p2>0</p2>
 *             </calibration>	  
 * 		       <sensor id = "0">
 * 	       </sensors>
 *         <cameras next_id="2">
 *             <camera id = "0">
 *                 <transform>1 0 0 0 0 -1 -1 0 0 1 -1 0 0 0 0 1</transform>          
 *             </camera>
 * 			   <camera id = "1">
 *                 <transform>9 3 2 -1 -4 -9 1 6 2 -2 -9 1 0 0 0 1</transform>
 *             </camera>
 *         </cameras>
 *     </chunk>
 * </document>
 */



class DataManager
{
public:
	DataManager(const std::string &rootDir, const std::string& sDlibDetPath) :
		m_pathRootDir(path(rootDir)),
		m_pathPhotoDir(m_pathRootDir / "image"),
		m_pathXml(m_pathRootDir / "cam.xml")
	{		
		LOG(INFO) << "Init data manager";

		if(this->loadCamInfo() == Status_Error)
		{
			m_nViews = 0;
			return;
		}

		if(this->loadTextures() == Status_Error)
		{
			m_nViews = 0;
			return;
		}

		this->preprocess(sDlibDetPath);

		LOG(INFO) << "Load successfully\n";
		LOG(INFO) << "********************************************";
		LOG(INFO) << "************* Load Data ********************";
		LOG(INFO) << "********************************************";
		LOG(INFO) << "Camera intrinsic parameters(f cx cy):\t" << m_f << "\t" << m_cx << "\t" << m_cy;
		LOG(INFO) << "Number of view:\t" << m_nViews;
		LOG(INFO) << "\n";


	}

	double getF() const { return m_f; }
	double getCx() const { return m_cx; }
	double getCy() const { return m_cy; }
	double getWidth() const { return m_width; }
	double getHeight() const { return m_height; }
	unsigned int getNViews() const { return m_nViews; }
	double getScale() const { return m_dZoomSc; }

	const auto& getCameraMatrices() const {return m_aCameraMatrices; }
	const auto& getProjMatrices() const { return m_aProjMatrices; }
	const auto& getInvTransMatrices() const { return m_aInvTransMatrices; }
	const auto& getCamPositions() const { return m_aCamPositions; }
	const auto& getTextures() const { return m_aTextures; }

	auto& getRotTypes() { return m_aRotTypes; }
	const auto& getArr2dImgs() const { return m_a2dImgs; }
	const auto& getArr2dTransImgs() const { return m_a2dTransImgs; }
	const auto& getDets() const { return m_aDets; }
	const auto& getTransDets() const { return m_aTransDets; }

private:
	path m_pathRootDir;
	path m_pathPhotoDir;
	path m_pathXml;

	// camera infomation
	double m_f;
	double m_cx;
	double m_cy;
	double m_width;
	double m_height;
	unsigned int m_nViews;

	Eigen::Matrix4f m_matModel;
	std::vector<Eigen::Matrix<float, 4, 4>> m_aCameraMatrices;
	std::vector<Eigen::Matrix<float, 3, 4>> m_aProjMatrices;
	std::vector<Eigen::Matrix<float, 3, 4>> m_aTransMatrices;
	std::vector<Eigen::Matrix4f> m_aInvTransMatrices;
	std::vector<Eigen::Vector3f> m_aCamPositions;
	std::vector<Texture> m_aTextures;

	double m_dZoomSc = 0.1;
	std::vector<RotateType> m_aRotTypes;
	std::vector<Arr2d> m_a2dImgs;
	std::vector<Arr2d> m_a2dTransImgs;
	std::vector<ObjDet> m_aDets;
	std::vector<ObjDet> m_aTransDets;

	dlib::frontal_face_detector m_faceDetector;
	dlib::shape_predictor m_shapePredictor;

	Status loadCamInfo()
	{
		LOG(INFO) << "Load camera information";
		m_matModel = Eigen::Matrix4f::Identity();

		m_aProjMatrices.clear();
		m_aTransMatrices.clear();
		m_aInvTransMatrices.clear();
		m_aCamPositions.clear();

		std::vector<double> sensor_f;
		std::vector<double> sensor_cx;
		std::vector<double> sensor_cy;
		std::vector<double> sensor_w;
		std::vector<double> sensor_h;

		std::vector<int> sensor_idxs;

		int sensorNum;

		XMLDocument doc;
		if(doc.LoadFile(m_pathXml.string().c_str()) != XML_SUCCESS)
		{
			LOG(ERROR) << "XML load failed.";
			return Status_Error;
		}

		XMLElement *root = doc.RootElement();
		XMLElement *chunk = root->FirstChildElement("chunk"); 
		if (chunk != NULL)
		{
			// model transform
			XMLElement *xml_transform = chunk->FirstChildElement("transform");
			if (xml_transform != NULL)
			{
				XMLElement* xmlR = xml_transform->FirstChildElement("rotation");
				if(xmlR == nullptr)
				{
					LOG(WARNING) << "No transform information found.";
					// const char* sScale = xml_transform->FirstChildElement("scale")->GetText();
					// m_matModel = Eigen::Matrix4f::Identity() / atof(sScale);
					// m_matModel = Eigen::Matrix4f::Identity();
					m_matModel.block(0, 0, 3, 3) = Eigen::Matrix3f::Identity();
				}
				else
				{
					const char* rotationText = xml_transform->FirstChildElement("rotation")->GetText();
					const char* translationText = xml_transform->FirstChildElement("translation")->GetText();					
					char *rotation = const_cast<char *>(rotationText);
					char *splits;
					const char *d = " ";
					splits = strtok(rotation, d);
					int row = 0, col = 0;
					while (splits)
					{
						m_matModel(row, col) = atof(splits);
						col++;
						if (col == 3)
						{
							row++;
							col = 0;
						}
						splits = strtok(NULL, d);
					}

					char *translation = const_cast<char *>(translationText);
					splits = strtok(translation, d);
					for (int i = 0; i < 3; i++)
					{
						m_matModel(i, 3) = atof(splits);
						splits = strtok(NULL, d);
					}
				}
			}


			// sensors
			XMLElement *xml_sensors = chunk->FirstChildElement("sensors");
			if (xml_sensors != NULL)
			{
				sensorNum = atoi(xml_sensors->Attribute("next_id"));

				// intial sensor size
				sensor_f.resize(sensorNum);
				sensor_cx.resize(sensorNum);
				sensor_cy.resize(sensorNum);
				sensor_w.resize(sensorNum);
				sensor_h.resize(sensorNum);

				XMLElement *xml_sensor = xml_sensors->FirstChildElement("sensor");
				for (int i = 0; i < sensorNum; i++)
					while (NULL != xml_sensor)
					{
						int sensorId = atoi(xml_sensor->Attribute("id"));
						//cout<<sensorId<<endl;

						XMLElement *xml_calibration = xml_sensor->FirstChildElement("calibration");
						XMLElement *xml_resolution = xml_calibration->FirstChildElement("resolution");

						XMLElement *xml_f = xml_calibration->FirstChildElement("f");
						XMLElement *xml_cx = xml_calibration->FirstChildElement("cx");
						XMLElement *xml_cy = xml_calibration->FirstChildElement("cy");

						sensor_w[sensorId] = atoi(xml_resolution->Attribute("width"));
						sensor_h[sensorId] = atoi(xml_resolution->Attribute("height"));
						sensor_f[sensorId] = atof(xml_f->GetText());
						sensor_cx[sensorId] = atof(xml_cx->GetText());
						sensor_cy[sensorId] = atof(xml_cy->GetText());

						//distoration parameters
						XMLElement *xml_k1 = xml_calibration->FirstChildElement("k1");
						XMLElement *xml_k2 = xml_calibration->FirstChildElement("k2");
						XMLElement *xml_p1 = xml_calibration->FirstChildElement("p1");
						XMLElement *xml_p2 = xml_calibration->FirstChildElement("p2");
						XMLElement *xml_k3 = xml_calibration->FirstChildElement("k3");

						xml_sensor = xml_sensor->NextSiblingElement("sensor");
					}
			}

			//cameras
			XMLElement *cameras = chunk->FirstChildElement("cameras");
			if (cameras != NULL)
			{
				m_nViews = atoi(cameras->Attribute("next_id"));

				sensor_idxs.resize(m_nViews);

				XMLElement *xml_camera = cameras->FirstChildElement("camera");
				while (NULL != xml_camera)
				{

					Eigen::Matrix<float, 4, 4> T_camera;
					Eigen::Matrix<float, 3, 4> P;
					int camera_idx = atoi(xml_camera->Attribute("id"));
					int sensor_idx = atoi(xml_camera->Attribute("sensor_id"));
					//cout << "camera " << camera_idx << " using sensor " << sensor_idx << endl;


					const char* transformtmp = xml_camera->FirstChildElement("transform")->GetText();
					char *transform = const_cast<char *>(transformtmp);
					char *splits;
					const char *d = " ";
					splits = strtok(transform, d);
					int row = 0, col = 0;
					while (splits) {
						T_camera(row, col) = atof(splits);
						col++;
						if (col == 4) {
							row++;
							col = 0;
						}
						splits = strtok(NULL, d);

					}

					double f = sensor_f[sensor_idx];
					double cx = sensor_cx[sensor_idx];
					double cy = sensor_cy[sensor_idx];
					double w = sensor_w[sensor_idx];
					double h = sensor_h[sensor_idx];
					Eigen::Matrix<float, 3, 4> K;
					K << f, 0, w / 2.0 + cx, 0,
						0, f, h / 2.0 + cy, 0,
						0, 0, 1, 0;

					Eigen::Matrix<float, 4, 4> T = T_camera.inverse() * m_matModel.inverse();
					P = K * T;

					m_aCameraMatrices.push_back(T_camera.inverse());
					// m_aCameraMatrices.push_back(T_camera);

					m_aProjMatrices.push_back(P);
					// std::cout << T << "\n" << std::endl;
					m_aTransMatrices.push_back(T.block(0, 0, 3, 4));
					m_aInvTransMatrices.push_back(T.inverse());

					Eigen::Matrix3f R = T.block(0, 0, 3, 3);
					Eigen::Vector3f t(T(0, 3), T(1, 3), T(2, 3));
					Eigen::Vector3f tmp = -R.transpose()*t;
					m_aCamPositions.push_back(Eigen::Vector3f(tmp[0], tmp[1], tmp[2]));

					sensor_idxs[camera_idx] = sensor_idx;

					xml_camera = xml_camera->NextSiblingElement("camera");
				}
			}
		}

		m_f = sensor_f[0];
		m_cx = sensor_cx[0];
		m_cy = sensor_cy[0];
		m_width = sensor_w[0];
		m_height = sensor_h[0];

		return Status_Ok;
	}
	

	Status loadTextures()
	{
		LOG(INFO) << "Load textures";
		bool bIsSuccess;
		m_aTextures.resize(m_nViews);

		// #pragma omp parallel for schedule(dynamic)
		for(std::size_t i = 0; i < m_nViews; ++i)
		{
			path pathTexture = m_pathPhotoDir / file_utils::Id2Str(i);
			m_aTextures[i] = Texture::LoadTexture(pathTexture.string());
		}

		return Status_Ok;
	}


	void preprocess(const std::string& sDlibDetPath)
	{
		m_faceDetector = dlib::get_frontal_face_detector();
		dlib::deserialize(sDlibDetPath) >> m_shapePredictor;

		m_a2dImgs.resize(m_nViews);
		m_a2dTransImgs.resize(m_nViews);
		m_aDets.resize(m_nViews);
		m_aTransDets.resize(m_nViews);
		m_aRotTypes = {	// Outlier: 2(DETECT TOO BAD) 3(FIT TOO BAD) 20(FIT NOT GOOD)
			RotateType_Invalid, RotateType_Invalid, RotateType_Invalid, RotateType_Invalid, RotateType_CCW, 
			RotateType_CW, RotateType_CW, RotateType_CW, RotateType_CW, RotateType_Invalid,
			RotateType_CCW, RotateType_Invalid, RotateType_CCW, RotateType_Invalid, RotateType_CCW,
			RotateType_CCW, RotateType_CCW, RotateType_CCW, RotateType_CCW, RotateType_Invalid,
			RotateType_Invalid, RotateType_CW, RotateType_Invalid, RotateType_Invalid
		};

		#pragma omp parallel for
		for(std::size_t iView = 0; iView < m_nViews; ++iView)
		{
			const Texture& tex = m_aTextures[iView];
			const std::string &sTexPath = tex.getPath();

			if(m_aRotTypes[iView] == RotateType_Invalid)
				continue;

			std::vector<dlib::rectangle> aRecs;

			dlib::load_image(m_a2dImgs[iView], sTexPath);
			dlib::resize_image(m_dZoomSc, m_a2dImgs[iView]);

			dlib::rotate_image(m_a2dImgs[iView], m_a2dTransImgs[iView], M_PI * 0.5);

			#pragma omp critical(detector)
			{
				aRecs = m_faceDetector(m_a2dTransImgs[iView]);
			}

			if(aRecs.size() == 0)
			{
				dlib::rotate_image(m_a2dImgs[iView], m_a2dTransImgs[iView], M_PI * 1.5);
				aRecs = m_faceDetector(m_a2dTransImgs[iView]);
				if(aRecs.size() == 0)
				{
					m_aRotTypes[iView] = RotateType_Invalid;
					continue;
				}
				else
					m_aRotTypes[iView] = RotateType_CW;
			}
			else
				m_aRotTypes[iView] = RotateType_CCW;

		
			ObjDet objDet = m_shapePredictor(m_a2dTransImgs[iView], aRecs[0]);
			m_aTransDets[iView] = objDet;

			auto height = tex.getHeight() * m_dZoomSc;
			auto width = tex.getWidth() * m_dZoomSc;
			#pragma omp parallel for
			for(auto j = 0; j < N_DLIB_LANDMARKS; j++)
				utils::RotateFromFront(objDet.part(j).x(), objDet.part(j).y(), height, width, m_aRotTypes[iView]);

			m_aDets[iView] = objDet;
		}

		LOG(INFO) << "Detection result:";
		LOG(INFO) << "----------------------------";
		for(auto iView = 0; iView < m_nViews; ++iView)
		{
			if(m_aRotTypes[iView] == RotateType_CCW)
				LOG(INFO) << "| " << iView << "\t| CCW              |";
			else if(m_aRotTypes[iView] == RotateType_CW)
				LOG(INFO) << "| " << iView << "\t| CW               |";
			else
				LOG(INFO) << "| " << iView << "\t| No face detected |";
		}
		LOG(INFO) << "----------------------------";

	}


};


#endif // DATA_MANAGER_H