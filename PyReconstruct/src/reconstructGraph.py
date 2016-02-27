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
# reconstruct multiple video then merge by localizing one video in another
# until all videos are merged into one
#
# Assumptions
# - no same image file name in any project
# - order merge according to sorted video folder name
################################################################################

import argparse
import os
import sys
import yajl as json
import numpy as np
import time
import hulo_sfm.cleanSfM as cleanSfM
import hulo_sfm.sfmMergeGraph as sfmMergeGraph
import hulo_bow.sfmMergeGraphBOW as sfmMergeGraphBOW
import hulo_file.FileUtils as FileUtils
import hulo_file.SfmDataUtils as SfMDataUtils
import hulo_param.ReconstructParam as ReconstructParam
import hulo_bow.ReconstructBOWParam as ReconstructBOWParam

def main():
    # set default parameter
    reconstructParam = ReconstructParam.ReconstructParam
    reconstructBOWParam = ReconstructBOWParam.ReconstructBOWParam
    
    # parse parameters
    description = 'This script is for reconstruct 3D models from multiple videos and merge to one 3D model. ' + \
                'BOW is used for accelerating 3D model merge. ' + \
                'Please prepare multiple videos in Input folder.'
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument('project_path', action='store', nargs=None, const=None, \
                        default=None, type=str, choices=None, metavar=None, \
                        help='Directory path where your 3D model project is stored.')
    parser.add_argument('-k', '--path-camera-file', action='store', nargs='?', const=None, \
                        default=None, type=str, choices=None, metavar=None, \
                        help='File path where camera matrix is stored in Numpy text format. (default: focal length ' + \
                            str(reconstructParam.focalLength) + ' will be used)')    
    parser.add_argument('--bow', action='store_true', default=False, \
                        help='Use BOW to accelerate 3D model merge if this flag is set (default: False)')    
    args = parser.parse_args()
    PROJECT_PATH = args.project_path
    USE_BOW = args.bow
    PATH_CAMERA_FILE = args.path_camera_file
    
    if PATH_CAMERA_FILE:
        if os.path.exists(PATH_CAMERA_FILE):
            with open(PATH_CAMERA_FILE,"r") as camMatFile:
                K = np.loadtxt(camMatFile)
            if K.shape[0]!=3 or K.shape[1]!=3:
                print "Error : invalid camera matrix size = " + str(K)
                sys.exit()
            print "Focal length " + str(K[0][0]) + " is set for reconstruction"
            reconstructParam.focalLength = K[0][0]
        else:
            print "Error : invalid camera matrix file = " + PATH_CAMERA_FILE
            sys.exit()
    
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
            os.system("openMVG_main_SfMInit_ImageListing -i " + sfm_inputImgDir + " -o " + sfm_matchesDir + " -d " + reconstructParam.CAMERA_DATABASE_PATH)
            
            # 1.1 Check intrinsic
            # ( if camera parameter not specified then replace with fixed camera.
            # and set appropriate width and height)
            with open(os.path.join(sfm_matchesDir, "sfm_data.json")) as sfm_data_file:
                sfm_data = json.load(sfm_data_file)
                hImg = sfm_data["views"][0]['value']['ptr_wrapper']['data']["height"]
                wImg = sfm_data["views"][0]['value']['ptr_wrapper']['data']["width"]
                if len(sfm_data["intrinsics"]) == 0:
                    for view in sfm_data["views"]:
                        view["value"]["ptr_wrapper"]["data"]["id_intrinsic"] = 0;
                        
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
                      " " + sfm_matchesDir + \
                      " -mf=" + str(reconstructParam.maxTrackletMatchDistance) + \
                      " -mm=" + str(reconstructParam.minMatchToRetain) + \
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
                os.system(reconstructParam.BUNDLE_ADJUSTMENT_PROJECT_PATH + \
                          " " + os.path.join(sfm_globalDir, "sfm_data.json") + \
                          " " + os.path.join(sfm_globalDir, "sfm_data.json") + \
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
    
    # train bag of words model, and extract bag of words feature for all images
    if USE_BOW and not os.path.isfile(os.path.join(outputPath, "merge_result", "Output", "matches", "BOWfile.yml")):
        outputBowPath = os.path.join(outputPath, "merge_result", "Output", "matches")
        if not os.path.isdir(outputBowPath):
            FileUtils.makedir(outputBowPath)
        print "Execute Training BOW : " + reconstructParam.WORKSPACE_DIR + "/TrainBoW/Release/TrainBoW " + outputPath + " " + \
                  os.path.join(outputBowPath, "BOWfile.yml") + " -p=" + os.path.join(outputBowPath, "PCAfile.yml")
        os.system(reconstructParam.WORKSPACE_DIR + "/TrainBoW/Release/TrainBoW " + outputPath + " " + \
                  os.path.join(outputBowPath, "BOWfile.yml") + " -p=" + os.path.join(outputBowPath, "PCAfile.yml"))
    
    # load graph structure from "mergeGraph.txt" if it exists
    # create new graph structure if it does not exist
    if os.path.isfile(os.path.join(outputPath, "merge_result", "Output", "SfM", "reconstruction", "mergeGraph.txt")):
        if USE_BOW:
            sfmGraph = sfmMergeGraphBOW.sfmGraphBOW.load(os.path.join(outputPath, "merge_result", "Output", "SfM", "reconstruction", "mergeGraph.txt"))
        else:
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
        if USE_BOW:
            sfmGraph = sfmMergeGraphBOW.sfmGraphBOW(inputPath,
                                                    outputPath,
                                                    os.path.join(outputPath, "merge_result", "Input"),
                                                    os.path.join(outputPath, "merge_result", "Output", "SfM", "reconstruction"),
                                                    os.path.join(outputPath, "merge_result", "Output", "matches"),
                                                    os.path.join(outputPath, "merge_result", "Input", "csv"),
                                                    os.path.join(outputPath, "merge_result", "Input", "inputImg"),
                                                    reconstructParam.WORKSPACE_DIR,
                                                    reconstructParam.minReconFrameToAdd)
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
    
    if USE_BOW:
        sfmGraph.mergeModel(os.path.join(outputPath, listVideo[0], "matches", "image_describer.txt"),
                            inputPath,
                            outputPath,
                            reconParam=reconstructParam,
                            reconBOWParam=reconstructBOWParam)
    else:
        sfmGraph.mergeModel(os.path.join(outputPath, listVideo[0], "matches", "image_describer.txt"),
                            inputPath,
                            outputPath,
                            reconParam=reconstructParam)
    
    # select largest 3D model and save it
    SfMDataUtils.saveFinalSfM(PROJECT_PATH)
    
if __name__ == '__main__':
    main()
