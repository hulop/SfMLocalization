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
#
# This script will create reference points file.
# You can convert SfM result to global coordinate by using the output of this script.
#
################################################################################

import argparse
import os
import json
import cv2
import scipy as scipy
import numpy as np
import math

##########################
# settings 
WINDOW_NAME_FRAME_VIEWER = "SfM Frame Viewer"
WINDOW_NAME_POINT_VIEWER = "SfM Point Viewer"
SHOW_IMAGE_SCALE = 1.0
THRESHOLD_FIND_CLICKED_KEYPOINT = 10
SHOW_KEYPOINT_SIZE = 10
SHOW_KEYPOINT_THICKNESS = 3
SHOW_KEYPOINT_TEXT_SIZE = 1.0

FRAME_SHOW_PAGE_SIZE = 10
SHOW_ALL_KEYPOINT_SIZE = 100
CV_KEY_ESC = 27
CV_KEY_LEFT = 65361
CV_KEY_RIGHT = 65363
CV_KEY_UP = 65362
CV_KEY_DOWN = 65364
CV_KEY_P = 112
CV_KEY_S = 115
CV_KEY_F = 102
CV_KEY_B = 98
CV_KEY_L = 108
CV_KEY_A = 97
CV_KEY_D = 100
##########################

##########################
# global variables
sfm_data = {}
rootpath = ''
views = {}
structure = {}
strucureIdViewIdListMap = {}
viewIdStructureIdListMap = {}
viewIdStructureId2DCoorMap = {}
curKeypoint2DLists = np.array([])
curKeypointIdLists = []
structureIdReal3DcoorMap = {}
##########################

def onMousePointViewer(event, _x, _y, flags, param):    
    x = _x/SHOW_IMAGE_SCALE
    y = _y/SHOW_IMAGE_SCALE    
    
    if event == cv2.EVENT_MOUSEMOVE:
        return
    elif event == cv2.EVENT_RBUTTONDOWN:
        return
    elif event == cv2.EVENT_LBUTTONDOWN:
        global curKeypoint2DLists
        global curKeypointIdLists
        if len(curKeypoint2DLists)>0:
            query = np.array([x,y])
            nearestIndex = scipy.argmin([scipy.inner(query-point,query-point) for point in curKeypoint2DLists])
            nearest = curKeypoint2DLists[nearestIndex]
            if (math.sqrt(scipy.inner(query-nearest,query-nearest)) < THRESHOLD_FIND_CLICKED_KEYPOINT):
                print "selected point index : " + str(nearestIndex)
                print "selected point id : " + str(curKeypointIdLists[nearestIndex])
                print "selected point : " + str(nearest[0]) + "," + str(nearest[1])
                showPointViewer(curKeypointIdLists[nearestIndex])
                return
        print "There is no keypoint around clicked points. Clicked point is (" + str(x) + "," + str(y) + ")"
        return

