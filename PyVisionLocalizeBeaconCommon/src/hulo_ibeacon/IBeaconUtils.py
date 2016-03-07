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
import numpy as np
import matplotlib.pyplot as plt
import time
import os
import json
import scipy.spatial.distance

# parse beacon setting file and extract information, which is
# number of beacon, and beacon map from (major,minor) to index
def parseBeaconSetting(beaconfile):
    
    # read file
    bfile = open(beaconfile,"r")
    lines = bfile.readlines()
    bfile.close()
    
    # generate beacon map
    beaconmap = {}
    ind = 0
    
    for i in range(1,1+int(float(lines[0]))):
        strsplit = lines[i].strip().split(" ")
        major = int(strsplit[0])
        minor = int(strsplit[1])
        beaconmap[(major,minor)] = ind 
        ind = ind + 1
    
    return beaconmap

# parse beacon list with beacon map and return numpy array
# e.g. [3 100 10 -65 100 30 0 120 60 -79] with map {((100,10),1),((100,20),0),((100,30),2)} 
# returns [0 35 0] i.e. rssi in map that is not zero will be sum by 100
def parseBeaconList(line,beaconmap):
    
    rssi = np.zeros(len(beaconmap),dtype=np.float32)
    
    for i in range(0,int(line[0])+1):
        # read value
        major = int(line[3*(i-1)+1])
        minor = int(line[3*(i-1)+2])
        strength = int(line[3*(i-1)+3])
         
        # convert value             
        if strength != 0:
            strength = strength + 100
        
        # add rssi to vector
        if (major,minor) in beaconmap:
            #print beaconmap[(major,minor)]
            rssi[beaconmap[(major,minor)]] = strength             
    
    return rssi

# parse CSV file and extract beacon list and photo list
def parseCSVfile(infilename,beaconmap,approach=0):
    approach = int(approach)
    
    beaconList = {}
    photoList = {}
    
    with open(infilename) as fclrz:
        for line in fclrz:
            line = line.strip()
            splitline = line.split(",")

            timestamp = int(splitline[0])

            # parse by type
            if splitline[1] == "Beacon":
                rssi = parseBeaconList(splitline[6:],beaconmap)
                if approach == 0: # max
                    rssiMax = np.max(rssi)
                    if rssiMax>0:
                        rssi = rssi/rssiMax*100
                elif approach == 1: #median
                    if np.shape(np.where(rssi>0))[1]>0:
                        rssiMed = np.median(rssi[np.where(rssi>0)[0]])
                        if rssiMed > 0:
                            rssi = rssi/rssiMed*100
                beaconList[timestamp] = rssi
            elif splitline[1] == "Misc" and splitline[2] == "Photo":
                photoList[timestamp] = splitline[3]

    return beaconList, photoList

# extract image name to view ID mapping from sfm_data
def getImgNameViewIDMap(sfm_data):
    viewIDMap = {}
    
    for i in range(0,len(sfm_data["views"])):
        filename = sfm_data["views"][i]["value"]["ptr_wrapper"]["data"]["filename"]
        viewID = sfm_data["views"][i]["value"]["ptr_wrapper"]["data"]["id_view"]
        viewIDMap[filename] = viewID
    
    return viewIDMap
    
# remove images not in viewIDMap (sfm_data)
def removeImgNotInSfmData(photoList, viewIDMap):
    
    # remove images not in viewIDMap (sfm_data) from photoList
    for key in photoList.keys():
        if photoList[key] not in viewIDMap:
            del photoList[key]
            
# associate beacon to image
# photoTmp is mapping int (timestamp) -> string (filename)
# beaconTmp is mapping int (timestamp) -> np.array (rssi)
# returns a mapping of int (viewID) ->  {"time" : int , "rssi" : np.array}
def associateBeaconToImg(photoTmp, beaconTmp, viewIDMap):   
    imgBeaconMap = {}
    
    for key in sorted(photoTmp.keys()):
        # get image info
        imgName = photoTmp[key]
        viewID = viewIDMap[imgName]
        
        # get closest RSSI in time
        beaconInd = np.argmin(abs(key-np.array(beaconTmp.keys())))
        rssi = beaconTmp[beaconTmp.keys()[beaconInd]]
        
        # save data
        imgBeaconMap[viewID] = {"time" : key , "rssi" : rssi}

    return imgBeaconMap

