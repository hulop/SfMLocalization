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
# Merge reconstruction between 2 models (called model A and model B). Model B will 
# be merged into model A.
#
# The steps are:
# 1. Localize each image of model B in model A
# 2. Find the matches between 3D points in model B and model A
#  2.1 if no dispute between matches, then consider it the same point
#  2.2 if there is dispute i.e. a 3D point in model B matches to many 3D points 
#      in model A, then consider them as different 3D POINT
# 3. Take the matched 3D points and find affine transformation 
#    that transform model B into model A's coordinate
# 4. Transform model B and merge the data
# 5. Recalculate R and t for all models and perform bundle Adjustment
################################################################################

import os
import sys
import numpy as np
from scipy.spatial import cKDTree
from fileinput import filename
import random
import hulo_file.FileUtils as FileUtils

# Read localization output json file to extract matches for each image in model B.
# Require folder of location as output
# Return lists of image names and match pairs between 2D (model B image) and 3D (model A structure)
def readMatch(locFolder):
    
    imgname = []
    matchlist = []
    
    print "Reading loc output: " + locFolder
    for filename in sorted(os.listdir(locFolder)):
        
        if filename[-4:] != "json":
            continue
        
        jsondata = FileUtils.loadjson(os.path.join(locFolder, filename))
        imgname.append(os.path.basename(jsondata["filename"]))
        matchlist.append(jsondata["pair"])
        
    return imgname, matchlist

# from list of image name return list of view IDs
def imgnameToViewID(imgname, sfm_data):
    
    viewID = []
    
    # preallocate list memory
    for i in range(0, len(imgname)):
        viewID.append(-1)
        
    # go thru list of views
    for i in range(0, len(sfm_data["views"])):
        sfm_imgname = sfm_data["views"][i]["value"]["ptr_wrapper"]["data"]["filename"]
        
        # compare image name and put in correct position
        if sfm_imgname in imgname:
            viewID[imgname.index(sfm_imgname)] = sfm_data["views"][i]["value"]["ptr_wrapper"]["data"]["id_view"]
      
    return viewID

# create a dictionary that return 3D id point
# given viewID and feature ID as input
def getViewFeatTo3DMap(sfm_data):
    viewFeatMap = {}
    
    structure = sfm_data["structure"]
    
    # go thru list of 3D points
    for i in range(0, len(structure)):
        obsKey = structure[i]["key"]
        obs = structure[i]["value"]["observations"]

        # go thru list of view having that 3D point
        for j in range(0, len(obs)):

            # save the viewID, featID map to 3D ID
            if obs[j]["key"] not in viewFeatMap:
                viewFeatMap[obs[j]["key"]] = {}
                
            viewFeatMap[obs[j]["key"]][obs[j]["value"]["id_feat"]] = obsKey
            
    return viewFeatMap

# get list of consistent matches between two 3D models
# that is no 3D point in one model matches to more than one 3D point
# in another model
#
# Input
# viewID : list of view ID 
# matchList : list of list of matches between 2D feature ID of each view in model B
#          to 3D point ID of model A (e.g. if matchList[1][3] is [2,45], this means
#          feature 2 of image with view ID viewID[1] of model B matches to 3D point with 
#          ID 45 in model A. The number 3 is just the third matches of this view) 
#         (!!Note that length of viewID and matchList must be the same!!) 
# viewFeatMap : a dictionary of dictionary, mapping viewID and feaID to 3D point ID 
#          in model B
#
# Output
# match3D : list of match pair between 3D pt ID of model B and 3D pt ID of model A 
def getConsistent3DMatch(viewID, matchList, viewFeatMap):
    
    # chech length equal
    if(len(viewID) != len(matchList)):
        sys.exit("lengths of viewID and matchList are not the same")
    
    # dictionary that map 3D pt ID of model B to 3D pt ID of model A
    matchMap = {}
    
    for i in range(0, len(matchList)):
        for pair in matchList[i]:
            
            if viewID[i] in viewFeatMap:
                if pair[0] in viewFeatMap[viewID[i]]:
                    
                    # check consistency
                    
                    # if new pair, add to matchMap
                    if viewFeatMap[viewID[i]][pair[0]] not in matchMap:
                        matchMap[viewFeatMap[viewID[i]][pair[0]]] = pair[1]
                        
                    # if already match exists but to different 3D point, set
                    # match to -1 to signify inconsistent and prevent future match
                    # if match exists but to the same match then do nothing
                    elif matchMap[viewFeatMap[viewID[i]][pair[0]]] != pair[1]:
                        matchMap[viewFeatMap[viewID[i]][pair[0]]] = -1
    
    # extract matches that are consistent (i.e. not -1)
    match3D = []
    for key in matchMap.keys(): 
        if matchMap[key] != -1:
            match3D.append([key, matchMap[key]])
            
    # find list of duplicate second element of pair
    sec = [y[1] for y in match3D]
    dup = set([y for y in sec if sec.count(y) > 1])
    
    # remove element with duplicate second element
    match3D = [x for x in match3D if x[1] not in dup]
            
    return match3D
    
