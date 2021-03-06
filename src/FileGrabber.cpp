/*
    Copyright (c) 2017 Mobile Robots Laboratory at Poznan University of Technology:
    -Jan Wietrzykowski name.surname [at] put.poznan.pl

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.
*/
#include <iterator>
#include <algorithm>

#include <boost/archive/text_iarchive.hpp>
#include <boost/serialization/string.hpp>

#include <pcl/io/ply_io.h>
#include <g2o/types/slam3d/se3quat.h>
#include <FileGrabber.hpp>


#include "FileGrabber.hpp"
#include "Exceptions.hpp"
#include "Misc.hpp"
#include "Map.hpp"
#include "Serialization.hpp"

using namespace std;
using namespace cv;

FileGrabber::FileGrabber(const FileNode &settings) :
	nextFrameIdx(-1),
	depthScale((float)settings["depthScale"]),
	imageFrame((int)settings["imageFrame"])
{
	boost::filesystem::path datasetDirPath(settings["datasetDirPath"]);
	{
		cout << "reading rgb images paths" << endl;
		boost::filesystem::path rgbDirPath = datasetDirPath / boost::filesystem::path("rgb");
		if(!boost::filesystem::is_directory(rgbDirPath)){
			throw PLANE_EXCEPTION(string("No RGB directory ") + boost::filesystem::absolute(rgbDirPath).c_str());
		}
		copy(boost::filesystem::directory_iterator(rgbDirPath),
				boost::filesystem::directory_iterator(),
				back_inserter(rgbPaths));
		sort(rgbPaths.begin(), rgbPaths.end());
//		for(int i = 0; i < 10; ++i){
//			cout << boost::filesystem::absolute(rgbPaths[i]) << endl;
//		}
	}
	{
		cout << "reading depth images paths" << endl;
		boost::filesystem::path depthDirPath = datasetDirPath / boost::filesystem::path("depth");
		if(!boost::filesystem::is_directory(depthDirPath)){
			throw PLANE_EXCEPTION(string("No depth directory ") + boost::filesystem::absolute(depthDirPath).c_str());
		}
		copy(boost::filesystem::directory_iterator(depthDirPath),
				boost::filesystem::directory_iterator(),
				back_inserter(depthPaths));
		sort(depthPaths.begin(), depthPaths.end());
//		for(int i = 0; i < 10; ++i){
//			cout << depthPaths[i] << endl;
//		}
	}
	{
		cout << "reading clouds paths" << endl;
		cloudAvailable.resize(rgbPaths.size(), false);
		cloudPaths.resize(rgbPaths.size());
		boost::filesystem::path cloudsDirPath = datasetDirPath / boost::filesystem::path("clouds");
		for(int f = 0; f < rgbPaths.size(); ++f){
			char cloudPlyFilename[100];
			sprintf(cloudPlyFilename, "cloud%04d.ply", f);
			boost::filesystem::path cloudFilePath = cloudsDirPath / boost::filesystem::path(cloudPlyFilename);
			if(boost::filesystem::is_regular_file(cloudFilePath)){
				cloudPaths[f] = cloudFilePath;
				cloudAvailable[f] = true;
			}
		}
	}
	{
		cout << "reading acc paths" << endl;
		accAvailable.resize(rgbPaths.size(), false);
		accPaths.resize(rgbPaths.size());
		boost::filesystem::path accDirPath = datasetDirPath / boost::filesystem::path("acc");
		for(int f = 0; f < rgbPaths.size(); ++f){
			char accFilename[100];
			sprintf(accFilename, "acc%05d", f);
			boost::filesystem::path accFilePath = accDirPath / boost::filesystem::path(accFilename);
			if(boost::filesystem::is_regular_file(accFilePath)){
				accPaths[f] = accFilePath;
				accAvailable[f] = true;
			}
		}
	}
	if((int)settings["readObjLabeling"]){
		cout << "reading instances images paths" << endl;
		boost::filesystem::path instancesDirPath = datasetDirPath / boost::filesystem::path("instances");
		if(!boost::filesystem::is_directory(instancesDirPath)){
			throw PLANE_EXCEPTION(string("No instances directory ") + boost::filesystem::absolute(instancesDirPath).c_str());
		}
		copy(boost::filesystem::directory_iterator(instancesDirPath),
				boost::filesystem::directory_iterator(),
				back_inserter(instancesPaths));
		sort(instancesPaths.begin(), instancesPaths.end());
//		for(int i = 0; i < 10; ++i){
//			cout << instancesPaths[i] << endl;
//		}
	}
	if((int)settings["readObjLabeling"]){
		cout << "reading labels images paths" << endl;
		boost::filesystem::path labelsDirPath = datasetDirPath / boost::filesystem::path("labels");
		if(!boost::filesystem::is_directory(labelsDirPath)){
			throw PLANE_EXCEPTION(string("No labels directory ") + boost::filesystem::absolute(labelsDirPath).c_str());
		}
		copy(boost::filesystem::directory_iterator(labelsDirPath),
				boost::filesystem::directory_iterator(),
				back_inserter(labelsPaths));
		sort(labelsPaths.begin(), labelsPaths.end());
//		for(int i = 0; i < 10; ++i){
//			cout << labelsPaths[i] << endl;
//		}
	}
	if((int)settings["readObjLabeling"]){
		cout << "reading classses" << endl;
		boost::filesystem::path classesFilePath = datasetDirPath / boost::filesystem::path("classes.txt");
		if(!boost::filesystem::exists(classesFilePath)){
			throw PLANE_EXCEPTION(string("No classes file ") + boost::filesystem::absolute(classesFilePath).c_str());
		}
		ifstream classesFile(classesFilePath.c_str());
		while(!classesFile.fail() && !classesFile.eof()){
			int id;
			string name;
			classesFile >> id;
			getline(classesFile, name);
			if(!classesFile.fail()){
//				cout << id << " " << name << endl;
				classIdToName[id] = name;
				classNameToId[name] = id;
			}
		}
	}
	if((int)settings["readAccel"]){
		cout << "reading accelerometer data" << endl;
		boost::filesystem::path accelFilePath = datasetDirPath / boost::filesystem::path("accelData.txt");
		if(!boost::filesystem::exists(accelFilePath)){
			throw PLANE_EXCEPTION(string("No accel file ") + boost::filesystem::absolute(accelFilePath).c_str());
		}
		ifstream accelFile(accelFilePath.c_str());
		while(!accelFile.fail() && !accelFile.eof()){
			vector<double> curData;
			for(int i = 0; i < 4; ++i){
				double tmp;
				accelFile >> tmp;
				curData.push_back(tmp);
			}
			if(!accelFile.fail()){
//				cout << curData << endl;
				accelDataAll.push_back(curData);
			}
		}
	}
	if((int)settings["readPose"]){
		cout << "reading pose data" << endl;
		boost::filesystem::path poseFilePath = datasetDirPath / boost::filesystem::path("groundtruth.txt");
		if(!boost::filesystem::exists(poseFilePath)){
			throw PLANE_EXCEPTION(string("No pose file ") + boost::filesystem::absolute(poseFilePath).c_str());
		}
		ifstream poseFile(poseFilePath.c_str());
		while(!poseFile.fail() && !poseFile.eof()){
			Vector7d curPose;
			// discard index
			double tmp;
			poseFile >> tmp;
			for(int i = 0; i < 7; ++i){
				poseFile >> curPose[i];
			}
			if(!poseFile.fail()){
//				cout << curPose << endl;
				groundtruthAll.push_back(curPose);
			}
		}
	}
    
    vector<double> voOffsetVals;
    settings["voOffset"] >> voOffsetVals;
    for(int v = 0; v < voOffsetVals.size(); ++v){
        voOffset(v) = voOffsetVals[v];
    }
    
	if((int)settings["readVO"]){
		cout << "reading VO data" << endl;
		boost::filesystem::path voFilePath = datasetDirPath / boost::filesystem::path("vo.txt");
		if(!boost::filesystem::exists(voFilePath)){
			throw PLANE_EXCEPTION(string("No VO file ") + boost::filesystem::absolute(voFilePath).c_str());
		}
		ifstream voFile(voFilePath.c_str());
		while(!voFile.fail() && !voFile.eof()){
			Vector7d curVo;
			// timestamp
			double tstamp;
			voFile >> tstamp;
			for(int i = 0; i < 7; ++i){
				voFile >> curVo[i];
			}
			if(!voFile.fail()){
//				cout << curVo << endl;
                g2o::SE3Quat curVoOffset = g2o::SE3Quat(voOffset) * g2o::SE3Quat(curVo);
                
				voAll.push_back(curVoOffset.toVector());
                if(tstamp >= 0){
                    voCorrAll.push_back(true);
                }
                else{
                    voCorrAll.push_back(false);
                }
			}
		}
	}
 
 
	if(!rgbPaths.empty()){
		cout << "setting nextFrameIdx" << endl;
		nextFrameIdx = 0;
	}
}

