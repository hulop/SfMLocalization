# -*- coding: utf-8 -*-
import argparse
import os
import json
import numpy as np
import sys
import shutil

VISION_LOCALIZE_WORKSPACE_PATH = "/home/ishihara/VisionLoc"

# remove SfM result if the distance between consective frame is larger than REMOVE_NOISE_THRESHOLD*medianDist
REMOVE_NOISE_THRESHOLD = 5.0

# do not add SfM estimation to beacon sample if time difference is larger than this value 
MAX_TIME_DIFF_BEACON_SFM = 1.0

def addSfMEstimateToBeaconCSV(csv_file, sfm_result_file, beacon_file):    
    shutil.copyfile(csv_file, csv_file + ".backup")
    
    # read SfM localization result
    with open(sfm_result_file) as f:
        sfm_data = json.load(f)

        filenameSfmPosMap = {}
        filenameSfmRotMap = {}
        for i in range(0,len(sfm_data["locGlobal"])):
            filename = os.path.basename(sfm_data["locGlobal"][i]["filename"])
            t = sfm_data["locGlobal"][i]["t"]
            filenameSfmPosMap[filename] = t
            
            rot = sfm_data["locGlobal"][i]["R"]
            filenameSfmRotMap[filename] = rot
    
        # calculate mean distance to previous, next frame, and medium threshold to remove noisy estimation from groundtruth
        filenameSfmDistMap = {}
        distance = np.zeros([len(sfm_data["locGlobal"]), 1], dtype=np.float)
        for i in range(0,len(sfm_data["locGlobal"])):
            filename = os.path.basename(sfm_data["locGlobal"][i]["filename"])
            if i==0:
                dist = np.linalg.norm(np.asarray(sfm_data["locGlobal"][i]["t"])-np.asarray(sfm_data["locGlobal"][i+1]["t"]))
            elif i==len(sfm_data["locGlobal"])-1:
                dist = np.linalg.norm(np.asarray(sfm_data["locGlobal"][i-1]["t"])-np.asarray(sfm_data["locGlobal"][i]["t"]))
            else:
                dist1 = np.linalg.norm(np.asarray(sfm_data["locGlobal"][i-1]["t"])-np.asarray(sfm_data["locGlobal"][i]["t"]))
                dist2 = np.linalg.norm(np.asarray(sfm_data["locGlobal"][i]["t"])-np.asarray(sfm_data["locGlobal"][i+1]["t"]))
                dist = (dist1 + dist2)/2.0
            distance[i] = dist
            filenameSfmDistMap[filename] = dist
        medianDist = np.median(distance) * REMOVE_NOISE_THRESHOLD
    
    # read original CSV data
    csv_orig_lines = []
    with open(csv_file) as f:
        for line in f:
            csv_orig_lines.append(line)
    
    # add SfM estimate to original CSV data
    sfmTimestamp = -1
    sfmT = []
    with open(csv_file,"w") as f:
        for line in csv_orig_lines:
            line = line.strip()
            splitline = line.split(",")
            
            # parse by type
            timestamp = int(splitline[0])
            if len(sfmT)>=3 and abs(timestamp-sfmTimestamp)/1000<MAX_TIME_DIFF_BEACON_SFM and splitline[1] == "Beacon":
                # write time stamp and Beacon name
                for i in range(2):
                    f.write(splitline[i])
                    f.write(",")
                # write SfM estimate position
                for i in range(3):
                    f.write(str(sfmT[i]))
                    f.write(",")
                # write floor
                f.write("0")
                f.write(",")
                # write Beacon signal
                for i in range(6, len(splitline)):
                    f.write(splitline[i])
                    if (i<len(splitline)-1):
                        f.write(",")
                f.write("\n")
            else:
                f.write(line)
                f.write("\n")
            
            # cache SfM estimate
            if splitline[1] == "Misc" and splitline[2] == "Photo":
                if splitline[3] in filenameSfmPosMap:
                    if filenameSfmDistMap[splitline[3]]<medianDist:
                        sfmTimestamp = int(splitline[0])
                        sfmT = filenameSfmPosMap[splitline[3]]
        
        f.close()

