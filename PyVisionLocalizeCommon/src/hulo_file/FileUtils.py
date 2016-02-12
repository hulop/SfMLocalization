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
import shutil
import yajl as json
import numpy as np
import struct

def makedir(s):
    if not os.path.isdir(s):
        os.makedirs(s)

def removedir(s):
    if os.path.isdir(s):
        shutil.rmtree(s)

# load json file
def loadjson(filename):
    with open(filename) as filejson:
        jsondata = json.load(filejson)
    return jsondata
 
# save json file
def savejson(jsondata,filename):
    with open(filename,"w") as filejson:
        json.dump(jsondata, filejson)

# read text file which has list of image and location
# each line should have one image file and its 3D location 
# this function will return map from filename to location
def loadImageLocationListTxt(filename,delimit=" "):    
    mapNameLoc = {}
    
    with open(filename,"r") as fileLoc:
        for line in fileLoc:
            line = line.strip()
            line = line.split(delimit)
            
            loc = [float(x) for x in line[1:] if len(x)>0]
            if len(loc) == 3:
                mapNameLoc[line[0]] = loc
            else: 
                print "File " + line[0] + " has invalid location " + str(loc)

    return mapNameLoc

# load Json file which stores reference points
# you can create Json file by "sfmCoordinateEditor"
def loadRefPointsJson(inputFile):
    print "Start load reference points..."
    
    with open(inputFile) as fp:
        jsonData = json.load(fp)
    
    structureIdReal3DcoorMap = {}
    for refPoint in jsonData['refpoints']:
        structureIdReal3DcoorMap[refPoint['key']] = np.array(refPoint['X'])
    
    print "Finish load reference points."
    print "Number of loaded reference points : " + str(len(structureIdReal3DcoorMap))
    return structureIdReal3DcoorMap

# Convert Python OpenCV Mat text file to C++ OpenCV Mat YAML file
def convertNumpyMatTxt2OpenCvMatYml(numpyMatFile,openCVMatFile,matName):
    # load input file
    mat = np.loadtxt(numpyMatFile, dtype='double')
    
    # open output file
    f = open(openCVMatFile,"w")
    
    # write header
    f.write("%YAML:1.0" + "\n")
    f.write(matName + ": !!opencv-matrix" + "\n")
    f.write("   rows: " + str(mat.shape[0]) + "\n")
    if len(mat.shape)>1:
        f.write("   cols: " + str(mat.shape[1]) + "\n")
    else:
        f.write("   cols: " + "1" + "\n")
    f.write("   dt: " + "d" + "\n")
    f.write("   data: [ ")
    if len(mat.shape)>1:
        for i in range(mat.shape[0]):
            for j in range(mat.shape[1]):
                f.write(str(mat[i][j]))
                if (i!=(mat.shape[0]-1)) or (j!=(mat.shape[1]-1)):
                    f.write(", ")
    else:
        for i in range(mat.shape[0]):
            f.write(str(mat[i]))
            if (i!=(mat.shape[0]-1)):
                f.write(", ")
    f.write(" ]" + "\n")
    f.close()

# load binary matrix file which is saved by FileUtils.cpp in VisionLocalizeCommon project
def loadBinMat(filename):
    f = open(filename, "rb")
    
    row = struct.unpack('<i', f.read(4))[0]
    if row==0:
        return np.array()
    
    f.seek(4, os.SEEK_SET)
    col = struct.unpack('<i', f.read(4))[0]
    
    f.seek(8, os.SEEK_SET)
    mattype = struct.unpack('<i', f.read(4))[0]
       
    f.seek(12, os.SEEK_SET)
    if mattype==0:
        dtype = np.uint8
    elif mattype==1:
        dtype = np.int8
    elif mattype==2:
        dtype = np.uint16
    elif mattype==3:
        dtype = np.int16
    elif mattype==4:
        dtype = np.int32
    elif mattype==5:
        dtype = np.float32
    elif mattype==6:
        dtype = np.float64
    else:
        print "invalid mat type : " + str(dtype)        
    return np.fromfile(f, dtype).reshape(row, col)