def showPointViewer(pointId):
    cv2.destroyWindow(WINDOW_NAME_FRAME_VIEWER)
    cv2.destroyWindow(WINDOW_NAME_POINT_VIEWER)
        
    global rootpath
    global view
    global strucureIdViewIdListMap
    global viewIdStructureIdListMap
    global viewIdStructureId2DCoorMap
    global structureIdReal3DcoorMap
    showViewIndex = 0
    while True:
        print "[SfM Point Viewer Mode]"
        print "Selected point ID : " + str(pointId)
        print "Number of views for selected point : " + str(len(strucureIdViewIdListMap[pointId]))
        if pointId in structureIdReal3DcoorMap:
            print "This point already has real world 3D coordinate : " + str(structureIdReal3DcoorMap[pointId])
        else:
            print "This point does not have real world 3D coordinate yet."
        print "Usage : "
        print "    blue keypoint is current keypoint, green keypoint has real world 3D coordinate, red keypoint does not have real world 3D coordinate"
        print "Key Command : "
        print "    right key : show posterior view"
        print "    left key : show previous view"
        print "    down key : show " + str(FRAME_SHOW_PAGE_SIZE) + " previous view"
        print "    up key : show " + str(FRAME_SHOW_PAGE_SIZE) + " posterior views"
        print "    'p' : switch mode to input real world 3D coordinate for this point"
        print "    'd' : switch mode to delete real world 3D coordinate for this point"
        print "    Escape : go back to SfM Frame Viewer Mode"
        
        viewId = strucureIdViewIdListMap[pointId][showViewIndex]
        for view in views:
            if view["key"]==viewId:
                imagePath = os.path.join(rootpath, view["value"]["ptr_wrapper"]["data"]["filename"])
                image = cv2.imread(imagePath)
                
                # draw keypoints
                for drawPointId in viewIdStructureIdListMap[viewId]:
                    if (drawPointId!=pointId) and (drawPointId not in structureIdReal3DcoorMap):
                        pointColor = (0,0,255)
                        pointCenter = (int(viewIdStructureId2DCoorMap[viewId][drawPointId][0]), int(viewIdStructureId2DCoorMap[viewId][drawPointId][1]))
                        cv2.circle(image, pointCenter, SHOW_KEYPOINT_SIZE, pointColor, SHOW_KEYPOINT_THICKNESS)                
                for drawPointId in viewIdStructureIdListMap[viewId]:
                    if (drawPointId!=pointId) and (drawPointId in structureIdReal3DcoorMap):
                        pointColor = (0,255,0)
                        pointCenter = (int(viewIdStructureId2DCoorMap[viewId][drawPointId][0]), int(viewIdStructureId2DCoorMap[viewId][drawPointId][1]))
                        cv2.circle(image, pointCenter, SHOW_KEYPOINT_SIZE, pointColor, SHOW_KEYPOINT_THICKNESS)
                        
                        textOffset = 20
                        textSize = SHOW_KEYPOINT_TEXT_SIZE
                        textColor = (0,255,255)
                        textThickness = 2
                        cv2.putText(image, str(drawPointId) + ": " + str(structureIdReal3DcoorMap[drawPointId]), (pointCenter[0]-textOffset, pointCenter[1]-textOffset), cv2.FONT_HERSHEY_PLAIN, textSize, textColor, textThickness)
                for drawPointId in viewIdStructureIdListMap[viewId]:
                    if drawPointId==pointId:
                        pointColor = (255,0,0)
                        pointCenter = (int(viewIdStructureId2DCoorMap[viewId][drawPointId][0]), int(viewIdStructureId2DCoorMap[viewId][drawPointId][1]))
                        cv2.circle(image, pointCenter, SHOW_KEYPOINT_SIZE, pointColor, SHOW_KEYPOINT_THICKNESS)
                        
                        if drawPointId in structureIdReal3DcoorMap:
                            textOffset = 20
                            textSize = SHOW_KEYPOINT_TEXT_SIZE
                            textColor = (0,255,255)
                            textThickness = 2
                            cv2.putText(image, str(drawPointId) + ": " + str(structureIdReal3DcoorMap[drawPointId]), (pointCenter[0]-textOffset, pointCenter[1]-textOffset), cv2.FONT_HERSHEY_PLAIN, textSize, textColor, textThickness)
                cv2.imshow(WINDOW_NAME_POINT_VIEWER, cv2.resize(image, (0,0), fx=SHOW_IMAGE_SCALE, fy=SHOW_IMAGE_SCALE))
                
                # watch key input
                keyevent = cv2.waitKey(-1)
                if (keyevent==CV_KEY_RIGHT) and showViewIndex<len(strucureIdViewIdListMap[pointId])-1:
                    showViewIndex += 1
                elif (keyevent==CV_KEY_LEFT) and showViewIndex>0:
                    showViewIndex -= 1
                elif (keyevent==CV_KEY_DOWN) and showViewIndex<len(strucureIdViewIdListMap[pointId])-FRAME_SHOW_PAGE_SIZE-1:
                    showViewIndex += FRAME_SHOW_PAGE_SIZE
                elif (keyevent==CV_KEY_UP) and showViewIndex>FRAME_SHOW_PAGE_SIZE-1:
                    showViewIndex -= FRAME_SHOW_PAGE_SIZE
                elif (keyevent==CV_KEY_ESC):
                    cv2.destroyWindow(WINDOW_NAME_POINT_VIEWER)
                    return
                elif (keyevent==CV_KEY_P):
                    cv2.destroyWindow(WINDOW_NAME_FRAME_VIEWER)
                    cv2.destroyWindow(WINDOW_NAME_POINT_VIEWER)    
                    while True:
                        print "[Input 3D Real World Coordinate Mode]"
                        print "Usage : "
                        print "    input 3D coordinate for this point in comma separated format (in the order of x,y,z), like '1.0,2.0,3.0'"
                        print "    type blank text to escape from text input mode"
                        
                        inputline = raw_input()
                        if not inputline:
                            break
                        inputtokens = inputline.split(',')
                        if (len(inputtokens)==3):
                            inputPoint = np.array([float(inputtokens[0]),float(inputtokens[1]),float(inputtokens[2])])
                            structureIdReal3DcoorMap[pointId] = inputPoint
                            break
                        else:
                            print "Error. Input text format is invalid. Please type in comma separated format (in the order of x,y,z), like '1.0,2.0,3.0'"
                elif (keyevent==CV_KEY_D):
                    while True:
                        print "[Delete Real World Coordinate Mode]"
                        print "Usage : "
                        print "    Do you really want to delete real world information for this point?"
                        print "    type 'yes' to delete, type 'no' to go back SfM Point Viewer"
                        
                        inputline = raw_input()
                        if inputline=="yes":
                            del structureIdReal3DcoorMap[pointId]
                            cv2.destroyWindow(WINDOW_NAME_POINT_VIEWER)
                            print "Real world information is deleted for the point : " + str(pointId)
                            return
                        elif inputline=="no":
                            break
                        else:
                            print "Invalid input. Please type 'yes' or 'no'"
                else:
                    print "unknown keyevent : " + str(keyevent)
    
    cv2.destroyWindow(WINDOW_NAME_POINT_VIEWER)
    