# generate consecutive pair list from list of numbers
def genPair(listID,frame):
    pairs = []
    
    for i in range(0,len(listID)):
        for j in range(i+1,min(len(listID),i+frame+1)):
            pairs.append((listID[i],listID[j]))
            
    return pairs

# generate list of pairs of view IDs according to consecutive frames in video
def genVidPair(imgBeaconMap,frameCont):
    
    pairs = []
    
    for vid in imgBeaconMap:
        pairsTmp = genPair(sorted(vid.keys()),frameCont)
        pairs = pairs + pairsTmp

    return pairs

# calculate cooccurence between two beacon numpy vector
def calBeaconCoocur(vec1,vec2):
    return np.sum(np.minimum(vec1,vec2))/np.sum(np.maximum(vec1,vec2))

# calculate distance between two beacon numpy vector
def calBeaconDistance(vec1,vec2):
    denom = np.sum((vec1 > 0) * (vec2 > 0))
    
    if denom < 2:
        return 500
    
    return np.sum((vec1 > 0) * (vec2 > 0) * np.abs(vec1-vec2)) / denom

#generate pair via beacon similarity
def genBeaconSimPair(imgBeaconMap,knn,distance,cooccur):   
        
    pairs = []    
    
    totalImg = 0
    for i in range(0,len(imgBeaconMap)):
        totalImg = totalImg + len(imgBeaconMap[i])
    
    D1 = np.zeros([totalImg,totalImg,3],dtype=np.float32)
    D2 = np.zeros([totalImg,totalImg,3],dtype=np.float32)
    # calculate pairs and append
    for i in range(0,len(imgBeaconMap)-1):
        for j in range(i+1,len(imgBeaconMap)):
            for viewID1 in imgBeaconMap[i]:
                for viewID2 in imgBeaconMap[j]:
                    cooccurTmp = calBeaconCoocur(imgBeaconMap[i][viewID1]["rssi"],imgBeaconMap[j][viewID2]["rssi"])
                    distTmp = calBeaconDistance(imgBeaconMap[i][viewID1]["rssi"],imgBeaconMap[j][viewID2]["rssi"])
                    
                    D1[viewID1,viewID2,:] = cooccurTmp
                    D1[viewID2,viewID1,:] = cooccurTmp
                    D2[viewID1,viewID2,:] = distTmp
                    D2[viewID2,viewID1,:] = distTmp
                    
                    if distTmp <= distance and cooccurTmp >= cooccur:
                        pairs = pairs + [(viewID1,viewID2)]
                        
    if False:
        plt.figure(0)
        plt.imshow(D1)
        plt.figure(1)
        plt.imshow(D2)
        plt.show()
        plt.draw()
        time.sleep(10)
    
    return pairs

# gen pair based on location in sfm_data (1 nearest neighbor)   
# also perform cut off if nesrest neighbor is farther than 1.5*distance to nearest neighbor in same video  
def genNNLocPair(imgBeaconMap,sfm_data):
    pairs = [] 
    
    # gen map between extrinsic ID to extrinsic index
    extMap = {}
    for ext in range(0,len(sfm_data["extrinsics"])):
        extMap[sfm_data["extrinsics"][ext]["key"]] = ext
    
    # in each video
    for i in range(0,len(imgBeaconMap)):
                
        # look thru the frames
        for viewI in imgBeaconMap[i].keys():
            
            # get extrinsic
            # if it exists, find closest point in its video and other videos    
            extrinsicID = sfm_data["views"][viewI]["value"]["ptr_wrapper"]["data"]["id_pose"]
            if extrinsicID in extMap.keys():
                
                locI = np.array(sfm_data["extrinsics"][extMap[extrinsicID]]["value"]["center"])
         
                # find closest distance in video
                distSelf = sys.float_info.max
                for viewI2 in imgBeaconMap[i].keys():
                    
                    if viewI2 == viewI:
                        continue
                    
                    # read thru other framse
                    extrinsicID2 = sfm_data["views"][viewI2]["value"]["ptr_wrapper"]["data"]["id_pose"]
                    if extrinsicID2 in extMap.keys():
                        
                        # read location and calculate distance
                        locI2 = np.array(sfm_data["extrinsics"][extMap[extrinsicID2]]["value"]["center"])
                        distTmp = np.linalg.norm(locI-locI2)
                        
                        if distTmp < distSelf:
                            distSelf = distTmp


                # find closest distance in other videos
                distMin = sys.float_info.max
                minID = -1
                for j in range(0,len(imgBeaconMap)):
        
                    # no match in same video
                    if i == j: 
                        continue 
                    
                    # read thru frames    
                    for viewJ in imgBeaconMap[j].keys():

                        extrinsicIDJ = sfm_data["views"][viewJ]["value"]["ptr_wrapper"]["data"]["id_pose"]
                        if extrinsicIDJ in extMap.keys():
                        
                            # read location and calculate distance
                            locJ = np.array(sfm_data["extrinsics"][extMap[extrinsicIDJ]]["value"]["center"])
                            distTmp = np.linalg.norm(locI-locJ)
                            
                            if distTmp < 3*distSelf:
                                pairs = pairs + [(viewI,viewJ)]
