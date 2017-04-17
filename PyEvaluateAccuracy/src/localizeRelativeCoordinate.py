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
import shutil
import hulo_file.PlyUtils as PlyUtis
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
    description = 'This script is for testing localization in relative coordinate.' + \
        'Please note that localization results by this script is not for real world coordinate.'
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument('matches_dir', action='store', nargs=None, const=None, \
                        default=None, type=str, choices=None, metavar=None, \
                        help='Directory path where OpenMVG created matches files.')
    parser.add_argument('sfm_data_dir', action='store', nargs=None, const=None, \
                        default=None, type=str, choices=None, metavar=None, \
                        help='Directory path where OpenMVG sfm_data.json is located.')
    parser.add_argument('test_project_dir', action='store', nargs=None, const=None, \
                        default=None, type=str, choices=None, metavar=None, \
                        help='Directory path where localization test image files are located.')
    parser.add_argument('--bow', action='store_true', default=False, \
                        help='Use BOW to accelerate localization if this flag is set (default: False)')
    parser.add_argument('--beacon', action='store_true', default=False, \
                        help='Use iBeacon to accelerate localization if this flag is set (default: False)')
    args = parser.parse_args()
    matches_dir = args.matches_dir
    sfm_data_dir = args.sfm_data_dir
    test_project_dir = args.test_project_dir
    USE_BOW = args.bow
    USE_BEACON = args.beacon
    
    BOW_FILE = os.path.join(matches_dir, "BOWfile.yml")
    PCA_FILE = os.path.join(matches_dir, "PCAfile.yml")
    SFM_BEACON_FILE = sfm_data_dir + "/beacon.txt"
    
    if USE_BOW and not os.path.isfile(BOW_FILE):
        print "Use BOW flag is set, but cannot find BOW model file"
        sys.exit()
    if USE_BEACON and not os.path.isfile(SFM_BEACON_FILE):
        print "Use iBeacon flag is set, but cannot find beacon signal file for SfM data"
        sys.exit()
    
    # create ply file only for camera, structure
    PlyUtis.saveCameraPly(os.path.join(sfm_data_dir,"sfm_data.json"), os.path.join(sfm_data_dir,"colorized_camera.ply"))
    PlyUtis.saveStructurePly(os.path.join(sfm_data_dir,"sfm_data.json"), os.path.join(sfm_data_dir,"colorized_structure.ply"))
    
    # start localize test
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
            
            guideMatchOption = ""
            if reconstructParam.bGuidedMatchingLocalize:
                guideMatchOption = " -gm"            
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
                          " -p=" + PCA_FILE + \
                          guideMatchOption)
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
                          " -n=" + str(localizeIBeaconParam.normApproach) + \
                          guideMatchOption)
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
                          " -p=" + PCA_FILE + \
                          guideMatchOption)
            else:
                os.system(reconstructParam.LOCALIZE_PROJECT_PATH + \
                          " " + os.path.join(TEST_DIR,"inputImg") + \
                          " " + sfm_data_dir + \
                          " " + matches_dir + \
                          " " + TEST_FOLDER_LOC + \
                          " -f=" + str(localizeParam.locFeatDistRatio) + \
                          " -r=" + str(localizeParam.locRansacRound) + \
                          guideMatchOption)
            
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
            
        # merge all localization results to one json file
        locRelativeJsonObj = {}
        locRelativeJsonObj["locRelative"] = []
        locRelativePoints = []
        for filename in sorted(os.listdir(TEST_FOLDER_LOC)):
            if filename[-4:]!="json":
                continue
            with open(os.path.join(TEST_FOLDER_LOC,filename)) as jsonfile:
                jsonLoc = json.load(jsonfile)
                locRelativeJsonObj["locRelative"].append(jsonLoc)
                
                locRelativePoints.append(jsonLoc["t"])
        with open(os.path.join(TEST_FOLDER_LOC,"loc_relative.json"),"w") as jsonfile:
            json.dump(locRelativeJsonObj, jsonfile)
        
        # save localization results to ply file
        PlyUtis.addPointToPly(os.path.join(sfm_data_dir,"colorized_structure.ply"), locRelativePoints, 
                              os.path.join(TEST_FOLDER_LOC,"colorized_relative_localize.ply"))

if __name__ == '__main__':
    main()
