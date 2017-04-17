/*******************************************************************************
* Copyright (c) 2015 IBM Corporation
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*******************************************************************************/

/*
 * GET or POST image for localization.
 */

var validator = require('validator'), async = require('async'), request = require('request'), fs = require('fs'), 
	localizeImage = require('bindings')('localizeImage'), share = require('../lib/share');

var MAX_USER_HISTORY_LENGTH = 10;

/*
 * GET parameters 
 *  user : ID of user
 * 	map : ID of map
 * 	image : URL of request image
 *  cx : center to restrict localization area (optional)
 *  cy : center to restrict localization area (optional)
 *  cz : center to restrict localization area (optional)
 *  radius : center to restrict localization area (optional)
 *  beacon : iBeacon signal for query image (optional)
 * example request
 * http://localhost:3000/localize?user=1&map=office&image=https://www.google.co.jp/images/srpr/logo11w.png
 */
exports.estimateGet = function(req, res) {
    var sendErrorResponse = function(code, message) {
        res.statusCode = code;
        res.setHeader("Content-Type", "application/json");
        res.write(JSON.stringify({
            message : message
        }));
        res.end();
    };
    if (!req.query.user) {
    	console.log('Error : User ID is not specified');
        return sendErrorResponse(404, 'User ID is not specified');
    }
    if (!req.query.map) {
    	console.log('Error : Map ID is not specified');
        return sendErrorResponse(404, 'Map ID is not specified');
    }
    if (!req.query.image) {
    	console.log('Error : Image URL is not specified');
        return sendErrorResponse(404, 'Image URL is not specified');
    }

    var imageURL = decodeURIComponent(req.query.image);
    if (!validator.isURL(imageURL)) {
    	console.log('Error : Image URL is not valid');
        return sendErrorResponse(404, 'Image URL is not valid');
    }
        
    // check user parameter
    if (!share.userNameMap[req.query.user]) {
    	console.log('Error : User ID is not valid');
    	return sendErrorResponse(500, 'User ID is not valid');
    }
    var kMatFile = share.userNameMap[req.query.user]['k_mat_file'];
    var distMatFile = share.userNameMap[req.query.user]['dist_mat_file'];
    var scaleImage = share.userNameMap[req.query.user]['scale_image'];
    
    // check map parameter
    if (!share.mapNameMap[req.query.map]) {
    	console.log('Error : Map ID is not valid');
    	return sendErrorResponse(500, 'Map ID is not valid');
    }
	var sfmDataDir = share.mapNameMap[req.query.map]['sfm_data_dir'];
	var matchDir = share.mapNameMap[req.query.map]['match_dir'];
	var aMatFile = share.mapNameMap[req.query.map]['a_mat_file'];
	
	// check return keypoint parameter
	var returnKeypoint = false;
    if (req.query.returnKeypoint) {
    	returnKeypoint = (req.query.returnKeypoint == 'true');
    }
	// check return time parameter
	var returnTime = false;
    if (req.query.returnTime) {
    	returnTime = (req.query.returnTime == 'true');
    }
    
    async.waterfall([ function(callback) {
        var requestSettings = {
            method : 'GET',
            url : imageURL,
            encoding : null
        };

        var size = 0;
        var req = request(requestSettings, function(error, response, body) {
            callback(error, body);
        }).on('data', function(chunk) {
            size += chunk.length;
            console.log('download size : ' + size);
        });
    }, function(image, callback) {
        var result;
        if (req.query.beacon) {
	        if (req.query.cx && req.query.cy && req.query.cz && req.query.radius) {
	        	result = localizeImage.localizeImageBufferBeacon(req.query.user, kMatFile, distMatFile, scaleImage,
	        			req.query.map, sfmDataDir, matchDir, aMatFile, returnKeypoint, returnTime, image, req.query.beacon,
	        			[req.query.cx, req.query.cy, req.query.cz], req.query.radius);
	        } else {
	        	result = localizeImage.localizeImageBufferBeacon(req.query.user, kMatFile, distMatFile, scaleImage,
	        			req.query.map, sfmDataDir, matchDir, aMatFile, returnKeypoint, returnTime, image, req.query.beacon);
	        }
        } else {
	        if (req.query.cx && req.query.cy && req.query.cz && req.query.radius) {
	        	result = localizeImage.localizeImageBuffer(req.query.user, kMatFile, distMatFile, scaleImage,
	        			req.query.map, sfmDataDir, matchDir, aMatFile, returnKeypoint, returnTime, image, 
	        			[req.query.cx, req.query.cy, req.query.cz], req.query.radius);
	        } else {
	        	result = localizeImage.localizeImageBuffer(req.query.user, kMatFile, distMatFile, scaleImage,
	        			req.query.map, sfmDataDir, matchDir, aMatFile, returnKeypoint, returnTime, image);
	        }
        }
        
        var jsonObj;
        if (result && result.length==5) {
        	console.log('localization successed.');
        	
        	console.log('estimate : ' + result[0]);
        	jsonObj = {'estimate':{'t':result[0].slice(0,3), 'R':[result[0].slice(3,6),result[0].slice(6,9),result[0].slice(9,12)]}};
        	
        	if (returnKeypoint) {
            	console.log('keypoints 2D : ' + result[1]);
            	console.log('keypoints 3D : ' + result[2]);
            	console.log('inlier keypoints : ' + result[3]);
        		
        		jsonObj['keypoints2D'] = result[1];
        		jsonObj['keypoints3D'] = result[2];
        		jsonObj['inlierKeypoints'] = result[3];
        	}
        	if (returnTime) {
            	console.log('times : ' + result[4]);
            	
        		jsonObj['times'] = result[4];
        	}
        	
            // update users' history
        	if (!share.userHistories[req.query.user]) {
        		share.userHistories[req.query.user] = [];
        	}
        	share.userHistories[req.body.user].push(jsonObj);
        } else {
        	console.log('localization failed.');
        	
            jsonObj = {'estimate':{'t':[], 'R':[]}};
        }
       	// Send back the result as a JSON                    
        callback(null, JSON.stringify(jsonObj));        
    } ], function(err, result) {
        if (err) {
            sendErrorResponse(500, err.message);
        }
        res.setHeader("Content-Type", "application/json");
        res.header("Access-Control-Allow-Origin", "*");
        res.write(result);
        res.end();
    });
};

