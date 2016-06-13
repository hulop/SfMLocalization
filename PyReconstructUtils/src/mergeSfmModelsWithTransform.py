################################################################################
# Copyright (c) 2015 IBM Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
################################################################################

# -*- coding: utf-8 -*-

################################################################################
#
# Combine multiple sfm_data.json files to one sfm_data.json file.
#
# This projects needs following CSV file for input
#
# <project path>,<sfm data file path>,<global transformation matrix file path>
#
################################################################################

import argparse
import sys
import json
import os
import csv
import hulo_file.FileUtils as FileUtils
import numpy as np
from scipy.spatial import cKDTree
import hulo_param.ReconstructParam as ReconstructParam
import hulo_sfm.mergeSfM as mergeSfM
import hulo_ibeacon.IBeaconUtils as iBeaconUtils

#FILE_COPY_OPTION = "-s" # copy as link
FILE_COPY_OPTION = "-d" # copy original file even if source is link

def copyOriginalFiles(inputDir, outputDir):
    if not os.path.isdir(os.path.join(outputDir,"Input")):
        FileUtils.makedir(os.path.join(outputDir,"Input"))
    if not os.path.isdir(os.path.join(outputDir,"Input","inputImg")):
        FileUtils.makedir(os.path.join(outputDir,"Input","inputImg"))
    if not os.path.isdir(os.path.join(outputDir,"Input","csv")):
        FileUtils.makedir(os.path.join(outputDir,"Input","csv"))
    if not os.path.isdir(os.path.join(outputDir,"Output","matches")):
        FileUtils.makedir(os.path.join(outputDir,"Output","matches"))
    
    os.system("cp --remove-destination " + FILE_COPY_OPTION + " " + os.path.join(inputDir,"Input","*","inputImg","*") + " " + os.path.join(outputDir,"Input","inputImg"))
    os.system("cp --remove-destination " + FILE_COPY_OPTION + " " + os.path.join(inputDir,"Input","*","csv","*") + " " + os.path.join(outputDir,"Input","csv"))
    os.system("cp --remove-destination " + FILE_COPY_OPTION + " " + os.path.join(inputDir,"Output","*","matches","*.desc") + " " + os.path.join(outputDir,"Output","matches"))
    os.system("cp --remove-destination " + FILE_COPY_OPTION + " " + os.path.join(inputDir,"Output","*","matches","*.feat") + " " + os.path.join(outputDir,"Output","matches"))
    os.system("cp --remove-destination " + FILE_COPY_OPTION + " " + os.path.join(inputDir,"Output","final","Output","matches","image_describer.txt") + " " + os.path.join(outputDir,"Output","matches"))    

#
# find inliers when transforming model B to model A
# A and B are both 3 x n
# M is 3 x 4 matrix to convert model B to model A
# threshold to find inliers
#
def findInliersByKnownTransform(pointIdA, pointIdB, A, B, M, thres):
    # stack rows of 1
    B = np.vstack((B, np.ones((1, B.shape[1]))))
    # transform cordinate
    Btmp = np.dot(M, B)
    
    # find inliers by KD tree
    inliers = []
    kdtree = cKDTree(A.T)
    dist, indexes = kdtree.query(Btmp.T)
    
    for idx, val in enumerate(dist):                        
        if val < thres:
            inliers.append([pointIdB[idx], pointIdA[indexes[idx]]])
    
    return inliers

