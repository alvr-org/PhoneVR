
import CoreMotion
let mm = CMMotionManager()

let defs = UserDefaults.standard


// Keys for UserDefaults
var notFirstRunKey = "not_first_run"// I need it negated because of how UserDefaults work
var pcIPKey = "PC_IP"
var videoPortKey = "video_stream_port"
var orPortKey = "pose_stream_port"

//Default values for UserDefaults
var pcIPDef = ""
var videoPortDef = 15243
var orPortDef = 51423

//audio
import AudioToolbox

//var audioCallback: @convention(c)(UnsafeMutablePointer<()>, AudioQueueRef, AudioQueueBufferRef) -> () = { _, aq, buff in
//
//}

func audioCallback(inUserData: UnsafeMutableRawPointer?, aq: AudioQueueRef, buff: AudioQueueBufferRef) {
	if pvrAFrameReady == 1 {
        memcpy(buff.pointee.mAudioData, pvrAFrame.0, 2304)
        //memcpy(&(buff.memory.mAudioData[2304]), mlAFrame.1, 2304)
		pvrAFrameReady = 0
	}
	// let a =
	// let a = rand()
	// let arr = [Int32](count: 4096 / 4, repeatedValue: rand());
	// memcpy(buff.memory.mAudioData, arr, 4096)

	AudioQueueEnqueueBuffer(aq, buff, 0, nil)
	// print("////////////////////////////////////////////////")
}

var whichBuff = 1
//var aq = AudioQueueRef(bitPattern: ())
var aq = AudioQueueRef(bitPattern: 0)

func startPlayback() {
	var desc = AudioStreamBasicDescription()
	desc.mSampleRate = 22050//44100
	desc.mFormatID = kAudioFormatLinearPCM
	desc.mBytesPerPacket = 4// 418
	desc.mFramesPerPacket = 1
	desc.mBytesPerFrame = 4
	desc.mChannelsPerFrame = 2
	desc.mBitsPerChannel = 16
	desc.mFormatFlags = kAudioFormatFlagsCanonical// kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;//kAudioFormatFlagIsBigEndian | kAudioFormatFlagIsAlignedHigh//

    
	AudioQueueNewOutput(&desc, audioCallback, nil, nil, nil, 0, &aq)

	addBuffer(aq!) // add 2 slots
	addBuffer(aq!)

	AudioQueueSetParameter(aq!, kAudioQueueParam_Volume, 1.0)

	AudioQueueStart(aq!, nil)
	// var val: Float = 0.5
//	val = 0.0
//	AudioQueueGetParameter(aq, kAudioQueueProperty_CurrentLevelMeter, &val)
}

func addBuffer(_ aq: AudioQueueRef) {
	var buff = AudioQueueBufferRef(bitPattern: 0)
	AudioQueueAllocateBuffer(aq, 4608, &buff)
	buff!.pointee.mAudioDataByteSize = 2304//4608
	AudioQueueEnqueueBuffer(aq, buff!, 0, nil)
}

func stopPlayback() {
	AudioQueueStop(aq!, false)
}

