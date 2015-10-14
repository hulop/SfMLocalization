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
import shutil
import hulo_file.PlyUtils as PlyUtis
import hulo_param.ReconstructParam as ReconstructParam

# set parameter
reconstructParam = ReconstructParam.ReconstructParam

rerunRef = 0
rerunTest = 0

def main(argv):
    # check arguments
    if (len(argv) != 4):
        print 'Usage : python %s [project directory] [sfm data directory] [test image directory]' % argv[0]
        quit()
    PROJECT_PATH = argv[1]
    SFM_DATA_DIR  = argv[2]
    TEST_DIR  = argv[3]
    
    MATCHES_DIR = PROJECT_PATH + "/Output/merge_result/Output/matches"    
    TEST_FOLDER_LOC = os.path.join(TEST_DIR,"loc")
    if rerunTest or not os.path.isfile(os.path.join(TEST_FOLDER_LOC,"center.txt")):       
        
        # localize test images
        if os.path.isdir(TEST_FOLDER_LOC):
            shutil.rmtree(TEST_FOLDER_LOC)
        os.mkdir(TEST_FOLDER_LOC)
        
        os.system(reconstructParam.LOCALIZE_PROJECT_PATH + \
            " -q=" + os.path.join(TEST_DIR,"inputImg") + \
            " -s=" + SFM_DATA_DIR + \
            " -m=" + MATCHES_DIR + \
            " -f=" + "0.6" + \
            " -r=" + "25" + \
            " -o=" + TEST_FOLDER_LOC + \
            " -e=" + os.path.join(TEST_DIR,"csv") + \
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
                  
    # create ply file only for camera, structure
    PlyUtis.saveCameraPly(os.path.join(SFM_DATA_DIR,"sfm_data.json"), os.path.join(SFM_DATA_DIR,"colorized_camera.ply"))
    PlyUtis.saveStructurePly(os.path.join(SFM_DATA_DIR,"sfm_data.json"), os.path.join(SFM_DATA_DIR,"colorized_structure.ply"))

    # merge all localization results to one json file
    mergedJsonLoc = []
    for filename in sorted(os.listdir(TEST_FOLDER_LOC)):
        if filename[-4:]!="json":
            continue
        with open(os.path.join(TEST_FOLDER_LOC,filename)) as jsonfile:
            jsonLoc = json.load(jsonfile)
            mergedJsonLoc.append(jsonLoc)
    with open(os.path.join(SFM_DATA_DIR,"loc.json"),"w") as jsonfile:
        json.dump(mergedJsonLoc, jsonfile)


if __name__ == '__main__':
    main(sys.argv)
