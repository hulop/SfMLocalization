import sys
import random
import numpy as np
import hulo_transform.transformations as transformations

# find matrix M such that A = MB
# where A and B are both 3 x n
# return M as 3 x 4
# use ransac with threshold to find M
#
# note that you can find similarity transformation if you have at least 3 points,
# but this function use 4 points to find more stable transformation at each RANSAC step
def ransacSimilarityTransform(A, B, thres, ransacRound, svdRatio=sys.float_info.max):
    
    # RANSAC
    listInd = range(0, B.shape[1])  # list of all indices
    inliers = np.asarray([])  # to save list of inliers
    nInliers = 0
    
    for rRound in range(0, ransacRound):
        
        # select 4 points
        sel = random.sample(listInd, 4)
        
        # find tranformation
        M = transformations.superimposition_matrix(B[:, sel].tolist(), A[:, sel].tolist(), scale=True)
        
        # count inliers
        Btmp = np.dot(M, np.vstack((B, np.ones((1, B.shape[1])))))
        
        normVal = np.linalg.norm(Btmp[0:3,:] - A, axis=0)
        inliersTmp = np.where(normVal < thres)[0]
        nInliersTmp = len(inliersTmp)
        
        # compare
        if nInliersTmp >= nInliers:
            s = np.linalg.svd(M[0:3, 0:3], compute_uv=False)
            
            if nInliersTmp > nInliers and s[0] / s[-1] < svdRatio:
                nInliers = nInliersTmp
                inliers = inliersTmp
    
    if len(inliers)<4:
        return np.array([]), np.asarray([])
    
    M = transformations.superimposition_matrix(B[:, inliers].tolist(), A[:, inliers].tolist(), scale=True)
    
    print "Number of ransac inliers: " + str(nInliers)
    return M[0:3,:], inliers

transformations._import_module('_ransacTransform')