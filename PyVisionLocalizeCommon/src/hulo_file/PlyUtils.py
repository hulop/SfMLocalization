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
import os
import json
import numpy as np

def saveStructurePly(inSfmFile, outPlyFile):
    with open(os.path.join(inSfmFile)) as filejson:
        data = json.load(filejson)
        data['extrinsics'] = []
    with open(os.path.join(os.path.dirname(outPlyFile),"tmp.json"),"w") as filejson:
        json.dump(data, filejson)
    
    os.system("openMVG_main_ComputeSfM_DataColor -i " + os.path.join(os.path.dirname(outPlyFile),"tmp.json") + " -o " + outPlyFile)
    os.remove(os.path.join(os.path.dirname(outPlyFile),"tmp.json"));

def saveCameraPly(inSfmFile, outPlyFile):
    with open(os.path.join(inSfmFile)) as filejson:
        data = json.load(filejson)
        data['structure'] = []
    with open(os.path.join(os.path.dirname(outPlyFile),"tmp.json"),"w") as filejson:
        json.dump(data, filejson)
    
    os.system("openMVG_main_ComputeSfM_DataColor -i " + os.path.join(os.path.dirname(outPlyFile),"tmp.json") + " -o " + outPlyFile)
    os.remove(os.path.join(os.path.dirname(outPlyFile),"tmp.json"));

def saveGlobalPly(inFile, Amat, outFile):
    foundHeader = False
    fout = open(outFile, "w")
    
    with open(inFile) as f:
        for line in f:
            line = line.strip()
            
            # drop lines until find "end_header"         
            if not foundHeader:
                fout.write(line + "\n")
                if line.lower() == "end_header":
                    foundHeader = True
                continue
            
            if len(line) == 0:
                continue
            
            # transform location and write out
            val = np.fromstring(line, dtype=float, sep=" ")
            val[0:3] = np.dot(Amat[:, 0:3], val[0:3]) + Amat[:, 3]
            for i in range(0, len(val)):
                if i < len(val) - 3:
                    fout.write(str(val[i]) + " ")  # write directly for coordinate
                else:
                    fout.write(str(int(val[i])) + " ")  # round float to int for color
            fout.write("\n")
            
    fout.close()
    
def addPointToPly(inFile, points, outFile):
    numPoints = len(points)
    
    foundHeader = False
    fout = open(outFile, "w")
    
    with open(inFile) as f:
        for line in f:
            line = line.strip()
            
            # drop lines until find "end_header"         
            if not foundHeader:
                tokens = line.split()
                if len(tokens)==3 and tokens[0]=="element" and tokens[1]=="vertex":
                    fout.write("element vertex " + str(int(tokens[2])+numPoints) + "\n")
                else:
                    fout.write(line + "\n")
                if line.lower() == "end_header":
                    foundHeader = True
                continue
            
            if len(line) == 0:
                continue
            
            fout.write(line + "\n")
    
    for point in points:
        for val in point:
            fout.write(str(val) + " ")

        fout.write("255 0 0\n")
    
    fout.close()