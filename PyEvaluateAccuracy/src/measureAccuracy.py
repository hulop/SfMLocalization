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

import sys
import os
import yajl as json
import numpy as np
import shutil
import hulo_file.PlyUtils as PlyUtis
import hulo_file.SfmDataUtils as SfmDataUtils
import hulo_sfm.mergeSfM as mergeSfM
import hulo_param.ReconstructParam as ReconstructParam

# set parameter
reconstructParam = ReconstructParam.ReconstructParam

rerunRef = 0
rerunTest = 0

# read a delimited file and return map from filename to location
def getMapNameLoc(filename,delimit=" "):
    
    mapNameLoc = {}
    with open(filename,"r") as fileLoc:
        for line in fileLoc:
            line = line.strip()
            line = line.split(delimit)
            
            loc = [float(x) for x in line[1:] if len(x)>0]
            if len(loc) == 3:
                mapNameLoc[line[0]] = loc
            else: 
                print "File " + line[0] + " has invalid location " + str(loc)

    return mapNameLoc
 
##### Error Codes #####
RC_SUCCESS = 'SUCCESS'
RC_LESS_THAN_FOUR_REF_POINTS_LOCALIZED = 'LESS_THAN_FOUR_REF_POINTS_LOCALIZED'
RC_FAIL_TO_FIND_WORLD_COORDINATE = 'FAIL_TO_FIND_WORLD_COORDINATE'
RC_NO_TEST_IMAGES_LOCALIZED = 'NO_TEST_IMAGES_LOCALIZED'
#######################   
def main(argv):
    # check arguments
    if (len(argv) != 3):
        print 'Usage : python %s [project directory] [sfm data directory]' % argv[0]
        quit()
    PROJECT_PATH = argv[1]
    SFM_DATA_DIR  = argv[2]
    
    MATCHES_DIR = PROJECT_PATH + "/Output/merge_result/Output/matches"
    REF_FOLDER = PROJECT_PATH + "/Ref"
    TEST_FOLDER = PROJECT_PATH + "/Test"
    
    if rerunRef or not os.path.isfile(os.path.join(REF_FOLDER,"Amat.txt")):
        
        # 1. find transformation between reconstructed coordinate and world coordinate
        
        # 1.1 localize reference images
        REF_FOLDER_LOC = os.path.join(REF_FOLDER,"loc")
        if os.path.isdir(REF_FOLDER_LOC):
            shutil.rmtree(REF_FOLDER_LOC)
        os.mkdir(REF_FOLDER_LOC)
            
        os.system(reconstructParam.LOCALIZE_PROJECT_PATH + \
                  " -q=" + os.path.join(REF_FOLDER,"inputImg") + \
                  " -s=" + SFM_DATA_DIR + \
                  " -m=" + MATCHES_DIR + \
                  " -f=" + "0.6" + \
                  " -r=" + "25" + \
                  " -o=" + REF_FOLDER_LOC + \
                  " -e=" + os.path.join(REF_FOLDER,"csv") + \
                  " -k=" + "200" + \
                  " -c=" + "0.5" + \
                  " -v=" + "2" + \
                  " -n=" + "1")
        
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
        mapNameLocRef = getMapNameLoc(os.path.join(REF_FOLDER,"refcoor.txt"))
        
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
            return RC_LESS_THAN_FOUR_REF_POINTS_LOCALIZED, None
        
        # find tranformation
        Amat, inliers = mergeSfM.ransacAffineTransform(np.array(worldCoor).T, np.array(locCoor).T, 0.3, ransacRound=1000)
        
        if len(inliers) < 5:
            print "Cannot estimate transformation matrix to world coordinate"
            print Amat
            return RC_FAIL_TO_FIND_WORLD_COORDINATE, None