# get 3D location given sfm_data and 3D pt ID
def get3DPointloc(sfm_data, listID):
    list3D = []
    
    # generate map from 3D pt ID to structure index
    mapID = {}
    for j in range(0, len(sfm_data["structure"])):
        mapID[sfm_data["structure"][j]["key"]] = j
    
    for id3D in listID:
        if id3D in mapID:
            list3D.append(sfm_data["structure"][mapID[id3D]]["value"]["X"])
        else:
            list3D.append([float("inf"),float("inf"),float("inf")])
        
    return list3D

# get all 3D points location given sfm_data 
def getAll3DPointloc(sfm_data):
    listKey = []
    list3D = []
    
    for structure in sfm_data["structure"]:
        listKey.append(structure["key"])
        list3D.append(structure["value"]["X"])
    
    return listKey, list3D

# get camera location given sfm_data and list of viewIDs
def get3DViewloc(sfm_data, listID):
    listLoc = []
    
    # generate map from viewID to extrinsics ID
    mapViewID2ExtID = {}
    for j in range(0, len(sfm_data["views"])):
        mapViewID2ExtID[sfm_data["views"][j]["value"]["ptr_wrapper"]["data"]["id_view"]] = sfm_data["views"][j]["value"]["ptr_wrapper"]["data"]["id_pose"]
    
    # generate map from extrinsics ID to extrinsic index
    mapExtID2ExtIdx = {}
    for j in range(0, len(sfm_data["extrinsics"])):
        mapExtID2ExtIdx[sfm_data["extrinsics"][j]["key"]] = j
        
    # get location
    for viewID in listID:
        if viewID in mapViewID2ExtID:
            extID = mapViewID2ExtID[viewID]
            
            if extID in mapExtID2ExtIdx:
                listLoc.append(sfm_data["extrinsics"][mapExtID2ExtIdx[extID]]["value"]["center"])
                continue
        
        # if viewID is not found or no extrinsic, return infinity
        listLoc.append([float("inf"),float("inf"),float("inf")])
        
    return listLoc

# find RANSAC threshold as k times the median of distance between
# consecutive camera points from input sfm_data
def findMedianThres(sfm_data, k):
    if len(sfm_data["extrinsics"])<2:
        return 0
    
    distance = np.zeros([len(sfm_data["extrinsics"]) - 1, 1], dtype=np.float)
    
    X = np.asarray(sfm_data["extrinsics"][0]["value"]["center"]);
    for i in range(0, len(sfm_data["extrinsics"]) - 1):
        Y = np.asarray(sfm_data["extrinsics"][i + 1]["value"]["center"]);
        distance[i] = np.linalg.norm(X - Y)
        X = Y

    return k * np.median(distance)

# TODO : use this function when merging models
# find RANSAC threshold as k times the median of distance between
# structure points from input sfm_data
def findMedianStructurePointsThres(sfm_data, k):
    pointId, point = getAll3DPointloc(sfm_data)
    pointn = np.asarray(point, dtype=np.float)
    
    if pointn.shape[0]<2:
        return 0

    kdtree = cKDTree(pointn)
    
    distance = np.zeros([pointn.shape[0], 1], dtype=np.float)
    for i in range(0, pointn.shape[0]):
        dist, indexes = kdtree.query(pointn[i], 2)
        if len(dist)<2:
            print "Error, array of distance between points should be larger than 1."
            sys.exit()
        distance[i] = dist[1]
    
    return k * np.median(distance)

