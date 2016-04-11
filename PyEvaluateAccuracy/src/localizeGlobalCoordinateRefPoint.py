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

import argparse
import os
import sys
import json
import numpy as np
from scipy.spatial import cKDTree
import shutil
import hulo_file.FileUtils as FileUtils
import hulo_file.PlyUtils as PlyUtis
import hulo_file.SfmDataUtils as SfmDataUtils
import hulo_sfm.mergeSfM as mergeSfM
import hulo_param.ReconstructParam as ReconstructParam
import hulo_param.LocalizeParam as LocalizeParam
import hulo_bow.LocalizeBOWParam as LocalizeBOWParam
import hulo_ibeacon.LocalizeIBeaconParam as LocalizeIBeaconParam
import hulo_ibeacon.ReconstructIBeaconParam as ReconstructIBeaconParam

# set parameter
reconstructParam = ReconstructParam.ReconstructParam
localizeParam = LocalizeParam.LocalizeParam
localizeBOWParam = LocalizeBOWParam.LocalizeBOWParam
localizeIBeaconParam = LocalizeIBeaconParam.LocalizeIBeaconParam
reconstructIBeaconParam = ReconstructIBeaconParam.ReconstructIBeaconParam

def reduceClosePoints(sfmData, Amat, thres):
    strKeyGlobalXMap = {}
    for strKey in range(0,len(sfmData['structure'])):
        struct = sfmData['structure'][strKey]
        strKeyGlobalXMap[strKey] = np.dot(Amat,np.concatenate([struct["value"]["X"],[1]]))
     
    reduceStructures = []
    for strKey1 in range(0,len(sfmData['structure'])):            
        struct1 = sfmData['structure'][strKey1]
        
        if struct1 not in reduceStructures:
            for strKey2 in range(strKey1+1,len(sfmData['structure'])):
                struct2 = sfmData['structure'][strKey2]
                
                if struct2 not in reduceStructures:
                    globalX1 = strKeyGlobalXMap[strKey1]
                    globalX2 = strKeyGlobalXMap[strKey2]
                    
                    dist = np.linalg.norm(globalX1-globalX2)
                    if dist < thres:
                        print "merge point " + str(struct1["key"]) + " and " + str(struct2["key"]) + " here..., dist = " + str(dist)
                        
                        for pt in struct2["value"]["observations"]:
                            sfmData["structure"][strKey1]["value"]["observations"].append(pt)
                        reduceStructures.append(struct2)
    
    print "start remove reduced points from sfmData..."
    for struct in reduceStructures:
        sfmData['structure'].remove(struct)
    print "finish remove reduced points from sfmData."

def reduceClosePointsKDTree(sfmData, Amat, thres, knn):
    allGlobalX = np.asarray([])
    for strKey in range(0,len(sfmData['structure'])):
        struct = sfmData['structure'][strKey]
        globalX = np.dot(Amat,np.concatenate([struct["value"]["X"],[1]]))
        if allGlobalX.shape[0]==0:
            allGlobalX = globalX
        else:
            allGlobalX = np.vstack((allGlobalX, globalX))
    
    print "start build kdtree : " + str(allGlobalX.shape)
    kdtree = cKDTree(allGlobalX)
    print "finish build kdtree"
    
    reduceStructures = []
    for strKey1 in range(0,len(sfmData['structure'])):            
        struct1 = sfmData['structure'][strKey1]
        
        if struct1 not in reduceStructures:
            print "strt query kdtree..."
            dist, indexes = kdtree.query(allGlobalX[strKey1], min(knn, allGlobalX.shape[0]))
            print "finish query kdtree."
            
            for idx in range(0,len(dist)):
                strKey2 = indexes[idx]
                if strKey1 < strKey2 and dist[idx] < thres:                    
                    struct2 = sfmData['structure'][strKey2]
                    if struct2 not in reduceStructures:
                        print "merge point " + str(struct1["key"]) + " and " + str(struct2["key"]) + " here..., dist = " + str(dist[idx])
                        
                        for pt in struct2["value"]["observations"]:
                            sfmData["structure"][strKey1]["value"]["observations"].append(pt)
                        reduceStructures.append(struct2)
    
    print "start remove reduced points from sfmData..."
    for struct in reduceStructures:
        sfmData['structure'].remove(struct)
    print "finish remove reduced points from sfmData."

