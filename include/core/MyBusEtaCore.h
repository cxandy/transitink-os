#pragma once

// 掌上公交（WxBusServer）線路 ETA 純邏輯層。
// 完全不依賴 Arduino：可被 host 測試直接編譯（tests/test_core.py 模式）。
// 數據來源（逆向自 h5new.mygolbs.com 前端 JS）：
//   端點  https://wx.mygolbs.com/WxBusServer/ApiData.do（僅 HTTPS，HTTP 404）
//   方法  POST，x-www-form-urlencoded
//   Header  Token: zsgjh520220614
//   簽名  SIGN = MD5( CMD + "#" + TIMESTAMP + "#zsgjwxapi01" )，TIMESTAMP 為 13 位毫秒
//   拒簽  服務端回 HTTP 200 + 空 body（不是 4xx）

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace mybus {

constexpr const char* kApiToken = "zsgjh520220614";
constexpr const char* kSignKey = "zsgjwxapi01";
// 站間平均車速備用值（m/min，約 30 km/h）；speedlist 缺項或長度不足時使用。
constexpr double kDefaultMetersPerMinute = 500.0;
// 依抓包觀察，定位漂移車輛的 busToStationNiheDistance 可達 27 km 以上。
constexpr double kMaxPlausibleVehicleDistance = 30000.0;

// 城市名（CITYNAME）、線路名（LINENAME）、方向（upperOrDown "1"/"2"）、目標站序。
std::string apiEndpointBaseUrl();
std::string apiFormBody(const std::string& cmd,
                        const std::string& timestamp,
                        const std::vector<std::pair<std::string, std::string>>& params);
std::string signInput(const std::string& cmd, const std::string& timestamp);
std::string realtimeFormBody(const std::string& routeName,
                             const std::string& direction,
                             const std::string& cityName,
                             const std::string& timestamp);
bool validDirection(const std::string& value);

struct RealtimeVehicle {
    int index = -1;                  // 前方站 0 基序號（站列 stationOrder-1）
    double toStationMeters = -1.0;   // busToStationNiheDistance（米）
    std::string busNumber;           // 車牌，如 浙B37039D
};

// CMD=104 解析後的時間幀。dislist[j] = 站 j → 站 j+1 距離（米，末項通常為 0），
// speedlist[j] = 第 j 段速度（m/min，長度常為 dislist-1，末段缺項）。
struct RealtimeFrame {
    bool hasReal = false;
    int runState = 0;
    std::string planTime;
    std::vector<RealtimeVehicle> vehicles;
    std::vector<double> dislist;
    std::vector<double> speedlist;
};

struct EtaRecord {
    int minutes = -1;        // 0 = 下一分鐘內（即將到站）
    std::string remark;      // 車牌
};

// 依站序（1 基）估算到站時間。站序超出既有站列、車已過站、或推算不出時
// 回 reachable=false。多車會依距離由近到遠從 vehicles 中分別推算。
std::vector<EtaRecord> etaRecordsToStation(const RealtimeFrame& frame, int stationOrder);

}  // namespace mybus