#                             if distTmp < distMin:
#                                 minID = viewJ
#                                 distMin = distTmp
#                 
#                 if distMin < 1.5*distSelf:
#                     pairs = pairs + [(viewI,minID)]
    
    return pairs

# intersect beaconmaps and return list of pair of indices that matches
# the major,minor pair in each map
def intersectBeaconmap(beaconmap1,beaconmap2):
    return [[beaconmap1[pair],beaconmap2[pair]] for pair in beaconmap1 if pair in beaconmap2]


# find number of images in BEACONFILEQUERY that has cooccurence with
# BEACONFILEBASE lower than COOCCUR. 
# MININTERSECT is the minimum number of beacon that must be used in both models
# VALQ and VALB are lists of viewIDs from query and base models whose beacon signals
# will be intersected. Empty means all will be intersected.
def findNumImgByBeaconIntersect(beaconfileQuery,beaconfileBase,cooccur,valQ=[],valB=[],minIntersect=3):

    # read beacon files
    imgBeaconListQ, beaconmapQ = readBeaconData(beaconfileQuery)
    imgBeaconListB, beaconmapB = readBeaconData(beaconfileBase)

    # get matching indices
    indexMatches = intersectBeaconmap(beaconmapQ,beaconmapB)
    
    if len(indexMatches) < minIntersect:
        return 0
    
    indexQ = [x[0] for x in indexMatches]
    indexB = [x[1] for x in indexMatches]

    # reduce index to intersected ones and put into matrix
    sigB = np.zeros((0,len(indexB)),dtype=np.float32)
    for keyB in imgBeaconListB:
        if not valB or keyB in valB:
            sigB = np.vstack((sigB,imgBeaconListB[keyB]["rssi"][indexB]))
    
    sigQ = np.zeros((0,len(indexQ)),dtype=np.float32)
    for keyQ in imgBeaconListQ:
        if not valQ or keyQ in valQ:
            sigQ = np.vstack((sigQ,imgBeaconListQ[keyQ]["rssi"][indexQ]))

    # calculate generalized Jaccard similarity
    countIntSig = 0
    # slower version of generalized Jaccard similarity
#     for i in range(0,sigQ.shape[0]):
#         for j in range(0,sigB.shape[0]):
#             if calBeaconCoocur(sigQ[i,:],sigB[j,:]) > cooccur:
#                 countIntSig = countIntSig + 1
#                 break

    # faster version of generalized Jaccard similarity
    for i in range(0,sigQ.shape[0]):
        if np.any(np.sum(np.minimum(sigQ[i,:],sigB),axis=1)/np.sum(np.maximum(sigQ[i,:],sigB),axis=1)>cooccur):
            countIntSig = countIntSig + 1
    
    # slowest version of generalized Jaccard similarity
#     # check cooccurence and count number of intersecting signals
#     countIntSig = 0
#     for keyQ in imgBeaconListQ:
#         
#         if not valQ or keyQ in valQ: 
#             rssiQ = imgBeaconListQ[keyQ]["rssi"]
#         else:
#             continue
#         
#         for keyB in imgBeaconListB:
#             
#             if not valB or keyB in valB: 
#                 rssiB = imgBeaconListB[keyB]["rssi"]
#             else:
#                 continue
#             
#             if calBeaconCoocur(rssiQ[indexQ],rssiB[indexB]) > cooccur:
#                 countIntSig = countIntSig + 1
#                 break

    return countIntSig

