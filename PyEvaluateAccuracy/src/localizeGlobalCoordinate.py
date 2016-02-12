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
import yajl as json
import numpy as np
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

def main():
    description = 'This script is for calcularing the matrix for converting 3D model to world coordinate and testing localization.' + \
        'Before running this script, please prepare the text file which has image names and 3D coordinate where photos are taken in Ref folder.'
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
    parser.add_argument('--bow', action='store_true', default=False, \
                        help='Use BOW to accelerate localization if this flag is set (default: False)')    
    parser.add_argument('--beacon', action='store_true', default=False, \
                        help='Use iBeacon to accelerate localization if this flag is set (default: False)')    
    args = parser.parse_args()
    project_dir = args.project_dir
    matches_dir = args.matches_dir
    sfm_data_dir = args.sfm_data_dir
    test_project_dir = args.test_project_dir
    USE_BOW = args.bow
    USE_BEACON = args.beacon
    
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
        
        # 1.1 localize reference images
        REF_FOLDER_LOC = os.path.join(REF_FOLDER,"loc")
        if os.path.isdir(REF_FOLDER_LOC):
            shutil.rmtree(REF_FOLDER_LOC)
        os.mkdir(REF_FOLDER_LOC)
        
        if USE_BOW and not USE_BEACON:
            os.system(reconstructParam.LOCALIZE_PROJECT_PATH + \
                      " " + os.path.join(REF_FOLDER,"inputImg") + \
                      " " + sfm_data_dir + \
                      " " + matches_dir + \
                      " " + REF_FOLDER_LOC + \
                      " -f=" + str(localizeParam.locFeatDistRatio) + \
                      " -r=" + str(localizeParam.locRansacRound) + \
                      " -k=" + str(localizeBOWParam.locKNNnum) + \
                      " -a=" + BOW_FILE + \
                      " -p=" + PCA_FILE)
        elif not USE_BOW and USE_BEACON:
            os.system(reconstructIBeaconParam.LOCALIZE_PROJECT_PATH + \
                      " " + os.path.join(REF_FOLDER,"inputImg") + \
                      " " + sfm_data_dir + \
                      " " + matches_dir + \
                      " " + REF_FOLDER_LOC + \
                      " -f=" + str(localizeParam.locFeatDistRatio) + \
                      " -r=" + str(localizeParam.locRansacRound) + \
                      " -b=" + SFM_BEACON_FILE + \
                      " -e=" + os.path.join(REF_FOLDER,"csv") + \
                      " -k=" + str(localizeIBeaconParam.locKNNnum) + \
                      " -c=" + str(localizeIBeaconParam.coocThres) + \
                      " -v=" + str(localizeIBeaconParam.locSkipSelKNN) + \
                      " -n=" + str(localizeIBeaconParam.normApproach))
        elif USE_BOW and USE_BEACON:
            os.system(reconstructIBeaconParam.LOCALIZE_PROJECT_PATH + \
                      " " + os.path.join(REF_FOLDER,"inputImg") + \
                      " " + sfm_data_dir + \
                      " " + matches_dir + \
                      " " + REF_FOLDER_LOC + \
                      " -f=" + str(localizeParam.locFeatDistRatio) + \
                      " -r=" + str(localizeParam.locRansacRound) + \
                      " -b=" + SFM_BEACON_FILE + \
                      " -e=" + os.path.join(REF_FOLDER,"csv") + \
                      " -k=" + str(localizeIBeaconParam.locKNNnum) + \
                      " -c=" + str(localizeIBeaconParam.coocThres) + \
                      " -v=" + str(localizeIBeaconParam.locSkipSelKNN) + \
                      " -n=" + str(localizeIBeaconParam.normApproach) + \
                      " -kb=" + str(localizeBOWParam.locKNNnum) + \
                      " -a=" + BOW_FILE + \
                      " -p=" + PCA_FILE)
        else:
            os.system(reconstructParam.LOCALIZE_PROJECT_PATH + \
                      " " + os.path.join(REF_FOLDER,"inputImg") + \
                      " " + sfm_data_dir + \
                      " " + matches_dir + \
                      " " + REF_FOLDER_LOC + \
                      " -f=" + str(localizeParam.locFeatDistRatio) + \
                      " -r=" + str(localizeParam.locRansacRound))
        
        # extract centers from all json file and write to a file
        fileLoc = open(os.path.join(REF_FOLDER_LOC,"center.txt"),"w")
        countLocFrame = 0
        
        for filename in sorted(os.listdir(REF_FOLDER_LOC)):
            if filename[-4:]!="json":
                continue
                                    
            countLocFrame = countLocFrame + 1
            with open(os.path.join(REF_FOLDER_LOC,filename)) as locJson:
                #print os.path.join(sfm_locOut,filename)
                locJsonDict = json.load(locJson)
                loc = locJsonDict["t"]
                fileLoc.write(str(loc[0]) + " "  + str(loc[1]) + " "  +str(loc[2]) + " 255 0 0\n" )  
        
        fileLoc.close() 
        
        # read reference data
        mapNameLocRef = FileUtils.loadImageLocationListTxt(os.path.join(REF_FOLDER,"refcoor.txt"))
        
        # read localized json file and find its matching world coordinate
        worldCoor = []
        locCoor = []
        countLoc = 0
        for filename in os.listdir(REF_FOLDER_LOC):
            if filename[-4:] != "json":
                continue
            
            # read json localization file
            with open(os.path.join(REF_FOLDER_LOC,filename)) as jsonlocfile:
                jsonLoc = json.load(jsonlocfile)
                
                imgLocName = os.path.basename(jsonLoc["filename"])
                
                # if file exist in map, add to matrix
                if imgLocName in mapNameLocRef:
                    locCoor.append(jsonLoc["t"])
                    worldCoor.append(mapNameLocRef[imgLocName])
                    countLoc = countLoc + 1
        
        print "From " + str(len(mapNameLocRef)) + " reference images, " + str(countLoc) + " images has been localized."
        
        if countLoc < 4:
            print "Cannot fix to world coordinate because of less than 4 reference points"
            return
        
        # find tranformation
        Amat, inliers = mergeSfM.ransacAffineTransform(np.array(worldCoor).T, np.array(locCoor).T, 0.3, ransacRound=1000)
        
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
    
    # convert ply file to world coordinate
    SfmDataUtils.saveGlobalSfM(os.path.join(sfm_data_dir,"sfm_data.json"), os.path.join(REF_FOLDER,"Amat.txt"), os.path.join(sfm_data_dir,"sfm_data_global.json"))
    os.system("openMVG_main_ComputeSfM_DataColor -i " + os.path.join(sfm_data_dir,"sfm_data_global.json") + " -o " + os.path.join(sfm_data_dir,"colorized_global.ply"))   
    PlyUtis.saveCameraPly(os.path.join(sfm_data_dir,"sfm_data_global.json"), os.path.join(sfm_data_dir,"colorized_global_camera.ply"))
    PlyUtis.saveStructurePly(os.path.join(sfm_data_dir,"sfm_data_global.json"), os.path.join(sfm_data_dir,"colorized_global_structure.ply"))
    
    # start localize test
    if test_project_dir:
        for testFolder in sorted(os.listdir(test_project_dir)):
            TEST_DIR = os.path.join(test_project_dir,testFolder)
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
            with open(os.path.join(TEST_FOLDER_LOC,"loc_global.json"),"w") as jsonfile:
                json.dump(locGlobalJsonObj, jsonfile)
            
            # save localization results to ply file
            PlyUtis.addPointToPly(os.path.join(sfm_data_dir,"colorized_global_structure.ply"), locGlobalPoints, 
                                  os.path.join(TEST_FOLDER_LOC,"colorized_global_localize.ply"))

if __name__ == '__main__':
    main()
