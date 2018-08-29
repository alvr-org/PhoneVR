header:
//enum PKT_TAG {
//    PKT_TAG_INVALID = 0,
//    PKT_TAG_DEV_IP = 2,
//    PKT_TAG_ORIENTATION = 3,
//    PKT_TAG_ACCELEROMETER = 4,
//    PKT_TAG_GYROSCOPE = 5,
//    PKT_TAG_COMPASS = 6,
//    PKT_TAG_TIME = 7,
//    PKT_TAG_ACK = 8,
//    PKT_TAG_FOV = 9,
//    PKT_TAG_CLIENT_VERSION = 10,
//    PKT_TAG_SERVER_VERSION = 11,
//    // add more cardboard profile info
//};

//const std::map<PKT_TAG, size_t> Pkt_tag_size_dict = { { PKT_TAG_INVALID, 0 }, { PKT_TAG_CLIENT_VERSION, 16 }, { PKT_TAG_SERVER_VERSION, 16 }, { PKT_TAG_ORIENTATION, 16 },
//                                                { PKT_TAG_ACCELEROMETER, 12 }, { PKT_TAG_TIME, 8 }, { PKT_TAG_ACK, 1 }, { PKT_TAG_DEV_IP, 64 } };

//template <typename T>
//std::vector<T> byteVecToVec(std::vector<uint8_t> inVec) {

//}

//template<typename In, typename Out>
//Out *


//class Packet {
//    //std::vector<std::pair<PKT_TAG, std::vector<uint8_t>>> bufVec;
//    std::map<uint32_t, PKT_TAG> tagMap;
//    std::vector<uint8_t> *bufVec;
//    bool owning;
//public:

//    // encode packet
//    Packet() : bufVec(new std::vector<uint8_t>()), owning(true) {}

//    // decode packet or prealloc
//    Packet(std::vector<uint8_t> &data, vector<PKT_TAG> tags);

//    ~Packet() {
//        if (owning)
//            delete bufVec;
//    }

//    std::vector<uint8_t> &getPacketBuffer() {
//        return *bufVec;
//    }

//    //template code must reside in header files

//    //WARINING: in VS, templates hide all errors, even missing semicolons
//    template<typename T, typename std::enable_if<std::is_fundamental<T>::value>::type* = nullptr> // SFINAE specialization
//    bool enqueue(std::vector<T> &data, PKT_TAG tag) { // true -> success
//        size_t size = Pkt_tag_size_dict.find(tag);
//        if (size >= data.size() * sizeof(T)) {
//            std::vector<uint8_t> buf(it->second);
//            memcpy(&buf[0], &data[0], data.size() * sizeof(T));
//            bufVec.insert({ tag, buf });
//            return true;
//        }
//        return false;
//    }
//    /*
//            template<typename T>
//            T *getPtrAt
//    */
//    template<typename T, typename std::enable_if<std::is_fundamental<T>::value>::type* = nullptr>
//    std::vector<T> getItem(PKT_TAG tag, bool byRef = false) {
//        auto it = bufVec.find(tag);
//        if (it != bufVec.end()) {
//            auto buf = it->second;
//            if (buf.size() % sizeof(T) == 0) {
//                std::vector<T> newBuf(buf.size() / sizeof(T));
//                memcpy(&newBuf[0], &buf[0], buf.size());
//                return newBuf;
//            }
//        }
//        return std::vector<T>();
//    }


//};

src:

//Packet:

//Packet::Packet(vector<uint8_t> &data, vector<PKT_TAG> tags) : owning(false) {
//    bufVec = &data;
//    uint32_t c = 0;
//    for (auto tag : tags) {
//        tagMap.insert({ c, tag });
//        c += Pkt_tag_size_dict.at(tag);
//    }
//    /*auto datait = data.begin();
//    for (auto tag : tags) {
//        if (tag != PKT_TAG_INVALID) {
//            auto it = Pkt_tag_size_dict.find(tag);
//            if (it != Pkt_tag_size_dict.end()) {
//                size_t size = it->second;
//                if (size == -1) {
//                    bufVec.push_back({ tag, vector<uint8_t>(datait, data.end()) });
//                    return;
//                }
//                else if (datait + size != data.end()) {
//                    bufVec.push_back({ tag, vector<uint8_t>(datait, datait + size) });
//                    datait += size;
//                }
//            }
//        }
//    }*/
//}

//vector<uint8_t> &Packet::getPacketBuffer() {
//    //vector<uint8_t> buf;
//    //for (auto p : bufVec)
//    //    //buf.push_back(p.second); //for some reasons this does not work
//    //    buf.insert(buf.end(), p.second.begin(), p.second.end());
//    //return buf;
//    return 
//}