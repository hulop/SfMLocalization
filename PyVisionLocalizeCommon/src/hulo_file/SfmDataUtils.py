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
import json
import numpy as np
import os
import hulo_file.FileUtils as FileUtils

def saveGlobalSfM(inSfmFile, inAmatFile, outSfmFile):
    with open(inSfmFile) as filejson:
        data = json.load(filejson)
    
    with open(inAmatFile,"r") as filemat:
        Amat = np.loadtxt(filemat)
    
    for extrinsics in data["extrinsics"]:
        extrinsics["value"]["center"] = np.dot(Amat,np.concatenate([extrinsics["value"]["center"],[1]])).tolist()
        extrinsics["value"]["rotation"] = np.dot(Amat[:, 0:3],extrinsics["value"]["rotation"]).tolist()
    
    for structure in data["structure"]:
        structure["value"]["X"] = np.dot(Amat,np.concatenate([structure["value"]["X"],[1]])).tolist()
    
    with open(outSfmFile,"w") as filejson:
        json.dump(data, filejson)

#
# Select largest model from "Output" folder, and save to "Output/final"
#
def saveFinalSfM(projectDir):
    # prepare output directory
    finalOutputDir = os.path.join(projectDir,"Output","final")
    if not os.path.isdir(finalOutputDir):
        FileUtils.makedir(finalOutputDir)
    if not os.path.isdir(os.path.join(finalOutputDir,"Input")):
        FileUtils.makedir(os.path.join(finalOutputDir,"Input"))
    if not os.path.isdir(os.path.join(finalOutputDir,"Input","inputImg")):
        FileUtils.makedir(os.path.join(finalOutputDir,"Input","inputImg"))
    if not os.path.isdir(os.path.join(finalOutputDir,"Input","csv")):
        FileUtils.makedir(os.path.join(finalOutputDir,"Input","csv"))        
    if not os.path.isdir(os.path.join(finalOutputDir,"Output")):
        FileUtils.makedir(os.path.join(finalOutputDir,"Output"))
    if not os.path.isdir(os.path.join(finalOutputDir,"Output","matches")):
        FileUtils.makedir(os.path.join(finalOutputDir,"Output","matches"))
    if not os.path.isdir(os.path.join(finalOutputDir,"Output","SfM")):
        FileUtils.makedir(os.path.join(finalOutputDir,"Output","SfM"))
    if not os.path.isdir(os.path.join(finalOutputDir,"Output","SfM","reconstruction")):
        FileUtils.makedir(os.path.join(finalOutputDir,"Output","SfM","reconstruction"))
    if not os.path.isdir(os.path.join(finalOutputDir,"Output","SfM","reconstruction","global")):
        FileUtils.makedir(os.path.join(finalOutputDir,"Output","SfM","reconstruction","global"))

    maxPoseNum = -1
    selectedSfmOutputDir = ''
    # select largest model from "Output/merge_result" at first
    sfmOutputDirs = sorted(os.listdir(os.path.join(projectDir,"Output","merge_result","Output","SfM","reconstruction")))
    for sfmOutputDir in sfmOutputDirs:
        sfmDataFile = os.path.join(projectDir,"Output","merge_result","Output","SfM","reconstruction",sfmOutputDir,"sfm_data.json")
        if not os.path.exists(sfmDataFile):
            continue
        with open(sfmDataFile) as fp:
            sfmData = json.load(fp)
            poseNum = len(sfmData["extrinsics"])
            if (poseNum > maxPoseNum):
                selectedSfmOutputDir = os.path.join(projectDir,"Output","merge_result","Output","SfM","reconstruction",sfmOutputDir)
                maxPoseNum = poseNum
    # select from single 3D model if merged 3D model does not exist
    if not selectedSfmOutputDir:
        outputDirs = sorted(os.listdir(os.path.join(projectDir,"Output")))
        for outputDir in outputDirs:
            outputDirPath = os.path.join(projectDir,"Output",outputDir)
            if not os.path.isdir(outputDirPath):
                continue
            sfmOutputDir = os.path.join(outputDirPath,"SfM","reconstruction","global")
            sfmDataFile = os.path.join(sfmOutputDir,"sfm_data.json")
            if not os.path.exists(sfmDataFile):
                continue
            with open(sfmDataFile) as fp:
                sfmData = json.load(fp)
                poseNum = len(sfmData["extrinsics"])
                if (poseNum > maxPoseNum):
                    selectedSfmOutputDir = sfmOutputDir
                    maxPoseNum = poseNum
        
    # create symbolic links to all images, csv, and descriptor/feature files
    os.system("cp --remove-destination -s " + os.path.join(projectDir,"Input","*","inputImg","*") + " " + os.path.join(finalOutputDir,"Input","inputImg"))
    os.system("cp --remove-destination -s " + os.path.join(projectDir,"Input","*","csv","*") + " " + os.path.join(finalOutputDir,"Input","csv"))
    os.system("cp --remove-destination -s " + os.path.join(projectDir,"Output","*","matches","*.desc") + " " + os.path.join(finalOutputDir,"Output","matches"))
    os.system("cp --remove-destination -s " + os.path.join(projectDir,"Output","*","matches","*.feat") + " " + os.path.join(finalOutputDir,"Output","matches"))
    os.system("cp --remove-destination -s " + os.path.join(projectDir,"Output","*","matches","*.bow") + " " + os.path.join(finalOutputDir,"Output","matches"))
    
    # copy image_describer.txt
    listVideo = sorted(os.listdir(os.path.join(projectDir,"Input")))
    os.system("cp --remove-destination " + os.path.join(projectDir,"Output", listVideo[0], "matches", "image_describer.txt") + " " + os.path.join(finalOutputDir,"Output","matches"))
    
    # copy listbeacon.txt
    os.system("cp --remove-destination " + os.path.join(projectDir,"Input","listbeacon.txt") + " " + os.path.join(finalOutputDir,"Input"))
    
    # copy SfM result
    os.system("cp --remove-destination -s " + os.path.join(selectedSfmOutputDir,"sfm_data.json") + " " + os.path.join(finalOutputDir,"Output","SfM","reconstruction","global"))
    os.system("cp --remove-destination -s " + os.path.join(selectedSfmOutputDir,"colorized.ply") + " " + os.path.join(finalOutputDir,"Output","SfM","reconstruction","global"))    
    
    # copy PCAfile.yml and BOWfile.yml if exists
    if os.path.exists(os.path.join(projectDir,"Output","merge_result","Output","matches","PCAfile.yml")):
        os.system("cp --remove-destination " + os.path.join(projectDir,"Output","merge_result","Output","matches","PCAfile.yml") + " " + os.path.join(finalOutputDir,"Output","matches"))
    if os.path.exists(os.path.join(projectDir,"Output","merge_result","Output","matches","BOWfile.yml")):
        os.system("cp --remove-destination " + os.path.join(projectDir,"Output","merge_result","Output","matches","BOWfile.yml") + " " + os.path.join(finalOutputDir,"Output","matches"))
    
    # To create same directory structure before merging, create sfm_data.json without structure information in matches directory
    with open(os.path.join(os.path.join(selectedSfmOutputDir,"sfm_data.json"))) as fpr:
        sfmData = json.load(fpr)
        sfmData["extrinsics"] = []
        sfmData["control_points"] = []
        sfmData["structure"] = []
        with open(os.path.join(finalOutputDir,"Output","matches","sfm_data.json"),"w") as fpw:
            json.dump(sfmData, fpw)
    
    # copy beacon.txt if exists
    if os.path.exists(os.path.join(selectedSfmOutputDir,"beacon.txt")):
        os.system("cp --remove-destination " + os.path.join(selectedSfmOutputDir,"beacon.txt") + " " + os.path.join(finalOutputDir,"Output","SfM","reconstruction","global"))