def onMouseFrameViewer(event, _x, _y, flags, param):
    x = _x/SHOW_IMAGE_SCALE
    y = _y/SHOW_IMAGE_SCALE    
    
    if event == cv2.EVENT_MOUSEMOVE:
        return
    if event == cv2.EVENT_RBUTTONDOWN:
        return
    if event == cv2.EVENT_LBUTTONDOWN:
        #print "clicked point : " + str(x) + "," + str(y)
        global curKeypoint2DLists
        global curKeypointIdLists
        if len(curKeypoint2DLists)>0:
            query = np.array([x,y])
            nearestIndex = scipy.argmin([scipy.inner(query-point,query-point) for point in curKeypoint2DLists])
            nearest = curKeypoint2DLists[nearestIndex]
            if (math.sqrt(scipy.inner(query-nearest,query-nearest)) < THRESHOLD_FIND_CLICKED_KEYPOINT):
                #print "selected point index : " + str(nearestIndex)
                #print "selected point id : " + str(curKeypointIdLists[nearestIndex])
                #print "selected point : " + str(nearest[0]) + "," + str(nearest[1])
                showPointViewer(curKeypointIdLists[nearestIndex])
        return

def findPreviousFrameWithRegisteredKeypoint(curViewIndex):
    global views
    global viewIdStructureIdListMap
    global structureIdReal3DcoorMap
    for viewIndex in range(curViewIndex - 1, -1, -1):
        for pointId in viewIdStructureIdListMap[views[viewIndex]["key"]]:
            if pointId in structureIdReal3DcoorMap:
                return viewIndex

    print "cannnot find previous view which has any keypoints with real world coordinates"
    return curViewIndex

