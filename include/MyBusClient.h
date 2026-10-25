#pragma once

#include <Arduino.h>

#include <string>
#include <utility>
#include <vector>

#include "core/MyBusEtaCore.h"
#include "core/WidgetCore.h"

// 掌上公交（WxBusServer）线路实时数据客户端。
// 字段承载映射约定（写入 config 时保持此约定）：
//   config.routeId      <- LINENAME（如 滨海开发区专线）
//   config.directionId  <- upperOrDown "1"/"2"
//   config.serviceType  <- CITYNAME（如 慈溪市）
//   config.stopId       <- 目标站序（1 基十进制字符串，来自路线站点列表 stationOrder）

class MyBusClient {
public:
    bool fetchRouteStationsJson(const transitink::BusWidgetConfig& config,
                                int64_t nowEpoch,
                                String& body,
                                String& error);
    bool fetchRealtimeFrame(const transitink::BusWidgetConfig& config,
                            int64_t nowEpoch,
                            mybus::RealtimeFrame& frame,
                            String& error);
    bool fetchEtaRecords(const transitink::BusWidgetConfig& config,
                         int64_t nowEpoch,
                         std::vector<transitink::BusEtaRecord>& records,
                         String& error);

private:
    bool httpPostForm(const String& url, const String& formData, String& body, String& error);
};