def main():
    description = 'This script is for calcularing the matrix for converting 3D model to world coordinate and testing localization.' + \
        'Before running this script, please prepare the json file which has 3D coordinate for reference points in Ref folder.' + \
        'You can create reference points json file by sfmCoordinateEditor'
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument('project_dir', action='store', nargs=None, const=None, \
                        default=None, type=str, choices=None, metavar=None, \
                        help='Directory path where OpenMVG project is located.')
    parser.add_argument('matches_dir', action='store', nargs=None, const=None, \
                        default=None, type=str, choices=None, metavar=None, \
                        help='Directory path where OpenMVG created matches files.')
    parser.add_argument('sfm_data_dir', action='store', nargs=None, const=None, \
                        default=None, type=str, choices=None, metavar=None, \
                        help='Directory path where OpenMVG sfm_data.json is located.')
    parser.add_argument('-t', '--test-project-dir', action='store', nargs='?', const=None, \
                        default=None, type=str, choices=None, metavar=None, \
                        help='Directory path where localization test image files are located.')
    parser.add_argument('-o', '--output-json-filename', action='store', nargs='?', const=None, \
                        default='loc_global.json', type=str, choices=None, metavar=None, \
                        help='Output localization result json filename.')
    parser.add_argument('--bow', action='store_true', default=False, \
                        help='Use BOW to accelerate localization if this flag is set (default: False)')    
    parser.add_argument('--beacon', action='store_true', default=False, \
                        help='Use iBeacon to accelerate localization if this flag is set (default: False)')    
    parser.add_argument('--reduce-points', action='store_true', default=False, \
                        help='Reduce 3D points if points are close after transforming to global coordinate (default: False)')    
    args = parser.parse_args()
    project_dir = args.project_dir
    matches_dir = args.matches_dir
    sfm_data_dir = args.sfm_data_dir
    test_project_dir = args.test_project_dir
    output_json_filename = args.output_json_filename
    USE_BOW = args.bow
    USE_BEACON = args.beacon
    USE_REDUCE_POINTS = args.reduce_points
    
    BOW_FILE = os.path.join(matches_dir, "BOWfile.yml")
    PCA_FILE = os.path.join(matches_dir, "PCAfile.yml")
    SFM_BEACON_FILE = sfm_data_dir + "/beacon.txt"
    REF_FOLDER = project_dir + "/Ref"
    
    if USE_BOW and not os.path.isfile(BOW_FILE):
        print "Use BOW flag is set, but cannot find BOW model file"
        sys.exit()
    if USE_BEACON and not os.path.isfile(SFM_BEACON_FILE):
        print "Use iBeacon flag is set, but cannot find beacon signal file for SfM data"
        sys.exit()
    
    if not os.path.isfile(os.path.join(REF_FOLDER,"Amat.txt")):
        # 1. find transformation between reconstructed coordinate and world coordinate
        refPoints = FileUtils.loadRefPointsJson(os.path.join(REF_FOLDER,"refpoints.json"))
        
        # read localized json file and find its matching world coordinate
        worldCoor = []
        locCoor = []
        with open(os.path.join(sfm_data_dir, "sfm_data.json")) as fp:
            sfmData = json.load(fp)
            for pointId in refPoints:
                selectedPoint = [point for point in sfmData["structure"] if point["key"]==pointId][0]
                print "relative coordinate : " + str(selectedPoint["value"]["X"])
                print "world coordinate : " + str(refPoints[pointId])
                locCoor.append(selectedPoint["value"]["X"])
                worldCoor.append(refPoints[pointId])
                
        print "Number of reference points : " + str(len(worldCoor))
        
        if len(worldCoor) < 4:
            print "Cannot fix to world coordinate because of less than 4 reference points"
            return
        
        # find tranformation
        Amat, inliers = mergeSfM.ransacAffineTransform(np.array(worldCoor).T, np.array(locCoor).T, 
                                                       reconstructParam.ransacThresTransformWorldCoordinateRefPoint, ransacRound=1000)
        
        if len(inliers) < 5:
            print "Cannot estimate transformation matrix to world coordinate"
            print Amat
            return
        
        print "Transformation matrix has " + str(len(inliers)) + "inliers"
        print Amat
        
        with open(os.path.join(REF_FOLDER,"Amat.txt"),"w") as AmatFile:
            np.savetxt(AmatFile,Amat)
        FileUtils.convertNumpyMatTxt2OpenCvMatYml(os.path.join(REF_FOLDER,"Amat.txt"), os.path.join(REF_FOLDER,"Amat.yml"), "A")
    else:
        with open(os.path.join(REF_FOLDER,"Amat.txt"),"r") as AmatFile:
            Amat = np.loadtxt(AmatFile)
    
    if USE_REDUCE_POINTS:
        print "start reducing 3D points..."
        
        with open(os.path.join(sfm_data_dir, "sfm_data.json")) as fp:
            sfmData = json.load(fp)
        
        with open(os.path.join(REF_FOLDER,"Amat.txt"),"r") as AmatFile:
            Amat = np.loadtxt(AmatFile)
                
        print "point size before reducing : " + str(len(sfmData['structure']))
        #reduceClosePoints(sfmData, Amat, 0.01)
        reduceClosePointsKDTree(sfmData, Amat, 0.01, 1000)
        print "point size after reducing : " + str(len(sfmData['structure']))
        
        print "start save reduced sfm data..."
        os.system("cp " + os.path.join(sfm_data_dir, "sfm_data.json") + " " + os.path.join(sfm_data_dir, "sfm_data_b4rp.json"))
        with open(os.path.join(sfm_data_dir, "sfm_data.json"), "w") as fp:
            json.dump(sfmData, fp)
        print "finish save reduced sfm data..."
        
        print "finish reducing 3D points."
    
    # convert ply file to world coordinate
    SfmDataUtils.saveGlobalSfM(os.path.join(sfm_data_dir,"sfm_data.json"), os.path.join(REF_FOLDER,"Amat.txt"), os.path.join(sfm_data_dir,"sfm_data_global.json"))
    os.system("openMVG_main_ComputeSfM_DataColor -i " + os.path.join(sfm_data_dir,"sfm_data_global.json") + " -o " + os.path.join(sfm_data_dir,"colorized_global.ply"))   
    PlyUtis.saveCameraPly(os.path.join(sfm_data_dir,"sfm_data_global.json"), os.path.join(sfm_data_dir,"colorized_global_camera.ply"))
    PlyUtis.saveStructurePly(os.path.join(sfm_data_dir,"sfm_data_global.json"), os.path.join(sfm_data_dir,"colorized_global_structure.ply"))
    
    # start localize test
    if test_project_dir:
        for testFolder in sorted(os.listdir(test_project_dir)):
            TEST_DIR = os.path.join(test_project_dir,testFolder)
            
            if not os.path.exists(os.path.join(TEST_DIR,"inputImg")):
                continue
            
            TEST_FOLDER_LOC = os.path.join(TEST_DIR,"loc")
            if not os.path.isfile(os.path.join(TEST_FOLDER_LOC,"center.txt")):
                
                # localize test images
                if os.path.isdir(TEST_FOLDER_LOC):
                    shutil.rmtree(TEST_FOLDER_LOC)
                os.mkdir(TEST_FOLDER_LOC)
                
                if USE_BOW and not USE_BEACON:
                    os.system(reconstructParam.LOCALIZE_PROJECT_PATH + \
                              " " + os.path.join(TEST_DIR,"inputImg") + \
                              " " + sfm_data_dir + \
                              " " + matches_dir + \
                              " " + TEST_FOLDER_LOC + \
                              " -f=" + str(localizeParam.locFeatDistRatio) + \
                              " -r=" + str(localizeParam.locRansacRound) + \
                              " -k=" + str(localizeBOWParam.locKNNnum) + \
                              " -a=" + BOW_FILE + \
                              " -p=" + PCA_FILE)
                elif not USE_BOW and USE_BEACON:
                    os.system(reconstructIBeaconParam.LOCALIZE_PROJECT_PATH + \
                              " " + os.path.join(TEST_DIR,"inputImg") + \
                              " " + sfm_data_dir + \
                              " " + matches_dir + \
                              " " + TEST_FOLDER_LOC + \
                              " -f=" + str(localizeParam.locFeatDistRatio) + \
                              " -r=" + str(localizeParam.locRansacRound) + \
                              " -b=" + SFM_BEACON_FILE + \
                              " -e=" + os.path.join(TEST_DIR,"csv") + \
                              " -k=" + str(localizeIBeaconParam.locKNNnum) + \
                              " -c=" + str(localizeIBeaconParam.coocThres) + \
                              " -v=" + str(localizeIBeaconParam.locSkipSelKNN) + \
                              " -n=" + str(localizeIBeaconParam.normApproach))                
                elif USE_BOW and USE_BEACON:
                    os.system(reconstructIBeaconParam.LOCALIZE_PROJECT_PATH + \
                              " " + os.path.join(TEST_DIR,"inputImg") + \
                              " " + sfm_data_dir + \
                              " " + matches_dir + \
                              " " + TEST_FOLDER_LOC + \
                              " -f=" + str(localizeParam.locFeatDistRatio) + \
                              " -r=" + str(localizeParam.locRansacRound) + \
                              " -b=" + SFM_BEACON_FILE + \
                              " -e=" + os.path.join(TEST_DIR,"csv") + \
                              " -k=" + str(localizeIBeaconParam.locKNNnum) + \
                              " -c=" + str(localizeIBeaconParam.coocThres) + \
                              " -v=" + str(localizeIBeaconParam.locSkipSelKNN) + \
                              " -n=" + str(localizeIBeaconParam.normApproach) + \
                              " -kb=" + str(localizeBOWParam.locKNNnum) + \
                              " -a=" + BOW_FILE + \
                              " -p=" + PCA_FILE)
                else:
                    os.system(reconstructParam.LOCALIZE_PROJECT_PATH + \
                              " " + os.path.join(TEST_DIR,"inputImg") + \
                              " " + sfm_data_dir + \
                              " " + matches_dir + \
                              " " + TEST_FOLDER_LOC + \
                              " -f=" + str(localizeParam.locFeatDistRatio) + \
                              " -r=" + str(localizeParam.locRansacRound))
                
                # extract centers from all json file and write to a file
                fileLoc = open(os.path.join(TEST_FOLDER_LOC,"center.txt"),"w")
                countLocFrame = 0
                
                for filename in sorted(os.listdir(TEST_FOLDER_LOC)):
                    if filename[-4:]!="json":
                        continue
                    
                    countLocFrame = countLocFrame + 1
                    with open(os.path.join(TEST_FOLDER_LOC,filename)) as locJson:
                        #print os.path.join(sfm_locOut,filename)
                        locJsonDict = json.load(locJson)
                        loc = locJsonDict["t"]
                        fileLoc.write(str(loc[0]) + " "  + str(loc[1]) + " "  +str(loc[2]) + " 255 0 0\n" )
                
                fileLoc.close()
            
            # convert all localization results to world coordinate and merge to one json file
            locGlobalJsonObj = {}
            locGlobalJsonObj["locGlobal"] = []
            locGlobalPoints = []
            for filename in sorted(os.listdir(TEST_FOLDER_LOC)):
                if filename[-4:]!="json":
                    continue
                with open(os.path.join(TEST_FOLDER_LOC,filename)) as jsonfile:
                    jsonLoc = json.load(jsonfile)
                    
                    jsonLoc["t_relative"] = jsonLoc["t"]
                    jsonLoc["R_relative"] = jsonLoc["R"]
                    jsonLoc["t"] = np.dot(Amat,np.concatenate([jsonLoc["t"],[1]])).tolist()
                    jsonLoc["R"] = np.dot(Amat[:, 0:3],jsonLoc["R"]).tolist()
                    locGlobalJsonObj["locGlobal"].append(jsonLoc)
                    
                    locGlobalPoints.append(jsonLoc["t"])
            with open(os.path.join(TEST_FOLDER_LOC, output_json_filename),"w") as jsonfile:
                json.dump(locGlobalJsonObj, jsonfile)
            
            # save localization results to ply file
            PlyUtis.addPointToPly(os.path.join(sfm_data_dir,"colorized_global_structure.ply"), locGlobalPoints, 
                                  os.path.join(TEST_FOLDER_LOC,"colorized_global_localize.ply"))

if __name__ == '__main__':
    main()
