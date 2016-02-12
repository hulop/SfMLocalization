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

import argparse
import os
import numpy
import cv2
from glob import glob

##### fixed settings ####
numBoards = 60 # number of boards to capture
square_size = 1.0
pattern_size = (9, 6)
calib_image_path_pattern = "../img/*.jpg"
input_image_path_pattern = "/Input/*/inputImg/*.jpg"
#input_image_path_pattern = "/Ref/inputImg/*.jpg"    
output_K_file = "../data/K.txt"
output_dist_file = "../data/dist.txt"
#########################

def main():
    description = 'This script is for undistorting images. ' + \
        'Before running this program, please prepare images which take chessboard'
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument('project_path', action='store', nargs=None, const=None, \
                        default=None, type=str, choices=None, metavar=None, \
                        help='Directory path where chessboard photos are located.')
    args = parser.parse_args()
    projectPath = args.project_path
    
    # read fixed settings
    global numBoards
    global square_size
    global pattern_size
    global calib_image_path_pattern
    global input_image_path_pattern
    global output_K_file
    global output_dist_file
    
    if not os.path.isfile(output_K_file) or not os.path.isfile(output_dist_file):    
        pattern_points = numpy.zeros( (numpy.prod(pattern_size), 3), numpy.float32 )
        pattern_points[:,:2] = numpy.indices(pattern_size).T.reshape(-1, 2)
        pattern_points *= square_size
        obj_points = []
        img_points = []
        
        for fn in glob(calib_image_path_pattern):
            im = cv2.imread(fn)
            gray = cv2.cvtColor(im, cv2.COLOR_BGR2GRAY)
            print "loading..." + fn
            found, corner = cv2.findChessboardCorners(gray, pattern_size)
            if found:
                term = (cv2.TERM_CRITERIA_EPS + cv2.TERM_CRITERIA_COUNT, 30, 0.1)
                corners2 = cv2.cornerSubPix(gray, corner, (5,5), (-1,-1), term)
                im = cv2.drawChessboardCorners(im, pattern_size, corners2, found)
                img_points.append(corner.reshape(-1, 2))
                obj_points.append(pattern_points)
            if not found:
                print 'chessboard not found'
                
            cv2.imshow("image", cv2.resize(im, (0,0), fx=0.5, fy=0.5))
            cv2.waitKey(1)
            if len(img_points)>numBoards:
                break
     
        rms, K, dist, r, t = cv2.calibrateCamera(obj_points,img_points,(im.shape[1],im.shape[0]),None,None)
        print "RMS = ", rms
        print "K = \n", K
        print "dist = ", dist
        numpy.savetxt(output_K_file, K)
        numpy.savetxt(output_dist_file, dist)
    else:
        with open(output_K_file,"r") as fr:
            K = numpy.loadtxt(fr)
        with open(output_dist_file,"r") as fr:
            dist = numpy.loadtxt(fr)
    
    for fn in glob(projectPath + input_image_path_pattern):
        print "undistort image : " + fn
        im = cv2.imread(fn)
        h, w = im.shape[:2]
        newcameramtx, roi = cv2.getOptimalNewCameraMatrix(K,dist,(w,h),1,(w,h))
        dst = cv2.undistort(im,K,dist,None,newcameramtx)
        x,y,w,h = roi
        dst = dst[y:y+h,x:x+w]
        cv2.imwrite(fn,dst)
        cv2.imshow("original", cv2.resize(im, (0,0), fx=0.5, fy=0.5))
        cv2.imshow("undistort", cv2.resize(dst, (0,0), fx=0.5, fy=0.5))
        cv2.waitKey(1)
 
if __name__ == '__main__':
    main()