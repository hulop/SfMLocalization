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

################################################################################
# reconstruct multiple video then merge by localizing one video in another
# until all videos are merged into one
#
# Assumptions
# - no same image file name in any project
# - order merge according to sorted video folder name
################################################################################

import sys
import os
import yajl as json
import time
import hulo_sfm.cleanSfM as cleanSfM
import hulo_sfm.sfmMergeGraph as sfmMergeGraph
import hulo_file.FileUtils as FileUtils
import hulo_param.ReconstructParam as ReconstructParam

def main(argv):
    # check arguments
    if (len(argv) != 2):
        print 'Usage : python %s [project directory]' % argv[0]
        quit()
    PROJECT_PATH = argv[1]
    
    # set parameter
    reconstructParam = ReconstructParam.ReconstructParam
        
    # get paths
    inputPath = os.path.join(PROJECT_PATH, "Input")
    outputPath = os.path.join(PROJECT_PATH, "Output")
    
    FileUtils.makedir(outputPath)
    
    # reconstruct all videos
    listVideo = sorted(os.listdir(inputPath))
    for video in listVideo:
        if not os.path.isdir(os.path.join(inputPath, video)):
            continue
        
        print "Begin reconstructing video : " + video
        
        sfm_mainDir = os.path.join(outputPath, video)
        sfm_inputDir = os.path.join(inputPath, video)
        sfm_inputImgDir = os.path.join(sfm_inputDir, "inputImg")
        sfm_matchesDir = os.path.join(sfm_mainDir, "matches")
        sfm_sfmDir = os.path.join(sfm_mainDir, "SfM")
        sfm_reconstructDir = os.path.join(sfm_sfmDir, "reconstruction")
        sfm_globalDir = os.path.join(sfm_reconstructDir, "global")
                    
        FileUtils.makedir(sfm_mainDir)
        FileUtils.makedir(sfm_inputImgDir)
        FileUtils.makedir(sfm_matchesDir)
        FileUtils.makedir(sfm_sfmDir)
        FileUtils.makedir(sfm_reconstructDir)
        FileUtils.makedir(sfm_globalDir)
        
        if not os.path.isfile(os.path.join(sfm_globalDir, "sfm_data.json")):
            # list images
            os.system("openMVG_main_SfMInit_ImageListing -i " + sfm_inputImgDir + " -o " + sfm_matchesDir + " -d /usr/local/share/openMVG/cameraGenerated.txt")
            
            # 1.1 Check intrinsic
            # ( if camera parameter not specified then replace with fixed camera.
            # and set appropriate width and height)
            with open(os.path.join(sfm_matchesDir, "sfm_data.json")) as sfm_data_file:
                sfm_data = json.load(sfm_data_file)
                hImg = sfm_data["views"][0]['value']['ptr_wrapper']['data']["height"]
                wImg = sfm_data["views"][0]['value']['ptr_wrapper']['data']["width"]
                if len(sfm_data["intrinsics"]) == 0:
                    sfm_data["intrinsics"].append({})
                    sfm_data["intrinsics"][0]["key"] = 0
                    sfm_data["intrinsics"][0]["values"] = {}
                    # sfm_data["intrinsics"][0]["values"]["polymorphic_name"] = "pinhole_radial_k3"
                    sfm_data["intrinsics"][0]["values"]["polymorphic_name"] = "pinhole"
                    sfm_data["intrinsics"][0]["values"]["polymorphic_id"] = 2147999999
                    sfm_data["intrinsics"][0]["values"]["ptr_wrapper"] = {}
                    sfm_data["intrinsics"][0]["values"]["ptr_wrapper"]["id"] = 2147483660
                    sfm_data["intrinsics"][0]["values"]["ptr_wrapper"]["data"] = {}
                    sfm_data["intrinsics"][0]["values"]["ptr_wrapper"]["data"]["width"] = wImg
                    sfm_data["intrinsics"][0]["values"]["ptr_wrapper"]["data"]["height"] = hImg
                    sfm_data["intrinsics"][0]["values"]["ptr_wrapper"]["data"]["focal_length"] = reconstructParam.focalLength
                    sfm_data["intrinsics"][0]["values"]["ptr_wrapper"]["data"]["disto_k3"] = [0, 0, 0]
                    sfm_data["intrinsics"][0]["values"]["ptr_wrapper"]["data"]["principal_point"] = [wImg / 2, hImg / 2]
                    
            with open(os.path.join(sfm_matchesDir, "sfm_data.json"), "w") as sfm_data_file:
                json.dump(sfm_data, sfm_data_file)
                
            # 2 - Features computation and matching
            # ( Compute a list of features & descriptors for each image)
            os.system(reconstructParam.EXTRACT_FEATURE_MATCH_PROJECT_PATH + \
                      " -x=" + str(reconstructParam.maxTrackletMatchDistance) + \
                      " -a=" + str(reconstructParam.minMatchToRetain) + \
                      " -m=" + sfm_matchesDir + \
                      " -f=" + str(reconstructParam.extFeatDistRatio) + \
                      " -r=" + str(reconstructParam.extFeatRansacRound))
            
            # OpenMVG assumes matches.e.txt for global reconstruction, matches.f.txt for incremental reconstruction
            os.system("cp " + os.path.join(sfm_matchesDir, "matches.f.txt") + " " + os.path.join(sfm_matchesDir, "matches.e.txt"))
            
            # 3 - Global reconstruction
            countRecon = 1
            while not os.path.isfile(os.path.join(sfm_globalDir, "sfm_data.json")) and countRecon < reconstructParam.rerunRecon:  
                os.system("openMVG_main_GlobalSfM -i " + os.path.join(sfm_matchesDir, "sfm_data.json") + " -m " + sfm_matchesDir + " -o " + sfm_globalDir)
                countRecon = countRecon + 1
                time.sleep(1)
            
            if not os.path.isfile(os.path.join(sfm_globalDir, "sfm_data.json")):
                continue
                
            # 4 - Color the pointcloud
            os.system("openMVG_main_ComputeSfM_DataColor -i " + os.path.join(sfm_globalDir, "sfm_data.json") + " -o " + os.path.join(sfm_globalDir, "colorized.ply"))
            
            # 4.5 remove part of reconstruction where it is incorrect
            # Specifically,sometimes when their matching is not adequate,
            # the reconstructed model will be divided into two or more models
            # with different scale and a "jump" between pose translation.
            # This function detects such jump and retain the the largest 
            # beginning or ending part of reconstruction, while the rest
            # should be reconstructed separately by cleanSfM.
            countCut = 0
            # keep cutting until no more cut
            while cleanSfM.cutSfMDataJump(os.path.join(sfm_globalDir, "sfm_data.json"), bufferFrame=reconstructParam.bufferFrame):
                countCut = countCut + 1
                os.rename(os.path.join(sfm_globalDir, "sfm_data_BC.json"),
                          os.path.join(sfm_globalDir, "sfm_data_BC" + str(countCut) + ".json"))
                os.system("" + reconstructParam.WORKSPACE_DIR + "/" + reconstructParam.BUNDLE_ADJUSTMENT_PROJECT + "/" + reconstructParam.C_PROJECT_CONFIG + "/" + reconstructParam.BUNDLE_ADJUSTMENT_PROJECT + \
                          " -s=" + os.path.join(sfm_globalDir, "sfm_data.json") + \
                          " -o=" + os.path.join(sfm_globalDir, "sfm_data.json") + \
                          " -c=" + "rs,rst,rsti")
            os.system("openMVG_main_ComputeSfM_DataColor -i " + os.path.join(sfm_globalDir, "sfm_data.json") + " -o " + os.path.join(sfm_globalDir, "colorized_AC.ply"))
         
            # 5 - Clean sfm_data by removing viewID of frames that are not used
            # in reconstruction and put them in another folder and reconstruct them again
            # note that sfm_data.json in matches folder is renamed and kept as reference
            unusedImg = cleanSfM.cleanSfM(os.path.join(sfm_globalDir, "sfm_data.json"),
                                 [os.path.join(sfm_matchesDir, "matches.putative.txt"),
                                  os.path.join(sfm_matchesDir, "matches.e.txt"),
                                  os.path.join(sfm_matchesDir, "matches.f.txt")])
            
            # 6. move unused images, csv files into a new folder unless they have less than x images
            for i in range(0, len(unusedImg)):
                listUnused = unusedImg[i]
                if len(listUnused) < reconstructParam.minUnusedImgLength:
                    continue
                
                # set name for new video
                if i == 0:
                    newVidName = video + "_front"
                elif i == 1:
                    newVidName = video + "_back"
                else:
                    # this should not be called
                    continue
                
                # set path
                pathNewVid = os.path.join(inputPath, newVidName)
                
                # skip if there is already this folder
                if os.path.isdir(pathNewVid):
                    continue
                
                print "Extract unused part of " + video + " into " + newVidName
                
                FileUtils.makedir(pathNewVid)
                
                csvNewVid = os.path.join(pathNewVid, "csv")
                imgNewVid = os.path.join(pathNewVid, "inputImg")
                FileUtils.makedir(csvNewVid)
                FileUtils.makedir(imgNewVid)
                
                # copy image in list and csv file
                os.system("cp -s " + os.path.join(sfm_inputDir, "csv", "*.csv") + " " + csvNewVid)
                for unusedFilename in listUnused:
                    os.system("cp -s " + os.path.join(sfm_inputImgDir, unusedFilename) + " " + imgNewVid)
                
                # append the folder into reconstruction queue
                listVideo.append(newVidName)
    
    # train bag of words model
    if False and not os.path.isfile(os.path.join(outputPath, "merge_result", "Output", "matches", "BOWfile.yml")):
        print "Training BOW"
        os.system(reconstructParam.WORKSPACE_DIR + "/TrainBoW/Release/TrainBoW " + outputPath + " " + \
                  os.path.join(outputPath, "merge_result", "Output", "matches", "BOWfile.yml"))
    
    # load graph structure from "mergeGraph.txt" if it exists
    # create new graph structure if it does not exist
    if os.path.isfile(os.path.join(outputPath, "merge_result", "Output", "SfM", "reconstruction", "mergeGraph.txt")):
        sfmGraph = sfmMergeGraph.sfmGraph.load(os.path.join(outputPath, "merge_result", "Output", "SfM", "reconstruction", "mergeGraph.txt"))
        sfmGraph.workspacePath = reconstructParam.WORKSPACE_DIR
        
        #### start of manually adding new model code ####
        # In current code, you cannot add new 3D model once you start merging.
        # Enable following commented code to add new 3D model after you already started merging.
        '''
        newModelToAdd = []
        for newModelName in newModelToAdd:
            addModel(newModelName,os.path.join(inputPath,newModelName),os.path.join(outputPath,newModelName))
        sfmGraph.clearBadMatches()
        '''
        ### end of manually adding new model code ###
    else:
        sfmGraph = sfmMergeGraph.sfmGraph(inputPath,
                                          outputPath,
                                          os.path.join(outputPath, "merge_result", "Input"),
                                          os.path.join(outputPath, "merge_result", "Output", "SfM", "reconstruction"),
                                          os.path.join(outputPath, "merge_result", "Output", "matches"),
                                          os.path.join(outputPath, "merge_result", "Input", "csv"),
                                          os.path.join(outputPath, "merge_result", "Input", "inputImg"),
                                          reconstructParam.WORKSPACE_DIR,
                                          reconstructParam.minReconFrameToAdd)
    
    sfmGraph.mergeModel(os.path.join(outputPath, listVideo[0], "matches", "image_describer.txt"),
                        inputPath,
                        outputPath,
                        reconParam=reconstructParam)
    
if __name__ == '__main__':
    main(sys.argv)
