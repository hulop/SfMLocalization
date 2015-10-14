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
import yajl as json
import numpy as np
import random
import string
import hulo_file.FileUtils as FileUtils

# clean sfm_data.json by changing viewID and extrinsicID to start with 0 and end with last frame used for reconstruction
# also, the indices of viewID in matches file is updated as well
#
# return the name of images which are not used for reconstruction
def cleanSfM(sfm_data_path,matchesFile):
    
    sfm_data = FileUtils.loadjson(sfm_data_path)
    if (len(sfm_data["views"])==0):
        print "No views are used in reconstruction of " + sfm_data_path
        return [[],[]]
    if (len(sfm_data["extrinsics"])==0):
        print "No extrinsics are used in reconstruction of " + sfm_data_path
        return [[],[]]
    
    # get map from ID to index
    viewMap = {}
    for i in range(0,len(sfm_data["views"])):
        viewMap[sfm_data["views"][i]["value"]["ptr_wrapper"]["data"]["id_view"]] = i
                
    extMap = {}
    for i in range(0,len(sfm_data["extrinsics"])):
        extMap[sfm_data["extrinsics"][i]["key"]] = i
                
    strMap = {}
    for i in range(0,len(sfm_data["structure"])):
        strMap[sfm_data["structure"][i]["key"]] = i
    
    # find viewIDs of first and last frame used in reconstruction
    firstViewID = len(sfm_data["views"])
    lastViewID = 0
    firstExtID = min(extMap.keys())
    
    for i in range(0,len(sfm_data["views"])):
        viewID = sfm_data["views"][i]["value"]["ptr_wrapper"]["data"]["id_view"]
        extID = sfm_data["views"][i]["value"]["ptr_wrapper"]["data"]["id_pose"]
        
        if extID in extMap:
            if firstViewID > viewID:
                firstViewID = viewID
            if lastViewID < viewID:
                lastViewID = viewID
                
    if firstViewID >= lastViewID:
        print "No views are used in reconstruction of " + sfm_data_path
        return [[],[]]
    
    # get list of unused view back to front
    # and change the view Index
    unusedImgName = [[],[]]
    for i in range(len(sfm_data["views"])-1,lastViewID,-1):
        unusedImgName[1].append(sfm_data["views"][i]["value"]["ptr_wrapper"]["data"]["filename"])
        sfm_data["views"].pop(i)
    
    for i in range(lastViewID,firstViewID-1,-1):
        newViewID = sfm_data["views"][i]["value"]["ptr_wrapper"]["data"]["id_view"]-firstViewID
        sfm_data["views"][i]["key"] = newViewID
        sfm_data["views"][i]["value"]["ptr_wrapper"]["data"]["id_view"] = newViewID
        sfm_data["views"][i]["value"]["ptr_wrapper"]["data"]["id_pose"] = sfm_data["views"][i]["value"]["ptr_wrapper"]["data"]["id_pose"] - firstExtID
            
    for i in range(firstViewID-1,-1,-1):
        unusedImgName[0].append(sfm_data["views"][i]["value"]["ptr_wrapper"]["data"]["filename"])        
        sfm_data["views"].pop(i)
        
    # change extrinsics ID
    for i in range(0,len(sfm_data["extrinsics"])):
        sfm_data["extrinsics"][i]["key"] = sfm_data["extrinsics"][i]["key"]-firstExtID
        
    # change index of refered view in structure
    for i in range(0,len(sfm_data["structure"])):
        for j in range(0,len(sfm_data["structure"][i]["value"]["observations"])):
            sfm_data["structure"][i]["value"]["observations"][j]["key"] = \
                sfm_data["structure"][i]["value"]["observations"][j]["key"]-firstViewID
    
    # save jsonfile back
    FileUtils.savejson(sfm_data,sfm_data_path)
    
    # update matches file
    for matchfile in matchesFile:
        
        matchFileName = os.path.basename(matchfile)
        matchFileNameTmp = matchFileName.join(random.choice(string.lowercase) for i in range(10)) #random name
        matchDir = os.path.dirname(matchfile)
        
        fout = open(os.path.join(matchDir,matchFileNameTmp),"w")
        
        with open(matchfile,"r") as mfile:
            mode = 0
            write = False
            countLine = 0
            
            for line in mfile:
                
                line = line.strip()
                
                if mode == 0:
                    
                    line = line.split(" ")
                    
                    view1 = int(line[0])
                    view2 = int(line[1])
                    
                    if view1 < firstViewID or view1 > lastViewID or \
                        view2 < firstViewID or view2 > lastViewID:
                        write = False
                    else:
                        write = True
                    
                    if write:
                        # update viewID and write out
                        fout.write(str(int(line[0])-firstViewID))
                        fout.write(" ")
                        fout.write(str(int(line[1])-firstViewID))
                        fout.write("\n")
                    
                    countLine = 0
                    mode = 1
                    
                elif mode == 1:
                    
                    numMatch= int(line)
                    
                    if write:
                        # get number of matches and write out
                        fout.write(line + "\n")
                    
                    mode = 2
                    
                elif mode == 2:
                    
                    if write:
                        # write out matches
                        fout.write(line + "\n")
                    
                    countLine = countLine + 1
                    
                    if countLine == numMatch:
                        mode = 0
                        
        os.rename(os.path.join(matchDir,matchFileName),os.path.join(matchDir,matchFileName+"_old"))        
        os.rename(os.path.join(matchDir,matchFileNameTmp),os.path.join(matchDir,matchFileName))        
    
    return unusedImgName
    
