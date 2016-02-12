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
# Combine coordinate information in text file by using edge connection information
#
# This projects needs CSV file for edge connection information, and CSV file for list of refcoor.txt
#
# contents of edge connection information CSV file
# <edge ID>,<edge length>,<connected edge>,<offset length to connected edge>,<angle to connected edge>
#
# contents of coordinate information for each edge
# <edge ID>,<coordinate file>
#
################################################################################

import argparse
import sys
import os
import csv
import math
import numpy as np
import hulo_file.FileUtils as FileUtils

def rotationMatrix(angle):
    radian = math.radians(angle)
    R = [[math.cos(radian), -math.sin(radian)],
         [math.sin(radian), math.cos(radian)]]    
    return np.array(R)

#
# read image coordinate CSV file
# file format is as follows
# <image file name>,<x coordinate>,<y coordinate>,<z coordinate>
#
def readImageCoordinateCsv(imgCoorFile):
    imageCoordinate = []
    with open(imgCoorFile, "r") as f:
        reader = csv.reader(f, delimiter=" ")
        for row in reader:
            imageCoordinate.append([row[0], np.array([float(row[1]),float(row[2]),float(row[3])])])
    return imageCoordinate

def main():
    description = 'This script is for converting coordinate information for multiple models.' + \
                'By inputting connecting information for multiple models and local coordinate information for each model, ' + \
                'this script will convert local coordinate information to global coordinate for each model.'
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument('input_edge_csv', action='store', nargs=None, const=None, \
                        default=None, type=str, choices=None, metavar=None, \
                        help='Input CSV file which have information how each model is connected with other models.')
    parser.add_argument('input_coordinate_csv', action='store', nargs=None, const=None, \
                        default=None, type=str, choices=None, metavar=None, \
                        help='Input CSV file which has file path for input/output coordinate information.')
    args = parser.parse_args()
    input_edge_csv = args.input_edge_csv
    input_coordinate_csv = args.input_coordinate_csv
        
    # read edge information and target coordinate files
    edgeIdList = []
    edgeConnect = {}
    edgeOffsetX = {}
    edgeOffsetY = {}
    edgeAngle = {}
    with open(input_edge_csv, "r") as f:
        reader = csv.reader(f)
        for row in reader:
            if (len(row)!=5):
                print "invalid csv for edge connection information"
                sys.exit()
            
            edgeId = int(row[0])
            edgeIdList.append(edgeId)
            edgeConnect[edgeId] = int(row[1])
            edgeOffsetX[edgeId] = float(row[2])
            edgeOffsetY[edgeId] = float(row[3])
            edgeAngle[edgeId] = float(row[4])
    
    coordFileList = []
    with open(input_coordinate_csv, "r") as f:
        reader = csv.reader(f)
        for row in reader:
            coordFileList.append([row[0], row[1], row[2]])
    
    # calculate transformation matrix for each edge
    originEdgeId = -1
    for edgeId in edgeIdList:    
        if (edgeConnect[edgeId]==-1):
            originEdgeId = edgeId
            break
    if (originEdgeId==-1):
        print "error : cannot find origin edge"
        sys.exit()
    print "origin edge : " + str(originEdgeId)
    
    # path for each edge from the origin
    edgePathList = {}
    for edgeId in edgeIdList:
        paths = []
            
        curPath = edgeId
        while True:
            if (curPath==-1):
                break;
            paths.append(curPath)
            curPath = edgeConnect[curPath]
        
        paths.reverse()
        if (paths[0]!=originEdgeId):
            print "error : first path is not origin edge"
            sys.exit()
        edgePathList[edgeId] = paths
    
    # transform for each edge
    edgeTransforms = {}
    for edgeId in edgeIdList:
        transform = np.array([[1,0,0],[0,1,0],[0,0,1]])
        for idx, curPath in enumerate(edgePathList[edgeId]):
            if (idx>0):
                R = rotationMatrix(edgeAngle[curPath])
                T = np.array([edgeOffsetX[curPath], edgeOffsetY[curPath]])
                RT = np.vstack((np.c_[R,T], np.array([0,0,1])))
                transform = np.dot(transform, RT)
        edgeTransforms[edgeId] = transform
    
    # convert coordinate
    for coordFile in coordFileList:
        edgeId = int(coordFile[0])
        print "edge ID : " + str(edgeId)
        print "path : " + str(edgePathList[edgeId])
        print "transform : "
        print (edgeTransforms[edgeId])
        print "input coordinate file : " + coordFile[1]
        print "output coordinate file : " + coordFile[2]
        
        imageCoordinateList = readImageCoordinateCsv(coordFile[1])
        
        if not os.path.isdir(os.path.dirname(coordFile[2])):
            FileUtils.makedir(coordFile[2])
        
        with open(coordFile[2],"w") as outfile:
            for imageCoordinate in imageCoordinateList:
                hcoor = np.array([imageCoordinate[1][0], imageCoordinate[1][1], 1.0])
                gcoor = np.dot(edgeTransforms[edgeId], hcoor)
                outfile.write(imageCoordinate[0] + " "  + str(gcoor[0]) + " "  + str(gcoor[1]) + " "  \
                              + str(imageCoordinate[1][2]) + "\n")
            outfile.close()

if __name__ == '__main__':
    main()