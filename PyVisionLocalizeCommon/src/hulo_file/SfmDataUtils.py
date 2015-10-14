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
import yajl as json
import numpy as np

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