def findNextFrameWithRegisteredKeypoint(curViewIndex):
    global views
    global viewIdStructureIdListMap
    global structureIdReal3DcoorMap
    for viewIndex in range(curViewIndex + 1, len(views)):
        for pointId in viewIdStructureIdListMap[views[viewIndex]["key"]]:
            if pointId in structureIdReal3DcoorMap:
                return viewIndex
    
    print "cannnot find posterior view which has any keypoints with real world coordinates"
    return curViewIndex

def loadRefPoints3DCoordinate(inputFile):
    print "Start load reference points..."
    
    with open(inputFile) as fp:
        jsonData = json.load(fp)
    
    structureIdReal3DcoorMap = {}
    for refPoint in jsonData['refpoints']:
        structureIdReal3DcoorMap[refPoint['key']] = np.array(refPoint['X'])
    
    print "Finish load reference points."
    print "Number of loaded reference points : " + str(len(structureIdReal3DcoorMap))
    return structureIdReal3DcoorMap

def saveRefPoints3DCoordinate(outputFile, structureIdReal3DcoorMap):
    print "Start save reference points..."
    
    jsonData = {}
    jsonData['refpoints'] = []    
    for structureId in structureIdReal3DcoorMap:
        refPoint = {}
        refPoint['key'] = structureId
        refPoint['X'] = structureIdReal3DcoorMap[structureId].tolist()
        jsonData['refpoints'].append(refPoint)
    
    with open(outputFile,"w") as fp:
        json.dump(jsonData, fp)
    
    print "Finish save reference points."
    print "Number of saved reference points : " + str(len(structureIdReal3DcoorMap))