# find s,R,T such that || sRA + T1' - B ||_F is minimized 
def procrustes(B, A):
    mB = np.mean(B, axis=1)
    mA = np.mean(A, axis=1)
    Bn = (B.T - mB).T
    An = (A.T - mA).T
    
    # get CA = B approximately
    C = np.linalg.lstsq(An.T, Bn.T)[0].T
    """
    print "============"
    print C
    print np.dot(C,An)
    print Bn
    """
    U, s, V = np.linalg.svd(C)
        
    # print s
    s = np.mean(s)
    R = np.dot(U, V)
    """
    print "============"
    print R
    print s*np.dot(R,An)
    print Bn
    print "============"
    """
    T = np.mean(B - s * np.dot(R, A), axis=1, keepdims=True)
    """
    print B
    print s*np.dot(R,A).T
    print T
    print (s*np.dot(R,A).T+T).T-B
    """
    
    return s, R, T

# find matrix M such that A = MB 
# where A and B are both 3 x n
# return M as 3 x 4
# use ransac with threshold to find M
def ransacAffineTransform(A, B, thres, ransacRound, svdRatio=sys.float_info.max):
    
    # stack rows of 1
    B = np.vstack((B, np.ones((1, B.shape[1]))))
    
    # RANSAC
    listInd = range(0, B.shape[1])  # list of all indices
    inliers = np.asarray([])  # to save list of inliers
    nInliers = 0

    for rRound in range(0, ransacRound):
        
        # select 4 points
        sel = random.sample(listInd, 4) 
        
        '''
        # find transformation by procrustes
        s, R, T = procrustes(A[0:3, sel], B[0:3, sel])
        M = np.hstack((s * R, T))
        '''
        # find transformation by least square
        M = np.linalg.lstsq(B[:, sel].T, A[:, sel].T)[0].T
        
        # count inliers
        Btmp = np.dot(M, B)
        
        normVal = np.linalg.norm(Btmp - A, axis=0)
        inliersTmp = np.where(normVal < thres)[0]  
        nInliersTmp = len(inliersTmp)  
    
        # compare 
        if nInliersTmp >= nInliers:
            s = np.linalg.svd(M[0:3, 0:3], compute_uv=False)
            
            if nInliersTmp > nInliers and s[0] / s[-1] < svdRatio: 
                nInliers = nInliersTmp
                inliers = inliersTmp
            
    if len(inliers)==0:
        return np.array([]), inliers
    
    M = np.linalg.lstsq(B[:, inliers].T, A[:, inliers].T)[0].T
    
    print "Number of ransac inliers: " + str(nInliers)
    return M, inliers

# transform coordinate of sfm_data with M as transformation matrix
def transform_sfm_data(sfm_data, M):
    
    for extKey in range(0,len(sfm_data['extrinsics'])):
        # transform rotation
        matRot = np.asarray(sfm_data['extrinsics'][extKey]['value']['rotation'])
        nmatRot = np.dot(M[:,0:3], matRot)
        sfm_data['extrinsics'][extKey]['value']['rotation'] = np.ndarray.tolist(nmatRot)
                
        # transform center
        vCenter = np.asarray(sfm_data['extrinsics'][extKey]['value']['center'])
        nvCenter = np.dot(M[:,0:3],vCenter) + M[:,3]
        sfm_data['extrinsics'][extKey]['value']['center'] = np.ndarray.tolist(nvCenter)
        
    for strKey in range(0,len(sfm_data['structure'])):
        # transform 3d coordinate
        X = np.asarray(sfm_data['structure'][strKey]['value']['X'])
        nX = np.dot(M[:,0:3],X) + M[:,3]
        sfm_data['structure'][strKey]['value']['X'] = np.ndarray.tolist(nX)