def main():
    description = 'This script is for generating PSIM test file containing pose and image with time stamp. '
    parser = argparse.ArgumentParser(description=description)
    parser.add_argument('project_dir', action='store', nargs=None, const=None, \
                        default=None, type=str, choices=None, metavar=None, \
                        help='Directory path where OpenMVG project is located.')
    parser.add_argument('matches_dir', action='store', nargs=None, const=None, \
                        default=None, type=str, choices=None, metavar=None, \
                        help='Directory path where OpenMVG created matches files.')
    parser.add_argument('sfm_data_dir', action='store', nargs=None, const=None, \
                        default=None, type=str, choices=None, metavar=None, \
                        help='Directory path where OpenMVG sfm_data.json is located.')
    parser.add_argument('test_project_dir', action='store', nargs=None, const=None, \
                        default=None, type=str, choices=None, metavar=None, \
                        help='Directory path where localization test image files are located.')
    parser.add_argument('output_beacon_csv', action='store', nargs=None, const=None, \
                        default=None, type=str, choices=None, metavar=None, \
                        help='File path where beacon signal CSV will be saved.')
    parser.add_argument('--bow', action='store_true', default=False, \
                        help='Use BOW to accelerate localization if this flag is set (default: False)')
    parser.add_argument('--beacon', action='store_true', default=False, \
                        help='Use iBeacon to accelerate localization if this flag is set (default: False)')    
    args = parser.parse_args()
    project_dir = args.project_dir
    matches_dir = args.matches_dir
    sfm_data_dir = args.sfm_data_dir
    test_project_dir = args.test_project_dir
    output_beacon_csv = args.output_beacon_csv
    USE_BOW = args.bow
    USE_BEACON = args.beacon
    
    if not os.path.exists(os.path.join(project_dir, "Ref", "Amat.txt")):
        print "Error : A mat file does not exist. Please reconstruct model at first."
        sys.exit()
    if not os.path.exists(os.path.join(matches_dir, "BOWfile.yml")):
        print "Error : BOW file does not exist."
        sys.exit()
    if not os.path.exists(os.path.join(test_project_dir,"Input","listbeacon.txt")):
        print "Error : listbeacon.txt file does not exist."
        sys.exit()
    
    if USE_BOW and not USE_BEACON:
        print "Execute : python " + VISION_LOCALIZE_WORKSPACE_PATH + "/PyEvaluateAccuracy/src/localizeGlobalCoordinate.py " + \
                project_dir + " " + matches_dir + " " + sfm_data_dir + " -t " + os.path.join(test_project_dir,"Input") + " -o " + "loc_global.json" + " --bow"
        os.system("python " + VISION_LOCALIZE_WORKSPACE_PATH + "/PyEvaluateAccuracy/src/localizeGlobalCoordinate.py " + \
                project_dir + " " + matches_dir + " " + sfm_data_dir + " -t " + os.path.join(test_project_dir,"Input") + " -o " + "loc_global.json" + " --bow")
    elif not USE_BOW and USE_BEACON:
        print "Execute : python " + VISION_LOCALIZE_WORKSPACE_PATH + "/PyEvaluateAccuracy/src/localizeGlobalCoordinate.py " + \
                project_dir + " " + matches_dir + " " + sfm_data_dir + " -t " + os.path.join(test_project_dir,"Input") + " -o " + "loc_global.json" + " --beacon"
        os.system("python " + VISION_LOCALIZE_WORKSPACE_PATH + "/PyEvaluateAccuracy/src/localizeGlobalCoordinate.py " + \
                project_dir + " " + matches_dir + " " + sfm_data_dir + " -t " + os.path.join(test_project_dir,"Input") + " -o " + "loc_global.json" + " --beacon")
    elif USE_BOW and USE_BEACON:
        print "Execute : python " + VISION_LOCALIZE_WORKSPACE_PATH + "/PyEvaluateAccuracy/src/localizeGlobalCoordinate.py " + \
                project_dir + " " + matches_dir + " " + sfm_data_dir + " -t " + os.path.join(test_project_dir,"Input") + " -o " + "loc_global.json" + " --bow" + " --beacon"
        os.system("python " + VISION_LOCALIZE_WORKSPACE_PATH + "/PyEvaluateAccuracy/src/localizeGlobalCoordinate.py " + \
                project_dir + " " + matches_dir + " " + sfm_data_dir + " -t " + os.path.join(test_project_dir,"Input") + " -o " + "loc_global.json" + " --bow" + " --beacon")
    else:
        print "Execute : python " + VISION_LOCALIZE_WORKSPACE_PATH + "/PyEvaluateAccuracy/src/localizeGlobalCoordinate.py " + \
                project_dir + " " + matches_dir + " " + sfm_data_dir + " -t " + os.path.join(test_project_dir,"Input") + " -o " + "loc_global.json"
        os.system("python " + VISION_LOCALIZE_WORKSPACE_PATH + "/PyEvaluateAccuracy/src/localizeGlobalCoordinate.py " + \
                project_dir + " " + matches_dir + " " + sfm_data_dir + " -t " + os.path.join(test_project_dir,"Input") + " -o " + "loc_global.json")

    # add SfM estimation to beacon sample files    
    for inputDir in os.listdir(os.path.join(test_project_dir,"Input")):
        if os.path.isfile(os.path.join(test_project_dir,"Input",inputDir)):
            continue
        
        csvFiles = os.listdir(os.path.join(test_project_dir,"Input",inputDir,"csv"))
        if (len(csvFiles)>=1):
            for csvFile in csvFiles:
                csvFilename, csvFileExt = os.path.splitext(csvFile)
                if csvFileExt==".csv":
                    csv_file = os.path.join(test_project_dir,"Input",inputDir,"csv",csvFile)
                    sfm_result_file = os.path.join(test_project_dir,"Input",inputDir,"loc","loc_global.json")
                    beacon_file = os.path.join(test_project_dir,"Input","listbeacon.txt")
                    addSfMEstimateToBeaconCSV(csv_file, sfm_result_file, beacon_file)
        else:
            print "cannot find beacon signal file in the directory : " + os.path.join(test_project_dir,"Input",inputDir,"csv")

    # create beacon csv file by merging all beacon signal with SfM estimation
    if os.path.exists(output_beacon_csv):
        os.remove(output_beacon_csv)
    for inputDir in os.listdir(os.path.join(test_project_dir,"Input")):
        if os.path.isfile(os.path.join(test_project_dir,"Input",inputDir)):
            continue
        
        csvFiles = os.listdir(os.path.join(test_project_dir,"Input",inputDir,"csv"))
        if (len(csvFiles)>=1):
            for csvFile in csvFiles:
                csvFilename, csvFileExt = os.path.splitext(csvFile)
                if csvFileExt==".csv":
                    csv_file = os.path.join(test_project_dir,"Input",inputDir,"csv",csvFile)
                    
                    csv_lines = []
                    with open(csv_file) as f:
                        for line in f:
                            csv_lines.append(line)
                    
                    with open(output_beacon_csv,"a") as f:
                        for line in csv_lines:
                            line = line.strip()
                            splitline = line.split(",")
                            
                            # parse by type
                            if splitline[1] == "Beacon":
                                try:
                                    # check if Beacon sample has SfM estimation (x, y, z, floor) 
                                    for i in range(2,6):
                                        position = float(splitline[i])
                                    # write Beacon sample if check passed
                                    f.write(line)
                                    f.write("\n")
                                except ValueError:
                                    print "Skip Beacon sample without SfM estimation"                    
        else:
            print "cannot find beacon signal file in the directory : " + os.path.join(test_project_dir,"Input",inputDir,"csv")

if __name__ == '__main__':
    main()