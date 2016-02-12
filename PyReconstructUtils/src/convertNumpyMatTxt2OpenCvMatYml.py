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
# Convert Python OpenCV Mat text file to C++ OpenCV Mat YAML file
#
################################################################################

import argparse
import hulo_file.FileUtils as FileUtils

def main():
    description = 'This script is for converting Numpy Mat txt file to OpenCV mat YAML file.'
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument('input_file', action='store', nargs=None, const=None, \
                        default=None, type=str, choices=None, metavar=None, \
                        help='Input file path of Mat text data saved by Python Numpy.')
    parser.add_argument('output_file', action='store', nargs=None, const=None, \
                        default=None, type=str, choices=None, metavar=None, \
                        help='Ouput file path of YAML Mat data for C++ OpenCV.')
    parser.add_argument('output_name', action='store', nargs=None, const=None, \
                        default=None, type=str, choices=None, metavar=None, \
                        help='Mat name stored in output YAML file.')
    args = parser.parse_args()
    input_file = args.input_file
    output_file = args.output_file
    output_name = args.output_name
    
    FileUtils.convertNumpyMatTxt2OpenCvMatYml(input_file, output_file, output_name)

if __name__ == '__main__':
    main()