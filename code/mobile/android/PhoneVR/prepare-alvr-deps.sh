if [ "$1" == "nogvr" ]; then
nogvr=true
shift
fi

rustup target add \
    aarch64-linux-android \
    armv7-linux-androideabi \
    x86_64-linux-android \
    i686-linux-android

# # Download ALVR source
# rm -r "ALVR"
# curl -sLS "https://github.com/alvr-org/ALVR/archive/refs/heads/master.zip" > download.zip
# # curl -sLS "https://github.com/ShootingKing-AM/ALVR/archive/refs/heads/phonevr.zip" > download.zip
# unzip download.zip
# rm download.zip
# mv ALVR-master ALVR
# # mv ALVR-phonevr ALVR

pushd ALVR
cargo update
cargo xtask prepare-deps --platform android $@
popd

rm -r cardboard

# Download sdk source$
CARB_REPO_NAME="cardboard-master"
rm -r "${CARB_REPO_NAME}"
# curl -sLS "https://github.com/googlevr/cardboard/archive/refs/heads/master.zip" > download.zip
curl -sLS "https://github.com/nift4/cardboard/archive/refs/heads/master.zip" > download.zip
unzip download.zip
rm download.zip

# Build sdk
pushd "${CARB_REPO_NAME}"
./gradlew sdk:assembleRelease -Parm64-v8a
popd

# Prepare files
mkdir cardboard
mv "${CARB_REPO_NAME}/sdk/build/outputs/aar/sdk-release.aar" cardboard/cardboard-sdk.aar
cp "${CARB_REPO_NAME}/sdk/include/cardboard.h" cardboard/cardboard.h
rm -r "${CARB_REPO_NAME}"

rm -r "gvr-android-sdk-1.200"
if [ ! $nogvr ]; then
curl -sLS "https://github.com/googlevr/gvr-android-sdk/releases/download/v1.200/gvr-android-sdk-1.200.zip" > download.zip
unzip download.zip
rm download.zip
fi

