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
# This script will convert Ply file to global coordinate.
# Note this script assume input PLY file has vertex information.
#
################################################################################

import argparse
import numpy as np

def main():
    description = 'This script is for converting PLY file with vertex to global coordinate. ' + \
        'If you have binary PLY file, please convert to ASCII PLY file before inputting this script. '
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument('input_ply_file', action='store', nargs=None, const=None, \
                        default=None, type=str, choices=None, metavar=None, \
                        help='File path of input ASCII ply file with vertex.')
    parser.add_argument('input_Amat_file', action='store', nargs=None, const=None, \
                        default=None, type=str, choices=None, metavar=None, \
                        help='File path of Numpy mat file to convert global coordinate.')
    parser.add_argument('output_ply_file', action='store', nargs=None, const=None, \
                        default=None, type=str, choices=None, metavar=None, \
                        help='File path of output ASCII ply file conveted to global coordinate.')
    args = parser.parse_args()
    input_ply_file = args.input_ply_file
    input_Amat_file = args.input_Amat_file
    output_ply_file = args.output_ply_file
    
    with open(input_Amat_file,"r") as AmatFile:
        Amat = np.loadtxt(AmatFile)
    
    fread = open(input_ply_file, "r")
    fwrite = open(output_ply_file, "w")
        
    for line in fread:
        tokens = line.split()        
        if len(tokens)==8:
            rPoint = np.array([float(tokens[0]), float(tokens[1]), float(tokens[2]), 1.0])
            gPoint = np.dot(Amat,rPoint)
            fwrite.write(str(gPoint[0]) + " " + str(gPoint[1]) + " " + str(gPoint[2]) + " " + " ".join(tokens[3:]) + "\n")
        else:
            fwrite.write(line)
    
    fread.close()
    fwrite.close()
    
if __name__ == '__main__':
    main()
