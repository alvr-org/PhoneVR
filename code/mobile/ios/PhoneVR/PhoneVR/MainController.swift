
import UIKit


class MainController: UIViewController {
    

    override var supportedInterfaceOrientations: UIInterfaceOrientationMask { return .portrait }

	override func viewDidLoad() {
		super.viewDidLoad()
        mainCtrl = self;
        
		if !defs.bool(forKey: notFirstRunKey) {
			defs.set(pcIPDef, forKey: pcIPKey)
			defs.set(videoPortDef, forKey: videoPortKey)
            defs.set(orPortDef, forKey: orPortKey)
            defs.set(2, forKey: "mode")
			defs.set(true, forKey: notFirstRunKey)
        }
        pvrDisplMode = Int32(defs.integer(forKey: "mode"))
	}
    
    override func viewDidAppear(_ animated: Bool) {
        
        PVRStartSendDiscoverData(defs.string(forKey: pcIPKey), Int32(defs.integer(forKey: orPortKey))) {
            mainCtrl.performSegue(withIdentifier: "toGameView", sender: mainCtrl)
        }
    }
    
    override func viewDidDisappear(_ animated: Bool) {
        PVRStopSendDiscoverData()
    }

	@IBAction func goBack(_ segue: UIStoryboardSegue) { }

}

var mainCtrl: MainController! = nil // avoid capturing context in callback
