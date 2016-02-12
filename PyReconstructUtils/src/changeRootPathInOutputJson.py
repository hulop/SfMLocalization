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
# Just change "root_path" written in sfm_data.json created by OpenMVG.
# You need to modify "root_path" to move sfm_data.json to different PC.
# "root_path" should be the folder which has images used for reconstruction.
#
################################################################################

import argparse
import yajl as json

def main():
    description = 'This script is for modifying root path in OpenMVG output sfm_data.json file.' + \
                'Please note that you need to call this script when moving OpenMVG output sfm_data.json file, ' + \
                'because OpenMVG project file path is hard coded in OpenMVG output file.'
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument('input_file', action='store', nargs=None, const=None, \
                        default=None, type=str, choices=None, metavar=None, \
                        help='Input file path for sfm_data.json which is created by OpenMVG.')
    parser.add_argument('output_file', action='store', nargs=None, const=None, \
                        default=None, type=str, choices=None, metavar=None, \
                        help='Output file path for sfm_data.json whose root path is modified by this script.')
    parser.add_argument('root_path', action='store', nargs=None, const=None, \
                        default=None, type=str, choices=None, metavar=None, \
                        help='New OpenMVG project root path which will be written in output json file.')
    args = parser.parse_args()
    input_file = args.input_file
    output_file = args.output_file
    root_path = args.root_path
    
    with open(input_file) as jsonFile:
        jsonObj = json.load(jsonFile)
    
    jsonObj["root_path"] = root_path
    
    with open(output_file,"w") as jsonfile:
        json.dump(jsonObj, jsonfile)

if __name__ == '__main__':
    main()