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

import argparse
import os
import yajl as json
import csv
import scipy as sp
import cv2

MATCH_FILE = "matches.f.txt"

def showKeypointMatch(matchFolder, matchFile, ind1, ind2):
    sfm_data_dir = os.path.join(matchFolder,"sfm_data.json")
    matchFile = os.path.join(matchFolder,matchFile)
    
    with open(sfm_data_dir) as sfm_data_file:
        sfm_data = json.load(sfm_data_file)
        
    rootpath = sfm_data["root_path"]
    
    file1 = sfm_data["views"][ind1]["value"]["ptr_wrapper"]["data"]["filename"]
    img1 = os.path.join(rootpath,file1)
    img1Mat = cv2.imread(img1)
    feat1 = os.path.join(matchFolder,file1[0:-3]+"feat")
    
    file2 = sfm_data["views"][ind2]["value"]["ptr_wrapper"]["data"]["filename"]
    img2 = os.path.join(rootpath,file2)
    img2Mat = cv2.imread(img2)
    feat2 = os.path.join(matchFolder,file2[0:-3]+"feat")
    
    feat1Data = [ row for row in csv.reader(open(feat1, "r"), delimiter=' ') if len(row) != 0]
    feat2Data = [ row for row in csv.reader(open(feat2, "r"), delimiter=' ') if len(row) != 0]
    matchData = []
    with open(matchFile, "r") as f:
        reader = csv.reader(f, delimiter=' ')
        while True:
            try:            
                imagePair = next(reader)
                matchNum = next(reader)
                for x in range(int(matchNum[0])):
                    keypointPair = next(reader)
                    if (int(imagePair[0])==ind1 and int(imagePair[1])==ind2):
                        matchData.append([int(keypointPair[0]),int(keypointPair[1])])
                    elif (int(imagePair[0])==ind2 and int(imagePair[1])==ind1):
                        matchData.append([int(keypointPair[1]),int(keypointPair[0])])
            except csv.Error:
                print "Error"
            except StopIteration:
                break
    
    # visualization
    h1, w1 = img1Mat.shape[:2]
    h2, w2 = img2Mat.shape[:2]
    view = sp.zeros((max(h1, h2), w1 + w2, 3), sp.uint8)
    view[:h1, :w1] = img1Mat
    view[:h2, w1:] = img2Mat
    
    for m in matchData:
        # draw the keypoints
        # print m.queryIdx, m.trainIdx, m.distance
        color = tuple([sp.random.randint(0, 255) for _ in xrange(3)])
        cv2.line(view, (int(float(feat1Data[m[0]][0])), int(float(feat1Data[m[0]][1]))), (int(float(feat2Data[m[1]][0])) + w1, int(float(feat2Data[m[1]][1]))), color)
    
    cv2.imshow("image 1", cv2.resize(img1Mat, (0,0), fx=0.5, fy=0.5))
    cv2.imshow("image 2", cv2.resize(img2Mat, (0,0), fx=0.5, fy=0.5))
    cv2.imshow("matching", cv2.resize(view, (0,0), fx=0.5, fy=0.5))
    cv2.waitKey(1)

def main():
    description = 'This script is for visualizing putative matching result created by OpenMVG. '
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument('match_dir', action='store', nargs=None, const=None, \
                        default=None, type=str, choices=None, metavar=None, \
                        help='Directory path where matches results is stored.')
    parser.add_argument('ind1', action='store', nargs=None, const=None, \
                        default=None, type=int, choices=None, metavar=None, \
                        help='view index 1.')
    parser.add_argument('ind2', action='store', nargs=None, const=None, \
                        default=None, type=int, choices=None, metavar=None, \
                        help='view index 2.')
    args = parser.parse_args()
    matchFolder = args.match_dir
    ind1 = args.ind1
    ind2 = args.ind2
    
    while True:
        print "show matching images : ",ind1," <-> ",ind2
        showKeypointMatch(matchFolder, MATCH_FILE, ind1, ind2)
        key = cv2.waitKey(-1)
        if key==65362: # up
            ind1 += 1
        elif key==65364: # down
            ind1 -= 1
        elif key==65361: # left
            ind2 += 1
        elif key==65363: # right
            ind2 -= 1
        elif key==27:
            break

if __name__ == "__main__":
    main()
