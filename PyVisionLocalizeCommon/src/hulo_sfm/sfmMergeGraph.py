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

import os
import json
import numpy as np
import scipy.sparse.csgraph
import pickle
import sys
import hulo_file.FileUtils as FileUtils
import hulo_param.ReconstructParam as ReconstructParam
import hulo_sfm.mergeSfM as mergeSfM

class sfmModel:
    
    def __init__(self, name, imgFolLoc, csvFolLoc, matchesFolLoc, locFolLoc, sfm_dataLoc, 
                 validMergeRansacThres=-1, validMergeRansacThresK=-1, ransacStructureThres=-1, ransacStructureThresK=-1,
                 mergeStructureThres=-1, mergeStructureThresK=-1):
        if (validMergeRansacThres==-1 and validMergeRansacThresK==-1):
            print "error : invalid argument for sfmModel valid merge ransac"
            sys.exit()
        if (ransacStructureThres==-1 and ransacStructureThresK==-1):
            print "error : invalid argument for sfmModel structure ransac"
            sys.exit()
        if (mergeStructureThres==-1 and mergeStructureThresK==-1):
            print "error : invalid argument for sfmModel structure merge"
            sys.exit()
        
        self.name = name # folder name
        self.mergeOrder = name # structure similar to a tree specifying merge order
        self.imgFolLoc = imgFolLoc # folder dir of input image folder
        self.csvFolLoc = csvFolLoc # folder dir of csv folder
        self.matchesFolLoc = matchesFolLoc # folder of match folder with descriptor
        self.locFolLoc = locFolLoc # folder of localization result
        self.sfm_dataLoc = sfm_dataLoc # file dir of sfm_data.json
                
        # get list of reconstructed frames
        if self.sfm_dataLoc != "":
            sfm_data = FileUtils.loadjson(self.sfm_dataLoc)
            extKeyTmp = [x["key"] for x in sfm_data["extrinsics"]]
            self.reconFrame = [x["value"]["ptr_wrapper"]["data"]["id_view"] for x in sfm_data["views"] if x["value"]["ptr_wrapper"]["data"]["id_pose"] in extKeyTmp]
            if validMergeRansacThresK>0:
                self.validMergeRansacThres = mergeSfM.findMedianThres(sfm_data, validMergeRansacThresK)
            else:
                self.validMergeRansacThres = validMergeRansacThres
            
            if ransacStructureThresK>0:
                self.ransacStructureThres = mergeSfM.findMedianStructurePointsThres(sfm_data, ransacStructureThresK)
            else:
                self.ransacStructureThres = ransacStructureThres

            if mergeStructureThresK>0:
                self.mergeStructureThres = mergeSfM.findMedianStructurePointsThres(sfm_data, mergeStructureThresK)
            else:
                self.mergeStructureThres = mergeStructureThres
            
    # update information in self with information from newInfo
    def update(self, newInfo):
        self.name = newInfo.name
        self.imgFolLoc = newInfo.imgFolLoc
        self.csvFolLoc = newInfo.csvFolLoc
        self.matchesFolLoc = newInfo.matchesFolLoc
        self.locFolLoc = newInfo.locFolLoc
        self.sfm_dataLoc = newInfo.sfm_dataLoc
        self.reconFrame = newInfo.reconFrame
        self.mergeOrder = newInfo.mergeOrder
        self.validMergeRansacThres = newInfo.validMergeRansacThres
        self.ransacStructureThres = newInfo.ransacStructureThres
        self.mergeStructureThres = newInfo.mergeStructureThres
        