def main():
    description = 'This script is for editing global 3D coordinate information for sfm_data.json created by OpenMVG.' + \
                'After adding 3D information for more than 4 points from this script, ' + \
                'you can use output file for converting sfm_data.json to global coordinate.'
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument('sfm_data_file', action='store', nargs=None, const=None, \
                        default=None, type=str, choices=None, metavar=None, \
                        help='Input sfm_data.json file which you want to add global coordinate information for 3D points.')
    parser.add_argument('input_output_json_file', action='store', nargs=None, const=None, \
                        default=None, type=str, choices=None, metavar=None, \
                        help='Input/Output json file which stored 3D information for 3D points edited by this script.')
    args = parser.parse_args()
    SFM_DATA_FILE = args.sfm_data_file
    INPUT_OUTPUT_JSON_FILE = args.input_output_json_file
    
    global structureIdReal3DcoorMap
    if os.path.exists(INPUT_OUTPUT_JSON_FILE):
        structureIdReal3DcoorMap = loadRefPoints3DCoordinate(INPUT_OUTPUT_JSON_FILE)
    
    with open(SFM_DATA_FILE) as fp:
        global sfm_data
        global rootpath
        global views
        global structure
        sfm_data = json.load(fp)
        rootpath = sfm_data["root_path"]
        views = sfm_data["views"]
        structure = sfm_data["structure"]
                
        global strucureIdViewIdListMap
        strucureIdViewIdListMap = {}
        for point in structure:
            viewIdList = [observe["key"] for observe in point["value"]["observations"]]
            strucureIdViewIdListMap[point["key"]] = viewIdList            
        
        global viewIdStructureIdListMap
        viewIdStructureIdListMap = {}
        global viewIdStructureId2DCoorMap
        viewIdStructureId2DCoorMap = {}
        for view in views:
            structureIdList = []
            if view["key"] in viewIdStructureIdListMap:
                structureIdList = viewIdStructureIdListMap[view["key"]]
            for point in structure:
                for observe in point["value"]["observations"]:
                    if observe["key"]==view["key"]:
                        structureIdList.append(point["key"])
                        if view["key"] not in viewIdStructureId2DCoorMap:
                            viewIdStructureId2DCoorMap[view["key"]] = {}
                        viewIdStructureId2DCoorMap[view["key"]][point["key"]] = observe["value"]["x"]
            viewIdStructureIdListMap[view["key"]] = structureIdList
        
        showViewIndex = 0
        while (True):
            print "[SfM Frame Viewer Mode]"
            print "Number of keypoints for this view : " + str(len(viewIdStructureIdListMap[views[showViewIndex]["key"]]))
            print "Number of point which has real world coordinate : " + str(len(structureIdReal3DcoorMap))
            print "Current view index : " + str(showViewIndex) + "/" + str(len(views))
            print "Usage : "
            print "    click keypoint will show SfM Point Viewer"
            print "    green keypoint has real world 3D coordinate, red keypoint does not have real world 3D coordinate"
            print "Key Command : "
            print "    right key : show next view"
            print "    left key : show previous view"
            print "    down key : go forward " + str(FRAME_SHOW_PAGE_SIZE) + " views"
            print "    up key : go back " + str(FRAME_SHOW_PAGE_SIZE) + " views"
            print "    'f' : find posterior view which has any keypoints with real world coorninate"
            print "    'b' : find previous view which has any keypoints with real world coorninate"
            print "    'l' : list all keypoints which have real world 3D coordinate"
            print "    'a' : list top " + str(SHOW_ALL_KEYPOINT_SIZE) + " keypoints which are associated with more views"
            print "    'p' : show SfM Point Viewer by inputting point ID"
            print "    's' : save real world coordinates information to the file"
            
            imagePath = os.path.join(rootpath, views[showViewIndex]["value"]["ptr_wrapper"]["data"]["filename"])
            image = cv2.imread(imagePath)
            
            global curKeypoint2DLists
            global curKeypointIdLists
            curKeypoint2DLists = np.array([])
            curKeypointIdLists = []
            # store keypoints in buffer
            for pointId in viewIdStructureIdListMap[views[showViewIndex]["key"]]:                
                if curKeypoint2DLists.shape[0]>0:
                    curKeypoint2DLists = np.vstack((curKeypoint2DLists, np.array([viewIdStructureId2DCoorMap[views[showViewIndex]["key"]][pointId][0], viewIdStructureId2DCoorMap[views[showViewIndex]["key"]][pointId][1]])))
                else:
                    curKeypoint2DLists = np.array([viewIdStructureId2DCoorMap[views[showViewIndex]["key"]][pointId][0], viewIdStructureId2DCoorMap[views[showViewIndex]["key"]][pointId][1]])
                curKeypointIdLists.append(pointId)
            
            # draw keypoints
            for pointId in viewIdStructureIdListMap[views[showViewIndex]["key"]]:
                if pointId not in structureIdReal3DcoorMap:
                    pointColor = (0,0,255)
                    pointCenter = (int(viewIdStructureId2DCoorMap[views[showViewIndex]["key"]][pointId][0]), int(viewIdStructureId2DCoorMap[views[showViewIndex]["key"]][pointId][1]))
                    cv2.circle(image, pointCenter, SHOW_KEYPOINT_SIZE, pointColor, SHOW_KEYPOINT_THICKNESS)
            for pointId in viewIdStructureIdListMap[views[showViewIndex]["key"]]:
                if pointId in structureIdReal3DcoorMap:
                    pointColor = (0,255,0)
                    pointCenter = (int(viewIdStructureId2DCoorMap[views[showViewIndex]["key"]][pointId][0]), int(viewIdStructureId2DCoorMap[views[showViewIndex]["key"]][pointId][1]))
                    cv2.circle(image, pointCenter, SHOW_KEYPOINT_SIZE, pointColor, SHOW_KEYPOINT_THICKNESS)
                    
                    textOffset = 20
                    textSize = SHOW_KEYPOINT_TEXT_SIZE
                    textColor = (0,255,255)
                    textThickness = 2
                    cv2.putText(image, str(pointId) + ": " + str(structureIdReal3DcoorMap[pointId]), (pointCenter[0]-textOffset, pointCenter[1]-textOffset), cv2.FONT_HERSHEY_PLAIN, textSize, textColor, textThickness)
            cv2.imshow(WINDOW_NAME_FRAME_VIEWER, cv2.resize(image, (0,0), fx=SHOW_IMAGE_SCALE, fy=SHOW_IMAGE_SCALE))
            cv2.setMouseCallback(WINDOW_NAME_FRAME_VIEWER, onMouseFrameViewer)
            
            # watch key input
            keyevent = cv2.waitKey(-1)
            if (keyevent==CV_KEY_RIGHT) and showViewIndex<len(views)-1:
                showViewIndex += 1
            elif (keyevent==CV_KEY_LEFT) and showViewIndex>0:
                showViewIndex -= 1
            elif (keyevent==CV_KEY_DOWN) and showViewIndex<len(views)-FRAME_SHOW_PAGE_SIZE-1:
                showViewIndex += FRAME_SHOW_PAGE_SIZE
            elif (keyevent==CV_KEY_UP) and showViewIndex>FRAME_SHOW_PAGE_SIZE-1:
                showViewIndex -= FRAME_SHOW_PAGE_SIZE
            elif (keyevent == CV_KEY_F):
                showViewIndex = findNextFrameWithRegisteredKeypoint(showViewIndex)
            elif (keyevent == CV_KEY_B):
                showViewIndex = findPreviousFrameWithRegisteredKeypoint(showViewIndex)
            elif (keyevent == CV_KEY_L):
                print "[List Keypoints with Real World 3D Coordinates Mode]"
                print "Number of keypoints which has real world 3D coordinate : " + str(len(structureIdReal3DcoorMap))
                print "---------------------------------------------------------------------------"
                print "    Point ID    |    3D Coordinate    |    Number of Views for 3D Point    "
                print "---------------------------------------------------------------------------"
                for structureId in structureIdReal3DcoorMap:
                    print "    " + str(structureId) + "    |    " + str(structureIdReal3DcoorMap[structureId]) + "    |    " + str(len(strucureIdViewIdListMap[structureId]))
                    print "---------------------------------------------------------------------------"
            elif (keyevent == CV_KEY_A):                
                print "[List All Keypoint Mode]"
                print "Number of Keypoints : " + str(len(strucureIdViewIdListMap))
                print "---------------------------------------------------------------------------"
                print "    Point ID    |    3D Coordinate    |    Number of Views for 3D Point    "
                print "---------------------------------------------------------------------------"
                listPointCount = 0
                for structureId in sorted(strucureIdViewIdListMap, key=lambda structureId: len(strucureIdViewIdListMap[structureId]), reverse=True):
                    if structureId in structureIdReal3DcoorMap:
                        print "    " + str(structureId) + "    |    " + str(structureIdReal3DcoorMap[structureId]) + "    |    " + str(len(strucureIdViewIdListMap[structureId]))
                    else:
                        print "    " + str(structureId) + "    |    N/A    |    " + str(len(strucureIdViewIdListMap[structureId]))
                    print "---------------------------------------------------------------------------"
                    listPointCount += 1
                    if listPointCount >= SHOW_ALL_KEYPOINT_SIZE:
                        break
            elif (keyevent == CV_KEY_P):
                while True:
                    print "[Open SfM Point Viewer Mode]"
                    print "Usage : "
                    print "    input point ID which you want to open SfM Point Viewer"
                    print "    type blank text to escape from text input mode"
                      
                    inputline = raw_input()
                    if not inputline:
                        break
                    try:
                        pointId = int(inputline)
                        if pointId in strucureIdViewIdListMap:
                            print "open point viewer : " + str(pointId)
                            showPointViewer(pointId)
                            break;
                        else:
                            print "Invalid Point ID is inputted. Please input Point ID which has at least one frame."
                    except ValueError:
                        print "Invalid format. Please input pointID which you want to open SfM Point Viwer."
            elif (keyevent == CV_KEY_S):
                saveRefPoints3DCoordinate(INPUT_OUTPUT_JSON_FILE, structureIdReal3DcoorMap)
            else:
                print "unknown keyevent : " + str(keyevent)

if __name__ == '__main__':
    main()