# Given a recontruction from a video, usually a bad reconstruction has a long
# "jump" between two frames. This function takes location of sfm_data.json,
# detect first long jump, and cut the video at that point. By cutting, this means
# removal of extrinsics and observation associating with the removed part.
def cutSfMDataJump(sfm_data_location, thresMul=4.5, bufferFrame=5):
    
    if not os.path.isfile(sfm_data_location):
        return 0
    
    # load sfm_data
    with open(sfm_data_location,"r") as jsonfile:
        sfm_data = json.load(jsonfile)
    
    if len(sfm_data["extrinsics"]) < 1:
        return 0
    
    # calculate distance between each frame
    distance = np.zeros([len(sfm_data["extrinsics"])-1,1],dtype=np.float)
    for iExt in range(0,len(sfm_data["extrinsics"])-1):
        X = np.array(sfm_data["extrinsics"][iExt]["value"]["center"])
        Y = np.array(sfm_data["extrinsics"][iExt+1]["value"]["center"])

        distance[iExt] = np.linalg.norm(X-Y)
    
    # find distance between distance
    #diffDist = np.abs(distance[0:-1]-distance[1:])
    diffDistRatio = np.maximum(distance[0:-1]/distance[1:],distance[1:]/distance[0:-1])
    
    # another way to detect jump (haven'e tried) is to perform 
    # median filter first, then do the difference. This will have fewer cut
    # i.e. less likely to cut if the jump interval is fewer than 2 frames
    # (since medfilt kernel = 5) 
    # distanceMed = scipy.signal.medfilt(distance,5) # perform median filter
    # diffDistRatio = np.maximum(distanceMed[0:-1]/distanceMed[1:],distanceMed[1:]/distanceMed[0:-1])
    
    # find the frame that has the jump
    jumpFrame = np.where(diffDistRatio > thresMul)[0] 
    
    # remove jumpframe too close to beginning or end of sequence
    jumpFrame = [x for x in jumpFrame if x > bufferFrame-1 and x < len(sfm_data["extrinsics"])-bufferFrame]
    
    # return if no jump
    if len(jumpFrame) == 0:
        return 0
    
    # otherwise remove all frames after the first jump
    leaveFront = True # bool indicating which part to keep; True for front, False for back
    if jumpFrame[0] > len(sfm_data["extrinsics"]) - jumpFrame[-1]:
        leaveFront = True
        jumpFrame = jumpFrame[0]
    else:
        leaveFront = False
        jumpFrame = jumpFrame[-1]
    
    firstFrameCut = jumpFrame+1 # start cutting from this frame
    
    
    # get viewID of frame with extrinsic at firstFrameCut
    extID = sfm_data["extrinsics"][firstFrameCut]["key"]
    
    viewIDtoRm = -1
    for i in range(0,len(sfm_data["views"])):
        if sfm_data["views"][i]["value"]["ptr_wrapper"]["data"]["id_pose"] == extID:
            viewIDtoRm = sfm_data["views"][i]["value"]["ptr_wrapper"]["data"]["id_view"]
            break
        
    # if cannot find such view ID then sfm_data is corrupted (?), so return   
    if viewIDtoRm == -1:
        print "Cannot find view with extrinsic ID " + str(extID) + " to cut"
        return 0
    
    # remove all extrinsics after that
    if leaveFront:
        del sfm_data["extrinsics"][firstFrameCut:]
    else:
        del sfm_data["extrinsics"][0:firstFrameCut+1]
        
    # remove points associating with viewIDtoRm and after
    for i in range(0,len(sfm_data["structure"])):
        point3D = sfm_data["structure"][i]
        
        # iterate thru observation and start removing from back to prevent index out of bound
        for j in range(len(point3D["value"]["observations"])-1,-1,-1):
            print str(j) + " " + str(len(point3D["value"]["observations"]))
            if leaveFront:
                if point3D["value"]["observations"][j]["key"] >= viewIDtoRm:
                    del point3D["value"]["observations"][j]
            else:
                if point3D["value"]["observations"][j]["key"] <= viewIDtoRm:
                    del point3D["value"]["observations"][j]
                    
    # find a point has fewer than 3 views and remove them
    sfm_data["structure"][:] = [sfm_data["structure"][i] for i in range(0,len(sfm_data["structure"])) if len(sfm_data["structure"][i]["value"]["observations"]) > 2]
    
    # change the key of the points
    for i in range(0,len(sfm_data["structure"])):
        sfm_data["structure"][i]["key"] = i    
        
    # rename old sfm_data and write out new sfm_data
    os.rename(sfm_data_location, sfm_data_location[0:-5] + "_BC.json")
    with open(sfm_data_location,"w") as jsonfile:
        json.dump(sfm_data, jsonfile)
        
    return 1
    