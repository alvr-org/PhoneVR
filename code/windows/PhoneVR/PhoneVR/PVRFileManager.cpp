#include "PVRFileManager.h"

#include <chrono>
#include "PVRGlobals.h"

using namespace std;
using namespace nlohmann;

namespace {
	const wchar_t *setsFile = L"C:\\Program Files\\PhoneVR\\pvrsettings.json";
}

json PVRGetSets() {
	try {
		return json::parse(ifstream(setsFile));
	}
	catch (const exception& err) {
		PVR_DB("Error retrieving setting: "s + err.what());
	}
	return json();
}

void PVRSaveSets(json j) {
	ofstream(setsFile) << j.dump(4);
}