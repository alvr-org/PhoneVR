
class GameController: GLKViewController, GLKViewControllerDelegate, GVROverlayViewDelegate {
    
    override var supportedInterfaceOrientations: UIInterfaceOrientationMask { return .landscapeRight }
    
    override func viewDidLoad() {
        super.viewDidLoad()
        gameCtrl = self
        delegate = self
        
        
        let overlay = GVROverlayView(frame: view.bounds)
        overlay.autoresizingMask = [.flexibleWidth, .flexibleHeight]
        overlay.delegate = self
        view.addSubview(overlay)
        let glkView = view as! GLKView
        glkView.context = EAGLContext(api: .openGLES3)
        preferredFramesPerSecond = 60
        EAGLContext.setCurrent(glkView.context)
        glkView.bindDrawable()
        
        PVRCreateGVRAndContext()
        startIOStreams()
        PVRInitRenderer()
    }
    
    override func viewDidDisappear(_ animated: Bool) {
        super.viewDidDisappear(animated);
        PVRDestroyGVR()
        mm.stopDeviceMotionUpdates()
    }
    
    func startIOStreams() {
        Thread{
            PVRInitReceiveStreams(UInt16(defs.integer(forKey: videoPortKey))) {
                gameCtrl.performSegue(withIdentifier: "requestUnwind", sender: gameCtrl)
            }
            // pvrFrameWidth[0] = pvrFrameWidth[1] = pvrFrameWidth[2] = pvrFrameHeight[0] = pvrFrameHeight[1] = pvrFrameHeight[2] = 100
            print("end of stream")
        }.start()
        
        mm.deviceMotionUpdateInterval = 0.008 // ~120fps
        mm.startDeviceMotionUpdates()
        Thread{
            PVRStartSendSensorData(defs.string(forKey: pcIPKey)!, Int32(defs.integer(forKey: orPortKey))) { orientPtr, accPtr in
                if let dm = mm.deviceMotion {
                    let quat = dm.attitude.quaternion
                    let acc = dm.userAcceleration
                    memcpy(orientPtr, [Float(quat.w), Float(quat.x), Float(quat.y), Float(quat.z)], 4 * 4)
                    memcpy(accPtr, [Float(acc.x), Float(acc.y), Float(acc.z)], 4 * 4)
                    return 1
                }
                return 0
            }
        }.start()
    }
    
    override func glkView(_ view: GLKView, drawIn rect: CGRect) {
        super.glkView(view, drawIn: rect)
        PVRRender()
    }
    
    func glkViewControllerUpdate(_ controller: GLKViewController) { }
    
    func glkViewController(_ controller: GLKViewController, willPause pause: Bool) {
        
        if pause {
            PVRPause()
        } else {
            PVRResume()
        }
    }
    
    @IBAction func didTapView(_ sender: UITapGestureRecognizer) {
        PVRTrigger()
    }
    
    func didTapBackButton() {
        self.performSegue(withIdentifier: "requestUnwind", sender: self)
    }
    
    func didPresentSettingsDialog(_ presented: Bool) {
        isPaused = presented
    }
    
    func didChangeViewerProfile() {
        PVRResume()
    }
    
    func shouldDisableIdleTimer(_ shouldDisable: Bool) {
        UIApplication.shared.isIdleTimerDisabled = shouldDisable
    }
}

var gameCtrl: GameController! = nil