# save ibeacon data, including
# beaconmap:: (major,minor) -> beacon index in rssi vector 
# rssi vector of each image
def exportBeaconData(numView, beaconmap, imgBeaconMap, outfile):
    f = open(outfile,"w")
    
    # write beacon list
    f.write(str(len(beaconmap)) + "\n")
    beaconlist = beaconmap.items()
    beaconlist= sorted(beaconlist, key=lambda x: x[1]) # sort list based on index of rssi vector
    for i in range(0,len(beaconmap)):
        f.write(str(beaconlist[i][0][0])+ " " + str(beaconlist[i][0][1]) + "\n") # major and minor
    
    # write rssi vector
    imgBeaconList = {}
    #print len(imgBeaconMap)
    for i in range(0,len(imgBeaconMap)):
        imgBeaconList.update(imgBeaconMap[i])
        #print len(imgBeaconMap[i])
    #print len(imgBeaconList)
    f.write(str(numView) + "\n")
    for i in sorted(imgBeaconList.keys()):
        f.write(str(i) + " ") # write viewID
        
        # write beacon rssi values
        for j in range(0,len(beaconmap)):    
            f.write(str(imgBeaconList[i]["rssi"][j]) + " ")
        
        f.write("\n")
        
    f.close()
    
# read ibeacon data, including
# beaconmap:: (major,minor) -> beacon index in rssi vector 
# rssi vector of each image
def readBeaconData(infile):
    
    imgBeaconList = {}
    beaconmap = {}
    
    with open(infile,"r") as readfile:
        
        # mode explanation
        # mode == 0 : number of beacons
        # mode == 1 : major and minor
        # mode == 2 : number of beacon signals
        # mode == 3 : beacon signals        
        mode = 0
        
        for line in readfile:
            
            # number of beacon
            if mode == 0:
                
                nBeacon = int(float(line.strip()))
                countLine = 0
                
                mode = 1
                
            # map (major,minor) to index
            elif mode == 1:
                
                line = line.strip()
                line = line.split(" ")
                
                # set major,minor pair and the index
                beaconmap[(int(float(line[0])),int(float(line[1])))] = countLine
                
                countLine = countLine + 1
                
                if countLine == nBeacon:
                    mode = 2
            
            # number of beacon signal    
            elif mode == 2:
                
                nBeaconSignal = int(float(line.strip()))
                countLine = 0
                
                mode = 3
                
            # map viewID to beacon vector    
            elif mode == 3:
                
                line = line.strip()
                line = line.split(" ")
                
                # read viewID
                viewID = int(float(line[0]))
                line.pop(0)
                
                # put each rssi element in list and assign to imgBeaconList
                imgBeaconList[viewID] = {}
                imgBeaconList[viewID]["rssi"] = np.array([int(float(rssi)) for rssi in line],dtype=np.float32)
                
                countLine = countLine + 1
                
                if countLine == nBeaconSignal:
                    mode = 4
    
    return imgBeaconList, beaconmap

def exportBeaconDataForSfmImageFrames(csvdir, sfm_data_file, beacon_file, output, beaconnorm):    
    # parse beacon setting file
    beaconmap = parseBeaconSetting(beacon_file)
    
    # read sfm_data.json and get map between image name and view ID
    with open(sfm_data_file) as fp:
        sfm_data = json.load(fp)
    
    sfm_data.pop("structure") # removed set of 3D points since too memory heavy
    numView = len(sfm_data["views"])
    viewIDMap = getImgNameViewIDMap(sfm_data)
    
    # read each csv file
    imgBeaconMap = [] # list of {int (viewID) :  {"time" : int , "rssi" : np.array}}, one for each video
    for filename in sorted(os.listdir(csvdir)):
        
        # check csv file
        if filename[-3:].lower()=="csv":
            infilename = os.path.join(csvdir, filename)   
        else:
            continue  
        
        # parse CSV file , note that beacon is already normalized
        beaconTmp, photoTmp = parseCSVfile(infilename,beaconmap,beaconnorm)
        
        # remove images not in sfm_data
        removeImgNotInSfmData(photoTmp,viewIDMap)
        
        # associate beacon with each image
        imgBeaconMap.append(associateBeaconToImg(photoTmp,beaconTmp,viewIDMap))
    
    # export
    exportBeaconData(numView, beaconmap, imgBeaconMap, output)