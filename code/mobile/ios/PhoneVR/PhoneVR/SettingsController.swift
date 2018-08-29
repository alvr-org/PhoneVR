
class SettingsController: UIViewController {
    @IBOutlet weak var pcIPField: UITextField!
    @IBOutlet weak var modeField: UITextField!
    
    override func viewWillAppear(_ animated: Bool) {
        modeField.text = String(defs.integer(forKey: "mode"))
        pcIPField.text = defs.string(forKey: pcIPKey)
    }
    
    override func viewDidDisappear(_ animated: Bool) {
        defs.set(Int(modeField.text!)!, forKey: "mode")
        defs.set(pcIPField.text, forKey: pcIPKey)
        pvrDisplMode = Int32(defs.integer(forKey: "mode"))
    }

}