int FileGrabber::getFrame(cv::Mat& rgb,
                          cv::Mat& depth,
                          std::vector<FrameObjInstance>& objInstances,
                          std::vector<double>& accelData,
                          Vector7d& pose,
                          Vector7d &vo,
                          bool &voCorr)
{
	int curFrameIdx = nextFrameIdx;
	if(curFrameIdx >= 0){
		rgb = imread(rgbPaths[curFrameIdx].c_str());
		if(rgb.empty()){
			throw PLANE_EXCEPTION(string("Cannot read rgb file ") + boost::filesystem::absolute(rgbPaths[curFrameIdx]).c_str());
		}
		rgb = rgb(Range(imageFrame, rgb.rows - imageFrame), Range(imageFrame, rgb.cols - imageFrame));
		cvtColor(rgb, rgb, COLOR_BGR2RGB);

		depth = imread(depthPaths[curFrameIdx].c_str(), IMREAD_ANYDEPTH);
		if(depth.empty()){
			throw PLANE_EXCEPTION(string("Cannot read depth file ") + boost::filesystem::absolute(depthPaths[curFrameIdx]).c_str());
		}
		depth = depth(Range(imageFrame, depth.rows - imageFrame), Range(imageFrame, depth.cols - imageFrame));
		depth.convertTo(depth, CV_32F, 1.0/depthScale);



		if(!instancesPaths.empty()){
			Mat instances = imread(instancesPaths[curFrameIdx].c_str(), IMREAD_ANYDEPTH);
			if(instances.empty()){
				throw PLANE_EXCEPTION(string("Cannot read instances file ") + boost::filesystem::absolute(instancesPaths[nextFrameIdx]).c_str());
			}
		}
		if(!labelsPaths.empty()){
			Mat labels = imread(labelsPaths[curFrameIdx].c_str(), IMREAD_ANYDEPTH);
			if(labels.empty()){
				throw PLANE_EXCEPTION(string("Cannot read labels file ") + boost::filesystem::absolute(labelsPaths[nextFrameIdx]).c_str());
			}
		}
		//TODO Fill objInstances vector

		if(!accelDataAll.empty()){
			accelData = accelDataAll[curFrameIdx];
		}
		if(!groundtruthAll.empty()){
			pose = groundtruthAll[curFrameIdx];
		}
        if(!voAll.empty()){
            vo = voAll[curFrameIdx];
        }
        if(!voCorrAll.empty()){
            voCorr = voCorrAll[curFrameIdx];
        }
        
		++nextFrameIdx;
		if(nextFrameIdx >= rgbPaths.size()){
			//end of sequence
			nextFrameIdx = -1;
		}
	}
	return curFrameIdx;
}

