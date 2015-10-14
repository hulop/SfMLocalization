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
import sys
import hulo_file.SfmDataUtils as SfmDataUtils
    
def main(argv):
    SfmDataUtils.saveGlobalSfM(os.path.join("../data","sfm_data.json"), os.path.join("../data","Amat.txt"), os.path.join("../data","sfm_data_global.json"))
    
    os.system("openMVG_main_ComputeSfM_DataColor -i " + os.path.join("../data","sfm_data.json") + " -o " + os.path.join("../data","sfm_data.ply"))
    os.system("openMVG_main_ComputeSfM_DataColor -i " + os.path.join("../data","sfm_data_global.json") + " -o " + os.path.join("../data","sfm_data_global.ply"))
    
if __name__ == '__main__':
    main(sys.argv)