# merge sfm_dataB into sfm_dataA with M as transformation matrix
# inlierMapBA maps key key of 3D pt of model B to that of model A
def merge_sfm_data(sfm_dataA, sfm_dataB, M, inlierMapBA, sfmViewBeaconDataA=None, sfmViewBeaconDataB=None):    
    
    # note that use the same intrinsics parameters as sfm_dataA
    
    # get ID of first view after all views of sfm_dataA
    firstViewB = sfm_dataA["views"][-1]["value"]["ptr_wrapper"]["data"]["id_view"]+1
    
    # merge view
    for viewKey in range(0, len(sfm_dataB['views'])):
        view = sfm_dataB['views'][viewKey]  # reference
        view['key'] = firstViewB + view['value']['ptr_wrapper']['data']['id_view']
        view['value'] = sfm_dataB['views'][viewKey]['value']
        view['value']['ptr_wrapper']['data']['filename'] = view['value']['ptr_wrapper']['data']['filename']
        view['value']['ptr_wrapper']['data']['id_intrinsic'] = 0
        view['value']['ptr_wrapper']['data']['id_view'] = firstViewB + view['value']['ptr_wrapper']['data']['id_view']
        view['value']['ptr_wrapper']['data']['id_pose'] = firstViewB + view['value']['ptr_wrapper']['data']['id_pose']
        sfm_dataA['views'].append(view)
        
        # merge beacon data if specified
        if sfmViewBeaconDataA is not None and sfmViewBeaconDataB is not None:
            sfmViewBeaconDataA[len(sfm_dataA['views'])-1] = sfmViewBeaconDataB[viewKey]
            
    # load transformation matrix
    #Minv = np.linalg.inv(np.vstack((M,np.asarray([0,0,0,1]))))
    #Minv = Minv[0:3,:]
    
    # merge extrinsics
    for extKey in range(0,len(sfm_dataB['extrinsics'])):
        extr = sfm_dataB['extrinsics'][extKey]
        extr['key']  =  firstViewB+sfm_dataB['extrinsics'][extKey]['key']
            
        # transform rotation
        matRot = np.asarray(extr['value']['rotation'])
        nmatRot = np.dot(M[:,0:3],matRot)
        extr['value']['rotation'] = np.ndarray.tolist(nmatRot)
        
        # transform center
        vCenter = np.asarray(extr['value']['center'])
        nvCenter = np.dot(M[:,0:3],vCenter) + M[:,3] #np.dot(matRot,Minv[:,3]) + vCenter
        extr['value']['center'] = np.ndarray.tolist(nvCenter)
        sfm_dataA['extrinsics'].append(extr) 
    
    # merge 3D points
    
    # get mapping from 3D pt key (ID) of model A to list index
    # and also get next key for adding 3D pt of model B
    keyToListInd = {}
    nextKey = 0
    for i in range(0,len(sfm_dataA["structure"])):
        ptKey = sfm_dataA["structure"][i]["key"]
        keyToListInd[ptKey] = i
        
        if ptKey > nextKey:
            nextKey = ptKey
            
    nextKey = nextKey + 1 # add 1 to max
    
    # copy structure
    for strKey in range(0,len(sfm_dataB['structure'])):
        stru = sfm_dataB['structure'][strKey]
         
        # if this point matches to a 3D point in model A then merge
        if stru["key"] in inlierMapBA:
            ptAkey = inlierMapBA[stru["key"]] # key/ID of this point in model A
            ptAind = keyToListInd[ptAkey] # index in list
             
            # update view ID and add to points in model A
            for ptB in stru["value"]["observations"]:
                ptB["key"] = firstViewB + ptB["key"] 
                sfm_dataA["structure"][ptAind]["value"]["observations"].append(ptB)
         
        # if has no match then add to the end of point list of model A
        else:
              
            # update view ID for all observations
            for ptB in stru["value"]["observations"]:
                ptB["key"] = firstViewB + ptB["key"] 
                  
            # reset key value to be the next point
            stru["key"] = nextKey
            nextKey = nextKey + 1
              
            # transform its 3D coordinate
            # transform 3d coordinate
            X = np.asarray(stru['value']['X'])
            nX = np.dot(M[:,0:3],X) + M[:,3]
            stru['value']['X'] = np.ndarray.tolist(nX)
              
            # add stru to the end of list of point of model A
            sfm_dataA["structure"].append(stru)