class sfmGraph:
        
    # receive a path
    # if path is Input folder, list all folders as project
    # if path is log file, load previous data
    def __init__(self, inputPath, outputPath, mInputPath, mSfMPath, mMatchesPath, mCsvPath, mInputImgPath, 
                 workspacePath, validMergeRansacThresK=5, ransacStructureThresK=10,
                 mergeStructureThresK=0.01, minReconFrame=25):
        self.sfmModel = [] # list of sfmModel objects for merging
        self.mSfMPath = mSfMPath # sfm path containing multiple folder of merged models
        self.mMatchesPath = mMatchesPath # matches path
        self.mCsvPath = mCsvPath # csv path
        self.mInputImgPath = mInputImgPath # input images path
        self.mInputPath = mInputPath
        self.nMergedModel = 0 # number of merged performed
        self.badMatches = [] # keeps pair of bad matches
        self.workspacePath = workspacePath
        
        FileUtils.makedir(self.mSfMPath)
        FileUtils.makedir(self.mMatchesPath)
        FileUtils.makedir(self.mCsvPath)
        FileUtils.makedir(self.mInputImgPath)
        FileUtils.makedir(self.mInputPath)
        
        # list all folder as projects
        if os.path.isdir(inputPath):
            
            listDir = os.listdir(inputPath)
            
            for folder in sorted(listDir):
                
                # add model to self
                self.addModel(folder, os.path.join(inputPath,folder), os.path.join(outputPath,folder),
                        minReconFrame, validMergeRansacThresK, ransacStructureThresK, mergeStructureThresK)
    
    # add model to merge graph
    # note that the ordering of the folders in input and 
    # output paths must be in correct format
    # 
    # Input
    # videoName : name of video
    # inputPath : path to input folder (with inputImg and csv folders)
    # outputPath ; path to output folder (with matches and SfM folders)
    # minimumFrame : minimum number of frames used in reconstruction to be used for include
    #
    # Output
    # added : boolean whether the video is added
    def addModel(self,videoName,inputPath,outputPath,minimumFrame,
                 validMergeRansacThresK,ransacStructureThresK,mergeStructureThresK):
        
        # check whether all folders and files exists
        if (not os.path.isdir(inputPath)) or \
            (not os.path.isdir(os.path.join(inputPath,"inputImg"))) or \
            (not os.path.isdir(outputPath)) or \
            (not os.path.isdir(os.path.join(outputPath,"matches"))) or \
            (not os.path.isdir(os.path.join(outputPath,"SfM"))) or \
            (not os.path.isdir(os.path.join(outputPath,"SfM","reconstruction"))) or \
            (not os.path.isdir(os.path.join(outputPath,"SfM","reconstruction","global"))) or \
            (not os.path.isfile(os.path.join(outputPath,"SfM","reconstruction","global","sfm_data.json"))):
            
            print videoName + " is not a complete SfM project and will be ignored."
            return False
        
        # check if there is a video with the same name    
        elif videoName in [x.name for x in self.sfmModel]:
            print "There exists other video with name \"" + videoName + "\", thus this video will be ignored."
            return False
        
        # generate sfmModel object
        newModel = sfmModel(
            videoName,
            os.path.join(inputPath,"inputImg"),
            os.path.join(inputPath,"csv"),
            os.path.join(outputPath,"matches"),
            os.path.join(outputPath,"loc"),
            os.path.join(outputPath,"SfM","reconstruction","global","sfm_data.json"),
            validMergeRansacThresK=validMergeRansacThresK,
            ransacStructureThresK=ransacStructureThresK,
            mergeStructureThresK=mergeStructureThresK)

        # check number of frame is above minimum
        if len(newModel.reconFrame) < minimumFrame:
            print "# of reconstructed frames (" + str(len(newModel.reconFrame)) +") is lower than threshold (" + \
                str(minimumFrame) + "), hence model " + newModel.name + " will not be included." 
            return False
        
        print "Including " + videoName + " into merge order."
        self.sfmModel.append(newModel)
        
        return True
    
    # calculate graph between all pairs of model
    def calcGraph(self):
        
        print "Calculating graph edges between videos"
        nModel = len(self.sfmModel)
        graphEdges = np.zeros((nModel,nModel),dtype=np.float32)
        
        for i in range(0,nModel-1):
            for j in range(i+1,nModel):   
                graphEdges[i,j] = 1.0
                graphEdges[j,i] = 1.0                
        
        print "Complete calculating graph edges between videos"
        return graphEdges
    
    # save result to file
    def save(self, outputPath):
        with open(outputPath,"wb") as output:
            pickle.dump(self, output, pickle.HIGHEST_PROTOCOL)
    
    # load saved file
    @staticmethod
    def load(inputPath):
        with open(inputPath,"rb") as inputFile:
            data = pickle.load(inputFile)
        return data   
    
    # merge one sfmModel to other (specifically, model 2 to model 1)
    # all required folder will be created
    # returns whether the merge is success, and merged sfmModel    
    def mergeOneModel(self, model1, model2, reconParam):
        
        sfmOutPath = os.path.join(self.mSfMPath,"global"+str(self.nMergedModel))
        
        # modified by T. IShihara 2016.06.14
        # fix file name too long issue
        # 
        # create a temporary folder for reconstructed image of model2
        #inputImgTmpFolder = os.path.join(self.mSfMPath,"inputImgTmp","inputImgTmp"+model2.name)        
        inputImgTmpFolder = os.path.join(self.mSfMPath,"inputImgTmp","inputImgTmpModel2")
        if os.path.isdir(inputImgTmpFolder):
            FileUtils.removedir(inputImgTmpFolder)
        
        # copy reconstructed image fom model2 to tmp folder
        sfm_data2 = FileUtils.loadjson(model2.sfm_dataLoc)
        if not os.path.isdir(inputImgTmpFolder):
            listReconFrameName = [sfm_data2["views"][x]["value"]["ptr_wrapper"]["data"]["filename"] for x in range(0,len(sfm_data2["views"])) if sfm_data2["views"][x]["value"]["ptr_wrapper"]["data"]["id_view"] in model2.reconFrame]
            FileUtils.makedir(inputImgTmpFolder)
            for reconFrameName in listReconFrameName:
                os.system("cp -s " + os.path.join(model2.imgFolLoc,reconFrameName) + " " + inputImgTmpFolder)
        
        
        # remove all old localization result
        FileUtils.removedir(model2.locFolLoc) 
        FileUtils.makedir(model2.locFolLoc)

        # localize the images from model2 on model1
        os.system(reconParam.LOCALIZE_PROJECT_PATH + \
                  " " + inputImgTmpFolder + \
                  " " + os.path.dirname(model1.sfm_dataLoc) + \
                  " " + self.mMatchesPath + \
                  " " + model2.locFolLoc + \
                  " -f=" + str(reconParam.locFeatDistRatio) + \
                  " -r=" + str(reconParam.locRansacRound) + \
                  " -e=" + model2.csvFolLoc + \
                  " -i=" + str(reconParam.locSkipFrame))
                  
        # remove temporary image folder
        # removedir(inputImgTmpFolder)
        
        # extract centers from all json file and write to a file
        fileLoc = open(os.path.join(model2.locFolLoc,"center.txt"),"w")
        countLocFrame = 0
        for filename in sorted(os.listdir(model2.locFolLoc)):
            if filename[-4:]!="json":
                continue
            
            countLocFrame = countLocFrame + 1
            with open(os.path.join(model2.locFolLoc,filename)) as locJson:
                #print os.path.join(sfm_locOut,filename)
                locJsonDict = json.load(locJson)
                loc = locJsonDict["t"]
                fileLoc.write(str(loc[0]) + " "  + str(loc[1]) + " "  +str(loc[2]) + " 255 0 0\n" )   
        fileLoc.close() 
        
        # get inlier matches
        FileUtils.makedir(sfmOutPath)
        resultSfMDataFile = os.path.join(sfmOutPath,"sfm_data.json")
        # below also checks if the ratio between first and last svd of M[0:3,0:3] 
        # is good or not. If not then reject
        # TODO : revisit ransacRound parameter, use number of reconstruction frame to determine structure points transform seems small
        nMatchPointsTmp, nInlierTmp, M = mergeSfM.mergeModel(model1.sfm_dataLoc,
                            model2.sfm_dataLoc,
                            model2.locFolLoc,
                            resultSfMDataFile,
                            ransacThres=model1.ransacStructureThres,
                            mergePointThres=model1.mergeStructureThres,
                            ransacRoundMul=reconParam.ransacRoundMul,
                            inputImgDir=self.mInputImgPath,
                            minLimit=reconParam.min3DnInliers)
        
        ratioInlierMatchPoints = 0.0
        if nMatchPointsTmp>0:
            ratioInlierMatchPoints = float(nInlierTmp)/nMatchPointsTmp
        
        # 3. perform test whether merge is good
        sfm_merge_generated = True
        countFileAgree = 0
        countFileLoc = 1
        if os.path.isfile(resultSfMDataFile):
            os.system(reconParam.BUNDLE_ADJUSTMENT_PROJECT_PATH + " " + resultSfMDataFile + " " + resultSfMDataFile)
            countFileLoc, countFileAgree = mergeSfM.modelMergeCheckLocal(resultSfMDataFile, model2.locFolLoc, model1.validMergeRansacThres)
        else:
            sfm_merge_generated = False
        
        ratioAgreeFrameReconFrame = 0.0
        if (len(model2.reconFrame)>0):
            ratioAgreeFrameReconFrame = float(countFileAgree)/len(model2.reconFrame)
        ratioAgreeFrameLocFrame = 0.0
        if (countFileLoc>0):
            ratioAgreeFrameLocFrame = float(countFileAgree)/countFileLoc
        
        # write log file
        with open(os.path.join(self.mSfMPath,"global"+str(self.nMergedModel),"log.txt"),"a") as filelog:
            filelog.write(("M1: " + model1.name + "\n" + \
                          "M2: " + model2.name + "\n" + \
                          "nMatchedPoints: " + str(nMatchPointsTmp) + "\n" + \
                          "nInliers: " + str(nInlierTmp) + "\n" + \
                          "ratioInlierWithMatchedPoints: " + str(ratioInlierMatchPoints) + "\n" + \
                          "countLocFrame: " + str(countLocFrame) + "\n" + \
                          "nReconFrame M2: " + str(len(model2.reconFrame)) + "\n" + \
                          "countFileAgree: " + str(countFileAgree) + "\n" + \
                          "countFileLoc: " + str(countFileLoc) + "\n" + \
                          "not sfm_merge_generated: " + str(not sfm_merge_generated) + "\n" + \
                          # obsolete condition by T. Ishihara 2015.11.10
                          #"nInlierTmp > "+str(reconParam.vldMergeRatioInliersFileagree)+"*countFileAgree: " + str(nInlierTmp > reconParam.vldMergeRatioInliersFileagree*countFileAgree) + "\n" + \
                          "countFileAgree > "+str(reconParam.vldMergeMinCountFileAgree)+": " + str(countFileAgree > reconParam.vldMergeMinCountFileAgree) + "\n" + \
                          # obsolete condition by T. Ishihara 2016.04.02
                          #"countFileAgree > "+str(reconParam.vldMergeSmallMinCountFileAgree)+": " + str(countFileAgree > reconParam.vldMergeSmallMinCountFileAgree) + "\n" + \
                          # obsolete condition by T. Ishihara 2016.04.02                          
                          #"countFileLoc < countFileAgree*" +str(reconParam.vldMergeShortRatio)+ ": " + str(countFileLoc < countFileAgree*reconParam.vldMergeShortRatio) + "\n" + \
                          "ratioLocAgreeWithReconFrame: " + str(ratioAgreeFrameReconFrame) + "\n" + \
                          "ratioLocAgreeWithReconFrame > " + str(reconParam.vldMergeRatioAgrFReconF) + ": " + str(ratioAgreeFrameReconFrame > reconParam.vldMergeRatioAgrFReconF) + "\n" + \
                          "ratioLocAgreeWithLocFrame: " + str(ratioAgreeFrameLocFrame) + "\n" + \
                          "ratioLocAgreeWithLocFrame > " + str(reconParam.vldMergeRatioAgrFLocF) + ": " + str(ratioAgreeFrameLocFrame > reconParam.vldMergeRatioAgrFLocF) + "\n" + \
                          str(M) + "\n\n"))
       
        # rename the localization folder to save localization result
        '''
        if os.path.isdir(model2.locFolLoc+model1.name):
            FileUtils.removedir(model2.locFolLoc+model1.name)
        os.rename(model2.locFolLoc,model2.locFolLoc+model1.name)
        '''
        
        # obsolete merge condition
        '''
        if not sfm_merge_generated or \
            not (nInlierTmp > reconParam.vldMergeRatioInliersFileagree*countFileAgree and \
            ((countFileAgree > reconParam.vldMergeMinCountFileAgree or (countFileAgree > reconParam.vldMergeSmallMinCountFileAgree and countFileLoc < countFileAgree*reconParam.vldMergeShortRatio)) and \
            ((nInlierTmp > reconParam.vldMergeNInliers and float(countFileAgree)/len(model2.reconFrame) > reconParam.vldMergeRatioAgrFReconFNInliers) or float(countFileAgree)/countFileLoc > reconParam.vldMergeRatioAgrFLocF) and
            (float(countFileAgree)/len(model2.reconFrame) > reconParam.vldMergeRatioAgrFReconF))):
        '''
        # update merge condition by T. Ishihara 2015.11.10
        '''
        if not sfm_merge_generated or \
            not (countFileAgree > reconParam.vldMergeMinCountFileAgree and \
                 countFileAgree > reconParam.vldMergeSmallMinCountFileAgree and \
                 countFileLoc < countFileAgree*reconParam.vldMergeShortRatio and \
                 ((nInlierTmp > reconParam.vldMergeNInliers and ratioAgreeFrameReconFrame > reconParam.vldMergeRatioAgrFReconFNInliers) or \
                    ratioAgreeFrameReconFrame > reconParam.vldMergeRatioAgrFReconF) and \
                 ratioAgreeFrameLocFrame > reconParam.vldMergeRatioAgrFLocF):
        '''
        # update merge condition by T. Ishihara 2016.04.02
        '''
        if not sfm_merge_generated or \
            not (countFileAgree > reconParam.vldMergeMinCountFileAgree and \
                 ((nInlierTmp > reconParam.vldMergeNInliers and ratioAgreeFrameReconFrame > reconParam.vldMergeRatioAgrFReconFNInliers) or \
                    ratioAgreeFrameReconFrame > reconParam.vldMergeRatioAgrFReconF) and \
                 ratioAgreeFrameLocFrame > reconParam.vldMergeRatioAgrFLocF):
        '''
        # update merge condition by T. Ishihara 2016.06.09
        '''
        if not sfm_merge_generated or \
            not (countFileAgree > reconParam.vldMergeMinCountFileAgree and \
                 ratioAgreeFrameLocFrame > reconParam.vldMergeRatioAgrFLocF and \
                 nInlierTmp > reconParam.min3DnInliers and \
                 ratioInlierMatchPoints > reconParam.vldMergeRatioInliersMatchPoints):
        '''
        # update merge condition by T. Ishihara 2016.06.20
        if not sfm_merge_generated or \
            not (countFileAgree > reconParam.vldMergeMinCountFileAgree and \
                 ratioAgreeFrameLocFrame > reconParam.vldMergeRatioAgrFLocF and \
                 nInlierTmp > reconParam.min3DnInliers):        
            print "Transformed locations do not agree with localization. Skip merge between " + model1.name + " and " + model2.name + "."
        
            '''
            if os.path.isfile(os.path.join(sfmOutPath,"sfm_data.json")):
                os.rename(os.path.join(sfmOutPath,"sfm_data.json"), \
                          os.path.join(sfmOutPath,"sfm_data_("+model1.name + "," + model2.name+").json"))
            '''
            
            # move to next video
            return False, sfmModel("","","","","","",validMergeRansacThres=0,validMergeRansacThresK=0,
                                   ransacStructureThres=0, ransacStructureThresK=0, 
                                   mergeStructureThres=0, mergeStructureThresK=0)
            
        # generate colorized before bundle adjustment for comparison
        os.system("openMVG_main_ComputeSfM_DataColor " +
            " -i " + os.path.join(sfmOutPath,"sfm_data.json") +
            " -o " + os.path.join(sfmOutPath,"colorized_pre.ply"))        
        
        # TODO : try computing structure from know pose here
        # https://github.com/openMVG/openMVG/issues/246
        # http://openmvg.readthedocs.io/en/latest/software/SfM/ComputeStructureFromKnownPoses/
        
        # TODO : revisit the order of bundle adjustment
        # perform bundle adjustment
        # modified by T.Ishihara 2016.04.08
        # fix only translation at first
        '''
        os.system(reconParam.BUNDLE_ADJUSTMENT_PROJECT_PATH + " " + os.path.join(sfmOutPath,"sfm_data.json") + " " + os.path.join(sfmOutPath,"sfm_data.json") + \
                  " -c=" + "rs,rst,rsti" + " -r=" + "1")
        '''
        os.system(reconParam.BUNDLE_ADJUSTMENT_PROJECT_PATH + " " + os.path.join(sfmOutPath,"sfm_data.json") + " " + os.path.join(sfmOutPath,"sfm_data.json") + \
                  " -c=" + "rst,rsti" + " -r=" + "1")
        
        os.system("openMVG_main_ComputeSfM_DataColor " +
            " -i " + os.path.join(sfmOutPath,"sfm_data.json") +
            " -o " + os.path.join(sfmOutPath,"colorized.ply"))
        
        return True, sfmModel("A" + model1.name + "," + model2.name +"Z", self.mInputImgPath, self.mCsvPath, 
                              self.mMatchesPath, os.path.join(sfmOutPath,"loc"), resultSfMDataFile, 
                              validMergeRansacThres=model1.validMergeRansacThres,
                              ransacStructureThres=model1.ransacStructureThres, 
                              mergeStructureThres=model1.mergeStructureThres)
            
    # check if the pair video1, video2 is a bad match, i.e. a localization 
    # between these videos failed before, so can skip without redoing localization
    def isBadMatch(self,video1,video2):
        if [video1.name,video2.name] in self.badMatches or [video2.name,video1.name] in self.badMatches:
            return True
        return False
    
    # clear history of bad matches
    def clearBadMatches(self):
        self.badMatches = []
    
    # perform merging model
    # Input
    # image_descFile : path to image_describer.txt
    def mergeModel(self, image_descFile, inputPath, outputPath, reconParam=ReconstructParam):        
        print "Begin merging models"
        
        FileUtils.makedir(self.mInputImgPath)
        FileUtils.makedir(self.mCsvPath)
        FileUtils.makedir(self.mMatchesPath)
        FileUtils.makedir(self.mSfMPath)
        
        # create symbolic links to all images, csv, and descriptor/feature files
        os.system("cp --remove-destination -s " + os.path.join(inputPath,"*","inputImg","*") + " " + self.mInputImgPath)
        os.system("cp --remove-destination -s " + os.path.join(inputPath,"*","csv","*") + " " + self.mCsvPath)
        os.system("cp --remove-destination -s " + os.path.join(outputPath,"*","matches","*.desc") + " " + self.mMatchesPath)
        os.system("cp --remove-destination -s " + os.path.join(outputPath,"*","matches","*.feat") + " " + self.mMatchesPath)
        os.system("cp --remove-destination -s " + os.path.join(outputPath,"*","matches","*.bow") + " " + self.mMatchesPath)
        
        # copy image_describer.txt
        os.system("cp --remove-destination " + image_descFile + " " + self.mMatchesPath)
         
        listLead = range(0,len(self.sfmModel)) # list of model indexes which can initiate merge (list of model indexes which did not fail merge yet)
        listBye = [] # list of model indexes which will not be used to initiate merge (list of model indexes which already failed merge)
        baseVideo = -1
        mergeCandidatesRemainsForBaseVideo = True
        calcGraphEdges = False
        
        while True:
            # update model indexes which are not used to initiate merge
            if not mergeCandidatesRemainsForBaseVideo:
                listBye.append(self.sfmModel[baseVideo].name)
            
            listName = [(x,self.sfmModel[x].name) for x in range(0,len(self.sfmModel))]
            listLead = [x[0] for x in listName if x[1] not in listBye]
            
            # if there was a merge, recalculate the cooccurence graph
            if mergeCandidatesRemainsForBaseVideo:
                # calculate cooccurence graph
                if not calcGraphEdges:
                    graphEdges = self.calcGraph()
                    calcGraphEdges = True
                    
                print "graph edges : " + str(graphEdges)
                print "SfM model names : " + str([x.name for x in self.sfmModel])
                connectionGraph = (graphEdges > 0.0)
                
                # calculate connected component on graph
                ccLabel = scipy.sparse.csgraph.connected_components(
                    connectionGraph,
                    directed=False)[1]
                        
            # if nore more mergable components
            if len(np.unique(ccLabel)) == len(ccLabel):
                print "No more mergable components. Exiting."
                return
            
            # sort the length of reconstructed frames in each video 
            # from small to large to find the base Video
            reconFrameLenList = [len(self.sfmModel[i].reconFrame) for i in range(0,len(self.sfmModel))]
            reconFrameLenIdx = [x[0] for x in sorted(enumerate(reconFrameLenList), key=lambda y:y[1])]

            # find first base video that has a connected component
            baseVideo = ""
            for video in reconFrameLenIdx:
                if np.sum(ccLabel==ccLabel[video]) > 1 and video in listLead:
                    baseVideo = video
                    break
                
            # this should never be called since program should exit 
            # if there is no connected components in grap 
            if baseVideo == "":
                print "Cannot find connected component to merge. Exiting."
                return

            # get videos that connect to this baseVideo
            # and sort the from smallest to largest as merge order
            neighborVec = np.where(connectionGraph[baseVideo,:])[0]
            neighborVec = neighborVec[neighborVec!=baseVideo] # prevent selecting itself to merge
            mergeCandidate = neighborVec.tolist()
            nReconFrameMergeCand = [len(self.sfmModel[x].reconFrame) for x in mergeCandidate]
            orderMergeCand = [x[0] for x in sorted(enumerate(nReconFrameMergeCand), key=lambda y:y[1])]
            mergeCandidateModel = [self.sfmModel[mergeCandidate[i]] for i in orderMergeCand]

            mergedModel = self.sfmModel[baseVideo]
            
            print "Based model: " + mergedModel.name
            print "To merge with: " + str([x.name for x in mergeCandidateModel])
            mergeCandidatesRemainsForBaseVideo = False            
            for video in mergeCandidateModel:
                
                # check if failed localization has been performed on this pair before
                # if so, skip this localization
                if self.isBadMatch(video,mergedModel):
                    continue
                
                # swap order so small model is merged to larger model
                swap = False
                if len(mergedModel.reconFrame) < len(video.reconFrame):
                    tmp = mergedModel
                    mergedModel = video
                    video = tmp
                    swap = True
                
                # attempt merge
                mergeResult, mergedModelTmp = self.mergeOneModel(mergedModel,video,reconParam)
                
                if mergeResult:
                    mergedModel.update(mergedModelTmp)
                    videoIdx = self.sfmModel.index(video)
                    del self.sfmModel[videoIdx]
                    
                    # update graph
                    graphEdges = np.delete(graphEdges,videoIdx,0)
                    graphEdges = np.delete(graphEdges,videoIdx,1)
                                        
                    self.nMergedModel = self.nMergedModel+1
                    self.save(os.path.join(self.mSfMPath,"global" + str(self.nMergedModel-1),"mergeGraph.txt"))
                    self.save(os.path.join(self.mSfMPath,"mergeGraph.txt"))
                    mergeCandidatesRemainsForBaseVideo = True
                    
                    # reset listBye to allow small model to merge to new large model
                    listBye = []
                    
                    # write result log file
                    with open(os.path.join(self.mSfMPath,"logRecon.txt"),"a") as outLogFile:
                        outLogFile.write(str(self.nMergedModel-1) + " " + mergedModel.name + "\n")
                    
                    # start again
                    break
                else:
                    # add to bad matches
                    self.badMatches.append([video.name,mergedModel.name])
                
                    # save
                    self.save(os.path.join(self.mSfMPath,"mergeGraph.txt"))
                
                    if swap:
                        # swap back if not merged
                        mergedModel = video
