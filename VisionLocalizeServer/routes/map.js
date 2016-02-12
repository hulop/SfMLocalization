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

var fs = require('fs'), share = require('../lib/share');

exports.structure = function(req, res){  
	var sendErrorResponse = function(code, message) {
		res.statusCode = code;
		res.setHeader("Content-Type", "application/json");
		res.write(JSON.stringify({
			message : message
		}));
		res.end();
	};
	if (!req.query.name) {
		console.log('Error : Map ID is not specified');
		return sendErrorResponse(404, 'Map ID is not specified');
	}
	
    // check map parameter
    if (!share.mapNameMap[req.query.name]) {
    	console.log('Error : Map ID is not valid');
    	return sendErrorResponse(500, 'Map ID is not valid');
    }
    if (!share.mapNameMap[req.query.name]["structure_ply_file"]) {
    	console.log('Error : Map config does not have structure ply file');
    	return sendErrorResponse(500, 'Map config does not have structure ply file');
    }
    
    var filePath = share.mapNameMap[req.query.name]["structure_ply_file"];
    var stat = fs.statSync(filePath);
    res.writeHead(200, {
    	'Access-Control-Allow-Origin': '*',
    	'Content-Type': 'text/plain',
    	'Content-Length': stat.size
    });
    var readStream = fs.createReadStream(filePath);
    readStream.pipe(res);
};

exports.camera = function(req, res){  
	var sendErrorResponse = function(code, message) {
		res.statusCode = code;
		res.setHeader("Content-Type", "application/json");
		res.write(JSON.stringify({
			message : message
		}));
		res.end();
	};
	if (!req.query.name) {
		console.log('Error : Map ID is not specified');
		return sendErrorResponse(404, 'Map ID is not specified');
	}
	
    // check map parameter
    if (!share.mapNameMap[req.query.name]) {
    	console.log('Error : Map ID is not valid');
    	return sendErrorResponse(500, 'Map ID is not valid');
    }
    if (!share.mapNameMap[req.query.name]["camera_ply_file"]) {
    	console.log('Error : Map config does not have camera ply file');
    	return sendErrorResponse(500, 'Map config does not have camera ply file');
    }
    
    var filePath = share.mapNameMap[req.query.name]["camera_ply_file"];
    var stat = fs.statSync(filePath);
    res.writeHead(200, {
    	'Access-Control-Allow-Origin': '*',
    	'Content-Type': 'text/plain',
    	'Content-Length': stat.size
    });
    var readStream = fs.createReadStream(filePath);
    readStream.pipe(res);
};

exports.dense = function(req, res){  
	var sendErrorResponse = function(code, message) {
		res.statusCode = code;
		res.setHeader("Content-Type", "application/json");
		res.write(JSON.stringify({
			message : message
		}));
		res.end();
	};
	if (!req.query.name) {
		console.log('Error : Map ID is not specified');
		return sendErrorResponse(404, 'Map ID is not specified');
	}
	
    // check map parameter
    if (!share.mapNameMap[req.query.name]) {
    	console.log('Error : Map ID is not valid');
    	return sendErrorResponse(500, 'Map ID is not valid');
    }
    if (!share.mapNameMap[req.query.name]["dense_ply_file"]) {
    	console.log('Error : Map config does not have dense ply file');
    	return sendErrorResponse(500, 'Map config does not have dense ply file');
    }
    
    var filePath = share.mapNameMap[req.query.name]["dense_ply_file"];
    var stat = fs.statSync(filePath);
    res.writeHead(200, {
    	'Access-Control-Allow-Origin': '*',
    	'Content-Type': 'text/plain',
    	'Content-Length': stat.size
    });
    var readStream = fs.createReadStream(filePath);
    readStream.pipe(res);
};