# main function   
# merge 3D models given path to sfm_dataA, sfm_dataB, loc_folderB
# minLimit is minimum number of match between 3D models found before considering merging
# return the number of inliers for transformation
def mergeModel(sfm_data_dirA, sfm_data_dirB, locFolderB, outfile, ransacK=1.0, ransacRound=10000, inputImgDir="", minLimit=4, svdRatio=1.75):
    
    print "Loading sfm_data"
    sfm_dataB = FileUtils.loadjson(sfm_data_dirB)
    
    # read matching pairs from localization result
    imgnameB, matchlistB = readMatch(locFolderB)
    
    # get viewID from image name for model B
    viewIDB = imgnameToViewID(imgnameB, sfm_dataB)

    # get mapping between viewID,featID to 3D point ID
    viewFeatMapB = getViewFeatTo3DMap(sfm_dataB)

    # find consistent match between 3D of model B to 3D of model A
    print "Calculating consistent 3D matches"
    match3D_BA = getConsistent3DMatch(viewIDB, matchlistB, viewFeatMapB)
    print "Found " + str(len(match3D_BA)) + " consistent matches"
    
    # not enough matches
    if len(match3D_BA) <= 4 or len(match3D_BA) <= minLimit:
        return len(match3D_BA), [0]
 
    # move the load of larger model here to reduce time if merging is not possible
    sfm_dataA = FileUtils.loadjson(sfm_data_dirA)
 
    # get 3D point. Note that element 0 of each pair in match3D_BA
    # is 3D pt ID of model B and element 1 is that of model A
    print "Load 3D points"
    pointA = get3DPointloc(sfm_dataA, [x[1] for x in match3D_BA])
    pointB = get3DPointloc(sfm_dataB, [x[0] for x in match3D_BA])
    
    pointAn = np.asarray(pointA, dtype=np.float).T
    pointBn = np.asarray(pointB, dtype=np.float).T
    
    # calculate ransac threshold
    # calculate as 4 times the median of distance between 
    # 3D pt of A 
    print "Find transformation with RANSAC"
    # modified by T.Ishihara 2016.04.08
    # median of camera positions merge too many points, use median of structure points instead
    #ransacThres = findMedianThres(sfm_dataA, ransacK)
    ransacThres = findMedianStructurePointsThres(sfm_dataA, ransacK)

    # TODO : replace with RANSAC similarity transform
    # find robust transformation
    M, inliers = ransacAffineTransform(pointAn, pointBn, ransacThres, ransacRound, svdRatio)
    # cannot find RANSAC transformation
    if (len(inliers)==0):
        return len(match3D_BA), [0]
    print M
    
    # stop if not enough inliers
    sSvd = np.linalg.svd(M[0:3,0:3],compute_uv=0)
    if len(inliers) <= 4 or sSvd[0]/sSvd[-1] > svdRatio:
        return len(inliers), M
    
    # perform merge 
    # last argument is map from inliers 3D pt Id of model B to that of model A
    print "Merging sfm_data"
    merge_sfm_data(sfm_dataA, sfm_dataB, M, {match3D_BA[x][0]: match3D_BA[x][1] for x in inliers})
    
    # change input image folder
    if inputImgDir != "":
        sfm_dataA["root_path"] = inputImgDir
    
    # save json file
    print "Saving json file"
    FileUtils.savejson(sfm_dataA,outfile)
    
    # return number of inliers for transformation
    return len(inliers), M

# for checking sfm model
# get camera locations of merged model and compared against localization result
# sfm_data_path : path to sfm_data.json
# sfm_locOut : path to folder with localization result
# aDist : the distance factor used for determining agreement
#
# This function works by finding median MED of distance between all consecutive 
# frames in sfm_data. If localization result for camera i is within ADIST*MED 
# of merged camera location, then we say localization agrees with merged location.
# The function returns the number of frames localized and number of frames 
# in which each localized location agrees with merged location.
def modelMergeCheckLocal(sfm_data_path, sfm_locOut, aDist=1.5):
    
    # load sfm_data
    sfm_data = FileUtils.loadjson(sfm_data_path)
    
    # find threshold
    medThres = findMedianThres(sfm_data,aDist)
    
    # collect all image names ad location
    imgName = []
    imgLoc = []
    for filename in os.listdir(sfm_locOut):
        
        if filename[-4:]!="json":
                continue
        
        locJsonDict = FileUtils.loadjson(os.path.join(sfm_locOut,filename))
        imgName.append(os.path.basename(locJsonDict["filename"]))
        imgLoc.append(locJsonDict["t"])
        
    imgID = imgnameToViewID(imgName, sfm_data)    
    imgSfMLoc = get3DViewloc(sfm_data,imgID)
        
    # calculate distance and count agreement
    countFile = 0
    countAgree = 0
    
    for j in range(0,len(imgLoc)):
        dist = np.linalg.norm(np.array(imgLoc[j])-np.array(imgSfMLoc[j]))
        if dist < float("inf"):
            countFile = countFile + 1
            
            if dist < medThres:
                countAgree = countAgree + 1
                
    return countFile, countAgree