var fs = require('fs');	

var CONFIG_MAP_JSON_FILE = '/config/map.json';
var CONFIG_USER_JSON_FILE = '/config/user.json';

var CONFIG_MAP_JSON_PATH;
if (fs.existsSync(process.cwd() + CONFIG_MAP_JSON_FILE)) {
	CONFIG_MAP_JSON_PATH = process.cwd() + CONFIG_MAP_JSON_FILE;
} else if (fs.existsSync(process.env.VISION_LOCALIZE_SERVER_PATH + CONFIG_MAP_JSON_FILE)) {
	CONFIG_MAP_JSON_PATH = process.env.VISION_LOCALIZE_SERVER_PATH + CONFIG_MAP_JSON_FILE;
} else {
	console.log("Error : cannot find config/map.json. "
			+ "Please execute node application in VisionLocalizeServer directory, "
			+ "or set environment path to VisionLocalizeServer directory.");
	process.exit();
}

var CONFIG_USER_JSON_PATH;
if (fs.existsSync(process.cwd() + CONFIG_USER_JSON_FILE)) {
	CONFIG_USER_JSON_PATH = process.cwd() + CONFIG_USER_JSON_FILE;
} else if (fs.existsSync(process.env.VISION_LOCALIZE_SERVER_PATH() + CONFIG_USER_JSON_FILE)) {
	CONFIG_USER_JSON_PATH = process.env.VISION_LOCALIZE_SERVER_PATH() + CONFIG_USER_JSON_FILE;
} else {
	console.log("Error : cannot find config/user.json. "
			+ "Please execute node application in VisionLocalizeServer directory, "
			+ "or set environment path to VisionLocalizeServer directory.");
	process.exit();
}

var mapJson = JSON.parse(fs.readFileSync(CONFIG_MAP_JSON_PATH, 'utf8'));
console.log("number of loaded maps : " + mapJson['maps'].length);
var mapNameMap = {};
for (var i=0; i<mapJson['maps'].length; i++) {
	console.log("map index : " + i);
	if (mapJson['maps'][i]['name'] && mapJson['maps'][i]['match_dir'] && mapJson['maps'][i]['a_mat_file']) {
		console.log("map name : " + mapJson['maps'][i]['name']);
		console.log("sfm data dir : " + mapJson['maps'][i]['sfm_data_dir']);
		console.log("match dir : " + mapJson['maps'][i]['match_dir']);
		console.log("A mat file : " + mapJson['maps'][i]['a_mat_file']);
		
		mapNameMap[mapJson['maps'][i]['name']] = {};
		for (var p in mapJson['maps'][i]) {
			mapNameMap[mapJson['maps'][i]['name']][p] = mapJson['maps'][i][p];
		}
	} else {
		console.log("Error : invalid map setting json. "
				+ "All map entry should have sfm_data_dir, match_dir, a_mat_file.");
		process.exit();
	}
}

var userJson = JSON.parse(fs.readFileSync(CONFIG_USER_JSON_PATH, 'utf8'));
console.log("number of loaded users : " + userJson['users'].length);
var userNameMap = {};
for (var i=0; i<userJson['users'].length; i++) {
	console.log("user index : " + i);
	if (userJson['users'][i]['name'] && userJson['users'][i]['k_mat_file'] && userJson['users'][i]['dist_mat_file']) {
		console.log("user name : " + userJson['users'][i]['name']);
		console.log("K mat file : " + userJson['users'][i]['k_mat_file']);
		console.log("Dist mat file : " + userJson['users'][i]['dist_mat_file']);
		
		userNameMap[userJson['users'][i]['name']] = {};
		for (var p in userJson['users'][i]) {
			if (p!='scale_image') {
				userNameMap[userJson['users'][i]['name']][p] = userJson['users'][i][p];
			}
		}
		if (userJson['users'][i]['scale_image']) {
			console.log("scale image : " + userJson['users'][i]['scale_image']);
			userNameMap[userJson['users'][i]['name']]['scale_image'] = parseFloat(userJson['users'][i]['scale_image']);
		} else {
			console.log("do not scale input image when server localizes");
			userNameMap[userJson['users'][i]['name']]['scale_image'] = 1.0;
		}
	} else {
		console.log("Error : invalid user setting json. "
				+ "All user entry should have k_mat_file, dist_mat_file.");
		process.exit();
	}
}

var userHistories = {};

exports.mapNameMap = mapNameMap;
exports.userNameMap = userNameMap;
exports.userHistories = userHistories;