def main():
    description = 'This script is for merging multiple SfM output models to one SfM model.' + \
                'Please prepare multiple OpenMVG projects which have output SfM models, and matrix to convert to global coordinate.'
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument('input_csv', action='store', nargs=None, const=None, \
                        default=None, type=str, choices=None, metavar=None, \
                        help='Input CSV file which lists OpenMVG projects which will be merged.')
    parser.add_argument('output_dir', action='store', nargs=None, const=None, \
                        default=None, type=str, choices=None, metavar=None, \
                        help='Output directory path where merged model will be saved.')
    args = parser.parse_args()
    input_csv = args.input_csv
    output_dir = args.output_dir
        
    # load reconstruct parameters
    reconstructParam = ReconstructParam.ReconstructParam
    
    # read projects list
    projectList = []
    with open(input_csv, "r") as f:
        reader = csv.reader(f)
        for row in reader:
            project = {}
            project["dir"]  = row[0]
            project["sfm_data"]  = row[1]
            project["A"] = row[2]
            projectList.append(project)
    
    # copy source files to output directory
    for project in projectList:
        copyOriginalFiles(project["dir"], output_dir)
    
    # prepare output directory
    if not os.path.isdir(os.path.join(output_dir,"Ref")):
        FileUtils.makedir(os.path.join(output_dir,"Ref"))
    if not os.path.isdir(os.path.join(output_dir,"Ref","loc")):
        FileUtils.makedir(os.path.join(output_dir,"Ref","loc"))
    if not os.path.isdir(os.path.join(output_dir,"Output","SfM")):
        FileUtils.makedir(os.path.join(output_dir,"Output","SfM"))
    if not os.path.isdir(os.path.join(output_dir,"Output","SfM","reconstruction")):
        FileUtils.makedir(os.path.join(output_dir,"Output","SfM","reconstruction"))
    if not os.path.isdir(os.path.join(output_dir,"Output","SfM","reconstruction","global")):
        FileUtils.makedir(os.path.join(output_dir,"Output","SfM","reconstruction","global"))
    
    sfmDataList = []
    sfmViewBeaconDataList = []
    sfmBeaconMap = None
    for project in projectList:
        if not os.path.exists(project["sfm_data"]):
            print "cannot find sfm data : " + project["sfm_data"]
            sys.exit()
        with open(project["sfm_data"]) as jsonFile:
            sfmDataList.append(json.load(jsonFile))
        
        sfmBeaconFile = os.path.join(os.path.dirname(project["sfm_data"]), "beacon.txt")
        if os.path.exists(sfmBeaconFile):
            print "find beacon.txt for sfm data : " + project["sfm_data"]
            imgBeaconList, beaconMap = iBeaconUtils.readBeaconData(sfmBeaconFile)
            sfmViewBeaconDataList.append(imgBeaconList)
            if sfmBeaconMap is None:
                sfmBeaconMap = beaconMap
            else:
                if sfmBeaconMap!=beaconMap:
                    print "invalid find beacon.txt for sfm data : " + project["sfm_data"]
                    print "beacon.txt should be same for all merged sfm_data"
                    sys.exit()
                else:
                    print "valid beacon.txt for sfm data : " + project["sfm_data"]
    
    AList = []
    for project in projectList:
        AList.append(np.loadtxt(project["A"]))
        print "load mat : " + project["A"]
        print (np.loadtxt(project["A"]))
    
    print "Load 3D points"
    pointIdList = []
    pointList = []
    for sfmData in sfmDataList:
        pointId, point = mergeSfM.getAll3DPointloc(sfmData)
        pointn = np.asarray(point, dtype=np.float).T
        
        pointIdList.append(pointId)
        pointList.append(pointn)
    
    # merge models
    mergeSfmData = None
    mergePointId = None
    mergePointn = None
    mergeSfmViewBeaconData = None
    for idx in range(0, len(sfmDataList)):
        if idx==0:
            mergeSfmData = sfmDataList[0]
            mergeSfM.transform_sfm_data(mergeSfmData, AList[0])
            if len(sfmViewBeaconDataList)>0:
                mergeSfmViewBeaconData = sfmViewBeaconDataList[0]
        else:
            mergePointThres = mergeSfM.findMedianStructurePointsThres(mergeSfmData, reconstructParam.mergePointThresMul)
            print "thres to merge 3D points : " + str(mergePointThres)
            
            inlierMap = findInliersByKnownTransform(mergePointId, pointIdList[idx], mergePointn, pointList[idx], AList[idx], mergePointThres)
            print "number of points in base model : " + str(len(mergePointn[0]))
            print "number of points in model " + str(idx) + " : " + str(len(pointList[idx]))
            print "number of inliers : " + str(len(inlierMap))
            if len(sfmViewBeaconDataList)>0:
                mergeSfM.merge_sfm_data(mergeSfmData, sfmDataList[idx], AList[idx], {x[0]: x[1] for x in inlierMap}, mergeSfmViewBeaconData, sfmViewBeaconDataList[idx])
            else:
                mergeSfM.merge_sfm_data(mergeSfmData, sfmDataList[idx], AList[idx], {x[0]: x[1] for x in inlierMap})
        
        mergePointId, mergePoint = mergeSfM.getAll3DPointloc(mergeSfmData)
        mergePointn = np.asarray(mergePoint, dtype=np.float).T
    
    # go back to coordinate of the first model
    _invA = np.linalg.inv(AList[0][0:3,0:3])
    invA = np.c_[_invA, -np.dot(_invA,AList[0][:,3])]
    mergeSfM.transform_sfm_data(mergeSfmData, invA)
    
    mergeSfmData["root_path"] = os.path.join(output_dir,"Input","inputImg")
    
    resultSfMDataFile = os.path.join(output_dir,"Output","SfM","reconstruction","global","sfm_data.json")
    
    with open(os.path.join(resultSfMDataFile),"w") as jsonfile:
        json.dump(mergeSfmData, jsonfile)
    
    if mergeSfmViewBeaconData is not None:
        mergeSfmViewBeaconDataMapList = []
        for key in mergeSfmViewBeaconData:
            mergeSfmViewBeaconDataMap = {}
            mergeSfmViewBeaconDataMap[key] = mergeSfmViewBeaconData[key]
            mergeSfmViewBeaconDataMapList.append(mergeSfmViewBeaconDataMap)
        iBeaconUtils.exportBeaconData(len(mergeSfmData["views"]), sfmBeaconMap, mergeSfmViewBeaconDataMapList, 
                                      os.path.join(os.path.dirname(resultSfMDataFile), "beacon.txt"))
    
    '''
    os.system(reconstructParam.BUNDLE_ADJUSTMENT_PROJECT_PATH + " " + resultSfMDataFile + " " + resultSfMDataFile)
    '''
    
    Amat = AList[0]
    with open(os.path.join(output_dir,"Ref","Amat.txt"),"w") as AmatFile:
        np.savetxt(AmatFile,Amat)
    FileUtils.convertNumpyMatTxt2OpenCvMatYml(os.path.join(output_dir,"Ref","Amat.txt"), os.path.join(output_dir,"Ref","Amat.yml"), "A")
    
    # To create same directory structure before merging, create sfm_data.json without structure information in matches directory
    with open(resultSfMDataFile) as fpr:
        sfmData = json.load(fpr)
        sfmData["extrinsics"] = []
        sfmData["control_points"] = []
        sfmData["structure"] = []
        with open(os.path.join(output_dir,"Output","matches","sfm_data.json"),"w") as fpw:
            json.dump(sfmData, fpw)
    
    print "Execute : " + reconstructParam.WORKSPACE_DIR + "/TrainBoW/Release/TrainBoW " + os.path.join(output_dir,"Output") + " " + \
              os.path.join(output_dir,"Output", "matches", "BOWfile.yml") + " -p=" + os.path.join(output_dir,"Output", "matches", "PCAfile.yml")
    os.system(reconstructParam.WORKSPACE_DIR + "/TrainBoW/Release/TrainBoW " + os.path.join(output_dir,"Output") + " " + \
              os.path.join(output_dir,"Output", "matches", "BOWfile.yml") + " -p=" + os.path.join(output_dir,"Output", "matches", "PCAfile.yml"))
    
    os.system("openMVG_main_ComputeSfM_DataColor -i " + resultSfMDataFile + \
              " -o " + os.path.join(output_dir,"Output","SfM","reconstruction","global","colorized.ply"))

if __name__ == '__main__':
    main()