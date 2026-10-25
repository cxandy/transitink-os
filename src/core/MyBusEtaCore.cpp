#include "core/MyBusEtaCore.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace mybus {
namespace {

const double* speedAt(const std::vector<double>& speedlist, std::size_t index) {
    if (index >= speedlist.size()) return nullptr;
    const double speed = speedlist[index];
    if (speed <= 0.0) return nullptr;
    return &speedlist[index];
}

std::string formEncode(const std::string& value) {
    std::ostringstream encoded;
    for (const char character : value) {
        const unsigned char byte = static_cast<unsigned char>(character);
        if (std::isalnum(byte) || byte == '-' || byte == '_' || byte == '.' || byte == '*' || byte == '@') {
            encoded << character;
        } else if (byte == ' ') {
            encoded << '+';
        } else {
            encoded << '%' << std::uppercase << std::hex << std::setw(2)
                    << std::setfill('0') << static_cast<int>(byte)
                    << std::nouppercase << std::dec << std::setfill(' ');
        }
    }
    return encoded.str();
}

}  // namespace

std::string apiEndpointBaseUrl() { return "https://wx.mygolbs.com/WxBusServer/ApiData.do"; }

std::string apiFormBody(const std::string& cmd,
                        const std::string& timestamp,
                        const std::vector<std::pair<std::string, std::string>>& params) {
    std::ostringstream body;
    body << "CMD=" << formEncode(cmd) << "&TIMESTAMP=" << formEncode(timestamp);
    for (const auto& entry : params) {
        body << "&" << formEncode(entry.first) << "=" << formEncode(entry.second);
    }
    return body.str();
}

std::string signInput(const std::string& cmd, const std::string& timestamp) {
    return cmd + "#" + timestamp + "#" + kSignKey;
}

std::string realtimeFormBody(const std::string& routeName,
                             const std::string& direction,
                             const std::string& cityName,
                             const std::string& timestamp,
                             const std::string& signDigest) {
    std::vector<std::pair<std::string, std::string>> params;
    params.emplace_back("LINENAME", routeName);
    params.emplace_back("DIRECTION", direction);
    params.emplace_back("CITYNAME", cityName);
    params.emplace_back("SIGN", signDigest);
    return apiFormBody("104", timestamp, params);
}

bool validDirection(const std::string& value) {
    return value == "1" || value == "2";
}

std::vector<EtaRecord> etaRecordsToStation(const RealtimeFrame& frame, int stationOrder) {
    std::vector<EtaRecord> result;
    if (stationOrder < 1 || !frame.hasReal) {
        return result;
    }
    const int targetIndex = stationOrder - 1;  // 0 基站序
    for (const auto& vehicle : frame.vehicles) {
        const int front = vehicle.index;
        if (front < 0 || front > targetIndex) {
            continue;  // 车已过站（前方站序大于目标站）或数据异常
        }
        if (vehicle.toStationMeters < 0.0 ||
            vehicle.toStationMeters > kMaxPlausibleVehicleDistance) {
            continue;  // 定位漂移（抓包曾见 27 km 极端值）
        }

        // 車距前方站 front 的剩餘距離屬於「進入段」front-1（站 front-1 → 站 front），
        // 其後逐段 dislist[front..target-1] 對應 speedlist 同索引。
        if (front != targetIndex) {
            bool resolvable = true;
            for (int j = front; j < targetIndex; ++j) {
                if (static_cast<std::size_t>(j) >= frame.dislist.size()) {
                    resolvable = false;  // 服務端分段數據缺項，保守拒絕（文檔疑點 2）
                    break;
                }
            }
            if (!resolvable) {
                continue;
            }
        }

        const std::size_t approachSegment = front > 0 ? static_cast<std::size_t>(front - 1) : 0;
        const double approachSpeed = [&frame, approachSegment, front]() {
            if (front > 0) {
                if (const double* speed = speedAt(frame.speedlist, approachSegment)) {
                    return *speed;
                }
            }
            return kDefaultMetersPerMinute;
        }();
        auto segmentSpeed = [&frame](int j) {
            if (const double* speed = j >= 0 ? speedAt(frame.speedlist, static_cast<std::size_t>(j))
                                             : nullptr) {
                return *speed;
            }
            return kDefaultMetersPerMinute;
        };

        double minutes;
        if (front == targetIndex) {
            if (vehicle.toStationMeters <= 10.0) {
                minutes = 1.0;  // 車已壓站，下一分鐘
            } else {
                minutes = std::ceil(vehicle.toStationMeters / approachSpeed);
            }
        } else {
            double timeMinutes = vehicle.toStationMeters / approachSpeed;
            for (int j = front; j < targetIndex; ++j) {
                // toStationMeters 已屬於進入段（front-1），此段起用 dislist。
                timeMinutes += std::max(frame.dislist[static_cast<std::size_t>(j)], 0.0) /
                               segmentSpeed(j);
            }
            minutes = std::ceil(timeMinutes);
        }
        if (minutes < 0.0 || minutes > 180.0) {
            continue;  // 超过 3 小时的推算无展示价值
        }
        EtaRecord record;
        record.minutes = minutes < 1.0 ? 1 : static_cast<int>(minutes);
        record.remark = vehicle.busNumber;
        result.push_back(std::move(record));
    }

    std::stable_sort(result.begin(), result.end(),
                     [](const EtaRecord& a, const EtaRecord& b) {
                         return a.minutes < b.minutes;
                     });
    std::vector<std::string> seen;
    std::vector<EtaRecord> unique;
    for (auto& record : result) {
        std::string key = record.remark + "@" + std::to_string(record.minutes);
        if (std::find(seen.begin(), seen.end(), key) == seen.end()) {
            seen.push_back(std::move(key));
            unique.push_back(std::move(record));
        }
        if (unique.size() >= 2) {
            break;
        }
    }
    return unique;
}

}  // namespace mybus
