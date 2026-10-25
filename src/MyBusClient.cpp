#include "MyBusClient.h"

#include "TransitTlsTrust.h"
#include "TransitJsonParsers.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <MD5Builder.h>
#include <WiFiClientSecure.h>

namespace {

String toArduino(const std::string& value) { return String(value.c_str()); }

String md5Hex(const std::string& input) {
    MD5Builder md5;
    md5.begin();
    md5.add(input.c_str());
    md5.calculate();
    return md5.toString();
}

int parseStationOrder(const std::string& stopId) {
    int order = 0;
    for (const char character : stopId) {
        if (character < '0' || character > '9') {
            return 0;
        }
        if (order > 99999) {
            return 0;
        }
        order = order * 10 + (character - '0');
    }
    return order;
}

}  // namespace

bool MyBusClient::httpPostForm(const String& url, const String& formData, String& body, String& error) {
    WiFiClientSecure tls;
    transitink::configureMyBusVerifiedTls(tls);
    HTTPClient http;
    http.setTimeout(10000);
    http.setReuse(false);
    if (!http.begin(tls, url)) {
        error = "無法建立 HTTPS 連線";
        return false;
    }
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.addHeader("Token", mybus::kApiToken);
    const int code = http.POST(formData);
    if (code != HTTP_CODE_OK) {
        error = String("HTTP 錯誤: ") + code;
        http.end();
        return false;
    }
    body = http.getString();
    http.end();
    if (body.length() == 0) {
        // 服務端對拒簽請求回 200 + 空 body，必須視為錯誤而非「無數據」。
        error = "請求被拒（簽名校驗失敗）";
        return false;
    }
    return true;
}

bool MyBusClient::fetchRealtimeFrame(const transitink::BusWidgetConfig& config,
                                     int64_t nowEpoch,
                                     mybus::RealtimeFrame& frame,
                                     String& error) {
    frame = mybus::RealtimeFrame{};
    if (config.operatorId != transitink::BusOperator::MyBus) {
        error = "巴士營辦商設定不正確";
        return false;
    }
    if (!mybus::validDirection(config.directionId)) {
        error = "巴士營辦商設定不正確";
        return false;
    }

    const uint64_t nowMs = static_cast<uint64_t>(nowEpoch) * 1000ULL;
    const String timestamp = String(static_cast<unsigned long>(nowMs));
    const std::string digest = md5Hex(mybus::signInput("104", timestamp.c_str())).c_str();
    const std::string formData =
        mybus::realtimeFormBody(config.routeId, config.directionId, config.serviceType,
                                timestamp.c_str(), digest);

    String body;
    if (!httpPostForm(toArduino(mybus::apiEndpointBaseUrl()), toArduino(formData), body,
                      error)) {
        return false;
    }

    std::string parserError;
    if (!parseMyBusRealtimeJson(body.c_str(), frame, parserError)) {
        error = parserError.c_str();
        return false;
    }
    error = "";
    return true;
}

bool MyBusClient::fetchRouteStationsJson(const transitink::BusWidgetConfig& config,
                                         int64_t nowEpoch,
                                         String& body,
                                         String& error) {
    if (config.operatorId != transitink::BusOperator::MyBus) {
        error = "巴士營辦商設定不正確";
        return false;
    }
    if (nowEpoch <= 0) {
        error = "裝置時鐘未同步";
        return false;
    }
    const uint64_t nowMs = static_cast<uint64_t>(nowEpoch) * 1000ULL;
    const String timestamp = String(static_cast<unsigned long>(nowMs));
    const std::string digest = md5Hex(mybus::signInput("103", timestamp.c_str())).c_str();
    std::vector<std::pair<std::string, std::string>> params;
    params.emplace_back("LINENAME", config.routeId);
    params.emplace_back("DIRECTION", config.directionId);
    params.emplace_back("CITYNAME", config.serviceType);
    params.emplace_back("SIGN", digest);
    const std::string formData = mybus::apiFormBody("103", timestamp.c_str(), params);

    if (!httpPostForm(toArduino(mybus::apiEndpointBaseUrl()), toArduino(formData), body,
                      error)) {
        return false;
    }
    error = "";
    return true;
}

bool MyBusClient::fetchEtaRecords(const transitink::BusWidgetConfig& config,
                                  int64_t nowEpoch,
                                  std::vector<transitink::BusEtaRecord>& records,
                                  String& error) {
    records.clear();
    const int stationOrder = parseStationOrder(config.stopId);
    if (stationOrder < 1) {
        error = "巴士站序設定不正確";
        return false;
    }
    if (nowEpoch <= 0) {
        error = "裝置時鐘未同步";
        return false;
    }

    mybus::RealtimeFrame frame;
    if (!fetchRealtimeFrame(config, nowEpoch, frame, error)) {
        records.clear();
        return false;
    }

    const std::vector<mybus::EtaRecord> etas = mybus::etaRecordsToStation(frame, stationOrder);
    for (const auto& eta : etas) {
        transitink::BusEtaRecord record;
        record.operatorId = transitink::BusOperator::MyBus;
        record.routeId = config.routeId;
        record.directionId = config.directionId;
        record.serviceType = config.serviceType;
        record.eventEpoch = nowEpoch + static_cast<int64_t>(eta.minutes) * 60;
        record.remarkTc = eta.remark.empty() ? "" : eta.remark;
        record.destinationLabelTc = "";
        records.push_back(std::move(record));
    }
    error = "";
    return true;
}