/*
 * POST parameters 
 *  user : ID of user
 * 	map : ID of map
 *  image : binary image data
 *  cx : center to restrict localization area (optional)
 *  cy : center to restrict localization area (optional)
 *  cz : center to restrict localization area (optional)
 *  radius : center to restrict localization area (optional)
 *  beacon : iBeacon signal for query image (optional)
 */
exports.estimatePost = function(req, res) {
    var sendErrorResponse = function(code, message) {
        res.statusCode = code;
        res.setHeader("Content-Type", "application/json");
        res.write(JSON.stringify({
            message : message
        }));
        res.end();
    };
    if (!req.body.user) {
    	console.log('Error : User ID is not specified');
        return sendErrorResponse(404, 'User ID is not specified');
    }
    if (!req.body.map) {
    	console.log('Error : Map ID is not specified');
        return sendErrorResponse(404, 'Map ID is not specified');
    }
    if (!req.files || !req.files.image) {
    	console.log('Error : Image data is not specified');
        return sendErrorResponse(404, "Image data is not specified");
    }
        
    // check user parameter
    if (!share.userNameMap[req.body.user]) {
    	console.log('Error : User ID is not valid');
    	return sendErrorResponse(500, 'User ID is not valid');
    }
    var kMatFile = share.userNameMap[req.body.user]['k_mat_file'];
    var distMatFile = share.userNameMap[req.body.user]['dist_mat_file'];
    
    // check map parameter
    if (!share.mapNameMap[req.body.map]) {
    	console.log('Error : Map ID is not valid');
    	return sendErrorResponse(500, 'Map ID is not valid');
    }
	var sfmDataDir = share.mapNameMap[req.body.map]['sfm_data_dir'];
	var matchDir = share.mapNameMap[req.body.map]['match_dir'];
	var aMatFile = share.mapNameMap[req.body.map]['a_mat_file'];
    var scaleImage = share.userNameMap[req.body.user]['scale_image'];
    
	// check return keypoint parameter
	var returnKeypoint = false;
    if (req.body.returnKeypoint) {
    	returnKeypoint = (req.body.returnKeypoint == 'true');
    }
	// check return time parameter
	var returnTime = false;
    if (req.body.returnTime) {
    	returnTime = (req.body.returnTime == 'true');
    }
    
    async.waterfall([ function(callback) {
        fs.readFile(req.files.image.path, function (err, data) {
            var imageName = req.files.image.name;
            if(!imageName){
                console.log("Error to load uploaded image");
                res.redirect("/");
                res.end();
            } else {
                var result;
                if (req.body.beacon) {
                    if (req.body.cx && req.body.cy && req.body.cz && req.body.radius) {
                        result = localizeImage.localizeImageBufferBeacon(req.body.user, kMatFile, distMatFile, scaleImage,
                        		req.body.map, sfmDataDir, matchDir, aMatFile, returnKeypoint, returnTime,
                        		data, req.body.beacon, [req.body.cx, req.body.cy, req.body.cz], req.body.radius);
                    } else {
                        result = localizeImage.localizeImageBufferBeacon(req.body.user, kMatFile, distMatFile, scaleImage,
                        		req.body.map, sfmDataDir, matchDir, aMatFile, returnKeypoint, returnTime, data, req.body.beacon);
                    }
                } else {
	                if (req.body.cx && req.body.cy && req.body.cz && req.body.radius) {
	                    result = localizeImage.localizeImageBuffer(req.body.user, kMatFile, distMatFile, scaleImage,
	                    		req.body.map, sfmDataDir, matchDir, aMatFile, returnKeypoint, returnTime,
	                    		data, [req.body.cx, req.body.cy, req.body.cz], req.body.radius);
	                } else {
	                    result = localizeImage.localizeImageBuffer(req.body.user, kMatFile, distMatFile, scaleImage,
	                    		req.body.map, sfmDataDir, matchDir, aMatFile, returnKeypoint, returnTime, data);
	                }
                }
                                
                var jsonObj;
                if (result && result.length==5) {
                	console.log('localization successed.');
                	
                	console.log('estimate : ' + result[0]);
                	jsonObj = {'estimate':{'t':result[0].slice(0,3), 'R':[result[0].slice(3,6),result[0].slice(6,9),result[0].slice(9,12)]}};
                	
                	if (returnKeypoint) {
                    	console.log('keypoints 2D : ' + result[1]);
                    	console.log('keypoints 3D : ' + result[2]);
                    	console.log('inlier keypoints : ' + result[3]);
                		jsonObj['keypoints2D'] = result[1];
                		jsonObj['keypoints3D'] = result[2];
                		jsonObj['inlierKeypoints'] = result[3];
                	}
                	if (returnTime && result[4].length==7) {
                		console.log('time selectBeacon : ' + result[4][0]);
                		console.log('time selectBow : ' + result[4][1]);
                		console.log('time extFeat : ' + result[4][2]);
                		console.log('time putMatch : ' + result[4][3]);
                		console.log('time geoMatch : ' + result[4][4]);
                		console.log('time PnP : ' + result[4][5]);
                		console.log('time others : ' + result[4][6]);
                    	
                		jsonObj['times'] = {
                				'selectBeacon':result[4][0],
                				'selectBow':result[4][1],
                				'extFeat':result[4][2],
                				'putMatch':result[4][3],
                				'geoMatch':result[4][4],
                				'PnP':result[4][5],
                				'others':result[4][6]
                		};
                	}
                	
                    // update users' history
                	if (!share.userHistories[req.body.user]) {
                		share.userHistories[req.body.user] = [];
                	}
                	if (share.userHistories[req.body.user].length>MAX_USER_HISTORY_LENGTH) {
                		share.userHistories[req.body.user].shift();
                	}
                	share.userHistories[req.body.user].push(jsonObj);
                } else {
                	console.log('localization failed.');
                	
                    jsonObj = {'estimate':{'t':[], 'R':[]}};
                }
               	// Send back the result as a JSON                    
                callback(null, JSON.stringify(jsonObj));
            }
        });
    } ], function(err, result) {
        if (err) {
            sendErrorResponse(500, err.message);
        }
        res.setHeader("Content-Type", "application/json");
        res.header("Access-Control-Allow-Origin", "*");
        res.write(result);
        res.end();
    });
};
