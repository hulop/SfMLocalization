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

class ReconstructParam:

    ###############################################################################################        
    # environment variables
    ###############################################################################################        
    
    # User settings
    WORKSPACE_DIR = "/home/ishihara/VisionLoc"
         
    # Workspace settings
    C_PROJECT_CONFIG = "Release"
    EXTRACT_FEATURE_MATCH_PROJECT = "ExtFeatAndMatch"
    LOCALIZE_PROJECT = "OpenMVGLocalization_AKAZE"
    BUNDLE_ADJUSTMENT_PROJECT = "OpenMVG_BA"
    CAMERA_DATABASE_PATH = "/usr/local/share/openMVG/cameraGenerated.txt"
    
    EXTRACT_FEATURE_MATCH_PROJECT_PATH = WORKSPACE_DIR + "/" + EXTRACT_FEATURE_MATCH_PROJECT + "/" + C_PROJECT_CONFIG + "/" + EXTRACT_FEATURE_MATCH_PROJECT
    LOCALIZE_PROJECT_PATH = WORKSPACE_DIR + "/" + LOCALIZE_PROJECT + "/" + C_PROJECT_CONFIG + "/" + LOCALIZE_PROJECT
    BUNDLE_ADJUSTMENT_PROJECT_PATH = WORKSPACE_DIR + "/" + BUNDLE_ADJUSTMENT_PROJECT + "/" + C_PROJECT_CONFIG + "/" + BUNDLE_ADJUSTMENT_PROJECT    

    ###############################################################################################        
    # extFeatAndMatch
    ###############################################################################################        
    
    # Matching is done only between consecutive frames, while matching 
    # to a farther frames is done by traversing matches between consecutive frames to end frames.
    # This number set the maximum frame distance that feature match will traverse. E.g. if this number
    # is set to 10, features from frame i will be match via tracking-by-matching up to feature in frame i+10.
    # Note that this is different from option -v videoMatchFrame that -v directly perform matching 
    # between frames which are within its number, while -x only matches consecutive frames and propagate
    # the matches via match-track.
    # ExtFeatAndMatch option -x    
    maxTrackletMatchDistance = 10
    
    # Minimum number of putative matches between frames required to retain the matches.
    # If the number of matches is below this number, then all matches is removed.
    # ExtFeatAndMatch option -a
    minMatchToRetain = 30
    
    # Lowe's ratio for feature matching after feature extraction for reconstruction
    # ExtFeatAndMatch option -f
    extFeatDistRatio = 0.7
    
    # Number of RANSAC round for geometric matching after feature extraction for reconstruction
    # ExtFeatAndMatch option -r
    extFeatRansacRound = 500
    
    ###############################################################################################
    # reconstructUnorderedGreedyGraph
    ###############################################################################################        
    
    # focal length for camera
    #focalLength = 800.0      # for shrinked image taken by iPhone 6
    focalLength = 1609.0    # for image taken by iPhone 6
    
    # minimum number of unused frames cut from a video (incl. unused frames from both front and back) 
    # that the unused sequence must have in order for it to be include as a new video sequence
    minUnusedImgLength = 35
    
    # number of times to rerun reconstruction if the reconstruction fails
    rerunRecon = 3
    
    # minimum number of reconstructed frames to consider the model for merging
    minReconFrameToAdd = 25
    
    ###############################################################################################
    # cleanSfM
    ###############################################################################################        
    
    # the cut algorithm will not cut a video into multiple sequences if a jump between consecutive frames' locations
    # occur within BUFFERFRAME frames from beginning or ending of the video
    # cutSfMDataJump arg bufferFrame
    bufferFrame = 5 
        
    ###############################################################################################
    # sfmMergeGraph.mergeOneModel
    ###############################################################################################        
    
    # localization params
    
    # Lowe's ratio for feature matching for localization
    # OpenMVGLocalization_AKAZE option -f
    locFeatDistRatio = 0.6
    
    # Number of RSANSAC round for localization
    # OpenMVGLocalization_AKAZE option -r
    locRansacRound = 25
    
    # Number of frames to skip if current frame from video cannot be localized
    # E.g. if LOCSKIPFRAME=3 and current frame (say frame i) cannot be localized
    # then next frame to try to localize is i+LOCSKIPFRAME-1
    # OpenMVGLocalization_AKAZE option -k
    locSkipFrame = 3
    
    # merge params
    
    # TODO : revisit this parameter
    # Threshold for RANSAC to match 3D points between model A and B is calculated
    # via multiple of median of distance between structure points in 
    # sfm_data.json of model A. This value specifies the number that multiplies 
    # the median.
    # mergeSfM.mergeModel arg ransacK
    ransacStructureThresMul = 10.0
    
    # TODO : revisit this parameter
    # threshold to decide 3D points in merged models will be merged
    # multiplying this value and median distance between structured points will be threshold
    # this parameter is added by T. Ishihara 2016.06.06
    # previously ransacStructureThresMul is used for the same purpose
    mergePointThresMul = 0.01
    
    # TODO : revisit this parameter
    # modified this parameter 4 -> 100 by T. Ishihara 2016.06.07
    # Number of RANSAC round for merging two models. This number multiplies
    # number of matched points between merging models, and will be used as
    # number of RANSAC round
    # mergeSfM.mergeModel arg ransacRound
    ransacRoundMul = 100
    
    # TODO : revisit this parameter, number of minimun inliers should be defined by sfm data size?
    #    modified by T.Ishihara 2016.06.09
    #    100 -> 10
    # Number of 3D inliers after RANSAC to return affine transformation matrix
    # mergeSfM.mergeModel arg minLimit
    #min3DnInliers = 100
    min3DnInliers = 10
    
    # merge validation params
    
    # note : 
    # locFrame means localized frame
    # agrFrame means localized frame that agrees with transformation (see  vldMergeAgrFrameThresK)
    
    # TODO : revisit this parameter
    # Localized frames that agree with transformation is defined as frames that 
    # are localized where its location agrees with the location obtained by 
    # transformimd its 3D model with affine transformation. This agreement is
    # measured by being below a threshold, which is calculated as the median of distance 
    # consecutive cameras in 3D model multiplied by the following number.
    vldMergeAgrFrameThresK = 5
    
    # This condition to check merge is removed by T. Ishihara 2015.11.10
    # ratio between number of inliers against number of agrFrame
    #vldMergeRatioInliersFileagree = 1.5
    
    #    modified by T.Ishihara 2016.06.09
    #    30 -> 10
    # minimum number of agrFrame
    #vldMergeMinCountFileAgree = 30
    vldMergeMinCountFileAgree = 10
    
    # The following two values are related and are used in case
    # of small number of locFrame. If no. of agrFrame is more than 
    # vldMergeSmallMinCountFileAgree but less than vldMergeMinCountFileAgree, 
    # then to pass, we need
    # agreFrame*vldMergeShortRatio > locFrame
    # obsolete this condition by T. Ishihara 2016.04.02    
    #vldMergeSmallMinCountFileAgree = 15
    # obsolete this condition by T. Ishihara 2016.04.02
    #vldMergeShortRatio = 2
    
    # The ratio between agreFrame and frames used in reconstruction
    vldMergeRatioAgrFReconF = 0.1
    
    # The following two values are related
    # If the number of inliers is above vldMergeNInliers
    # and ratio between agreFrame and frames used in reconstruction
    # is above vldMergeRatioAgrFReconFNInliers, then this can replace
    # vldMergeRatioAgrFLocF validation
    vldMergeNInliers = 500
    vldMergeRatioAgrFReconFNInliers = 0.2
    
    # TODO : revisit this parameter
    # The ratio between agreFrame and locFrame
    # modified by T. Ishihara 2016.04.02
    #vldMergeRatioAgrFLocF = 0.6
    vldMergeRatioAgrFLocF = 0.4
    
    # 2016.06.09
    # new condition
    vldMergeRatioInliersMatchPoints = 0.4
    
    ###############################################################################################
    # localizeGlobalCoordinate
    ###############################################################################################        
    ransacThresTransformWorldCoordinateRefImage = 0.3

    ###############################################################################################
    # localizeGlobalCoordinateRefPoint
    ###############################################################################################        
    ransacThresTransformWorldCoordinateRefPoint = 0.1