#             sys.exit()
        
        print "Transformation matrix has " + str(len(inliers)) + "inliers"
        print Amat
        
        with open(os.path.join(REF_FOLDER,"Amat.txt"),"w") as AmatFile:
            np.savetxt(AmatFile,Amat)
    else:
        with open(os.path.join(REF_FOLDER,"Amat.txt"),"r") as AmatFile:
            Amat = np.loadtxt(AmatFile)
    
    TEST_FOLDER_LOC = os.path.join(TEST_FOLDER,"loc")
    if rerunTest or not os.path.isfile(os.path.join(TEST_FOLDER_LOC,"center.txt")):       
        
        # localize test images
        if os.path.isdir(TEST_FOLDER_LOC):
            shutil.rmtree(TEST_FOLDER_LOC)
        os.mkdir(TEST_FOLDER_LOC)
                
        os.system(reconstructParam.LOCALIZE_PROJECT_PATH + \
                  " -q=" + os.path.join(TEST_FOLDER,"inputImg") + \
                  " -s=" + SFM_DATA_DIR + \
                  " -m=" + MATCHES_DIR + \
                  " -f=" + "0.6" + \
                  " -r=" + "25" + \
                  " -o=" + TEST_FOLDER_LOC + \
                  " -e=" + os.path.join(TEST_FOLDER,"csv") + \
                  " -k=" + "200" + \
                  " -c=" + "0.5" + \
                  " -v=" + "2" + \
                  " -n=" + "1")
        
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
    
    # read test data
    mapNameLocTest = getMapNameLoc(os.path.join(TEST_FOLDER,"testcoor.txt"))
    
    # read localized json file and find its matching world coordinate
    worldCoorTest = []
    locCoorTest = []
    countLocTest = 0
    for filename in os.listdir(TEST_FOLDER_LOC):
        if filename[-4:] != "json":
            continue
        
        # read json localization file
        with open(os.path.join(TEST_FOLDER_LOC,filename)) as jsonlocfile:
            jsonLoc = json.load(jsonlocfile)
            
            imgLocName = os.path.basename(jsonLoc["filename"])
            
            # if file exist in map, add to matrix
            if imgLocName in mapNameLocTest:
                locCoorTest.append(jsonLoc["t"])
                worldCoorTest.append(mapNameLocTest[imgLocName])
                countLocTest = countLocTest + 1
            
    # transform loc coordinate to world coordinate
    print "From " + str(len(mapNameLocTest)) + " test images, " + str(countLocTest) + " images has been localized."
    if countLocTest == 0:
        return RC_NO_TEST_IMAGES_LOCALIZED, None
    locCoorTest1 = np.hstack((locCoorTest,np.ones((len(locCoorTest),1),dtype=np.float)))
    locCoorTestWorld = np.dot(Amat,locCoorTest1.T).T
    
    # calculate error
    normDiff = np.linalg.norm(worldCoorTest - locCoorTestWorld,axis=1)
    meanErr = np.mean(normDiff)
    medianErr = np.median(normDiff)
    print "Mean error = " + str(meanErr) + " meters."
    print "Median error = " + str(medianErr) + " meters."
    binEdge = [0.3*float(x) for x in range(0,11)]
    hist = np.histogram(normDiff,bins=binEdge)[0]
    print "Histogram of error: " + str(hist)
    print "Cumulative ratio: " + str(np.around(np.cumsum(hist,dtype=float)/countLocTest,2))
    print "Total loc err larger than " + str(np.max(binEdge)) + " meters: " + str(countLocTest-np.sum(hist))
    
    # convert ply file to world coordinate
    SfmDataUtils.saveGlobalSfM(os.path.join(SFM_DATA_DIR,"sfm_data.json"), os.path.join(REF_FOLDER,"Amat.txt"), os.path.join(SFM_DATA_DIR,"sfm_data_global.json"))
    os.system("openMVG_main_ComputeSfM_DataColor -i " + os.path.join(SFM_DATA_DIR,"sfm_data_global.json") + " -o " + os.path.join(SFM_DATA_DIR,"colorized_global.ply"))   
    PlyUtis.saveCameraPly(os.path.join(SFM_DATA_DIR,"sfm_data_global.json"), os.path.join(SFM_DATA_DIR,"colorized_global_camera.ply"))
    PlyUtis.saveStructurePly(os.path.join(SFM_DATA_DIR,"sfm_data_global.json"), os.path.join(SFM_DATA_DIR,"colorized_global_structure.ply"))
    
    # convert all localization results to world coordinate and merge to one json file
    mergedJsonLoc = []
    for filename in sorted(os.listdir(TEST_FOLDER_LOC)):
        if filename[-4:]!="json":
            continue
        with open(os.path.join(TEST_FOLDER_LOC,filename)) as jsonfile:
            jsonLoc = json.load(jsonfile)
            
            imgLocName = os.path.basename(jsonLoc["filename"])
            
            # if file exist in map
            if imgLocName in mapNameLocTest:
                jsonLoc["t"] = np.dot(Amat,np.concatenate([jsonLoc["t"],[1]])).tolist()
                jsonLoc["R"] = np.dot(Amat[:, 0:3],jsonLoc["R"]).tolist()
                jsonLoc["groundtruth"] = mapNameLocTest[imgLocName]
                mergedJsonLoc.append(jsonLoc)
    with open(os.path.join(SFM_DATA_DIR,"loc_global.json"),"w") as jsonfile:
        json.dump(mergedJsonLoc, jsonfile)
        
    return RC_SUCCESS, normDiff

if __name__ == '__main__':
    main(sys.argv)