int FileGrabber::getFrame(cv::Mat& rgb,
                          cv::Mat& depth,
                          std::vector<FrameObjInstance>& objInstances,
                          std::vector<double>& accelData,
                          Vector7d& pose,
                          Vector7d &vo,
                          bool &voCorr,
					      pcl::PointCloud<pcl::PointXYZRGBNormal>::Ptr pointCloud)
{
	if(nextFrameIdx >= 0){
		pointCloud->clear();
		if(cloudAvailable[nextFrameIdx]){
			pcl::io::loadPLYFile(string(cloudPaths[nextFrameIdx].c_str()), *pointCloud);
		}
	}
	
	return getFrame(rgb,
					depth,
					objInstances,
					accelData,
					pose,
                    vo,
                    voCorr);
}

int FileGrabber::getFrame(cv::Mat &rgb,
						  cv::Mat &depth,
						  vector<FileGrabber::FrameObjInstance> &objInstances,
						  std::vector<double> &accelData,
						  Vector7d &pose,
						  Vector7d &vo,
						  bool &voCorr,
						  Map &accMap)
{
	if(nextFrameIdx >= 0){
        accMap = Map();
        
		if(accAvailable[nextFrameIdx]) {
            cout << "loading map from file: " << accPaths[nextFrameIdx].c_str() << endl;
            std::ifstream ifs(accPaths[nextFrameIdx].c_str());
            boost::archive::text_iarchive ia(ifs);
            ia >> accMap;
        }
	}
	
	return getFrame(rgb,
					depth,
					objInstances,
					accelData,
					pose,
					vo,
					voCorr);
}

boost::filesystem::path FileGrabber::getRgbFilePath(int idx) {
    return rgbPaths[idx];
}

int FileGrabber::getNumFrames() {
    return rgbPaths.size();
}
