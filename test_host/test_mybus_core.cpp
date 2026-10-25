// 掌上公交（MyBus/WxBusServer）純邏輯 host 測試。
// 編譯方式見 tests/test_core.py::test_mybus_core_cpp_behaviors。

#include "core/MyBusEtaCore.h"

#include <cassert>
#include <string>
#include <vector>

namespace {

using mybus::EtaRecord;
using mybus::RealtimeFrame;
using mybus::RealtimeVehicle;

RealtimeFrame frameWith(std::vector<RealtimeVehicle> vehicles,
                        std::vector<double> dislist,
                        std::vector<double> speedlist,
                        bool hasReal = true) {
    RealtimeFrame frame;
    frame.hasReal = hasReal;
    frame.planTime = "09:30";
    frame.vehicles = std::move(vehicles);
    frame.dislist = std::move(dislist);
    frame.speedlist = std::move(speedlist);
    return frame;
}

// 綜合場景：8 站路線（對照 104_realtime.json fixture：dislist len 8、末項 0、
// speedlist len 7、車輛 index 0 基）。目標站序 6（0 基 5）。
void normalMultiSegment() {
    RealtimeVehicle vehicle;
    vehicle.index = 0;
    vehicle.toStationMeters = 500.0;
    vehicle.busNumber = "浙B37039D";
    RealtimeFrame frame = frameWith({vehicle},
                                    {1000.0, 2000.0, 3000.0, 4000.0, 5000.0,
                                     6000.0, 7000.0, 0.0},
                                    {450.0, 500.0, 600.0, 700.0, 800.0, 900.0, 1000.0});
    const std::vector<EtaRecord> records = mybus::etaRecordsToStation(frame, 6);
    assert(records.size() == 1);
    // 時間 = 進入段 500/500(缺速取默認) + 1000/450 + 2000/500 + 3000/600
    //        + 4000/700 + 5000/800 = 24.19 → ceil = 25
    assert(records[0].minutes == 25);
    assert(records[0].remark == "浙B37039D");
}

// 車已壓站：距目標站不足 10 米 → 1 分鐘已定。
void frontStationArriving() {
    RealtimeVehicle near;
    near.index = 5;
    near.toStationMeters = 8.0;
    near.busNumber = "浙B00001D";
    RealtimeFrame frame = frameWith({near}, {1000.0}, {500.0});
    const std::vector<EtaRecord> records = mybus::etaRecordsToStation(frame, 6);
    assert(records.size() == 1);
    assert(records[0].minutes == 1);
}

// 車在前方站但還有距離：speedlist 缺項（車已過最後一個速度取樣段）→ 用默認 500 m/min。
void defaultSpeedWhenSpeedlistShort() {
    RealtimeVehicle vehicle;
    vehicle.index = 5;
    vehicle.toStationMeters = 261.0;  // ceil(261/500) = 1
    RealtimeFrame frame = frameWith({vehicle}, {1000.0, 2000.0, 0.0}, {});
    const std::vector<EtaRecord> records = mybus::etaRecordsToStation(frame, 6);
    assert(records.size() == 1);
    assert(records[0].minutes == 1);

    vehicle.toStationMeters = 1500.0;  // ceil(1500/500) = 3
    RealtimeFrame slowerFrame = frameWith({vehicle}, {1000.0, 2000.0, 0.0}, {});
    const std::vector<EtaRecord> slower = mybus::etaRecordsToStation(slowerFrame, 6);
    assert(slower.size() == 1);
    assert(slower[0].minutes == 3);
}

// 幽靈車（>30 km）與已過站車輛都必須跳過。
void skipGhostAndPassed() {
    RealtimeVehicle ghost;
    ghost.index = 0;
    ghost.toStationMeters = 45000.0;
    ghost.busNumber = "幽靈";
    RealtimeVehicle passed;
    passed.index = 7;  // targetIndex = 5，車 front 已過站
    passed.toStationMeters = 100.0;
    passed.busNumber = "過站";
    RealtimeFrame frame = frameWith({ghost, passed}, {1000.0}, {500.0});
    assert(mybus::etaRecordsToStation(frame, 6).empty());
}

// dislist 缺段時保守拒絕（widely observed server length mismatch）。
void refuseWhenDislistShort() {
    RealtimeVehicle vehicle;
    vehicle.index = 0;
    vehicle.toStationMeters = 100.0;
    RealtimeFrame frame = frameWith({vehicle}, {1000.0}, {500.0});
    assert(mybus::etaRecordsToStation(frame, 6).empty());
}

// hasReal=0（尾班車已過 / 未開線）→ 空結果。
void noRealtimeSeparator() {
    RealtimeVehicle vehicle;
    vehicle.index = 0;
    vehicle.toStationMeters = 500.0;
    RealtimeFrame frame = frameWith({vehicle}, {1000.0}, {500.0}, false);
    assert(mybus::etaRecordsToStation(frame, 6).empty());
}

// 多車排序 + 同分去重 + 最多 2 條。
void sortDedupeCap() {
    RealtimeVehicle first;
    first.index = 4;
    first.toStationMeters = 1000.0;
    first.busNumber = "浙B11111D";
    RealtimeVehicle second;
    second.index = 4;
    second.toStationMeters = 1000.0;
    second.busNumber = "浙B22222D";
    RealtimeVehicle third;
    third.index = 2;
    third.toStationMeters = 100.0;
    third.busNumber = "浙B33333D";
    RealtimeFrame frame =
        frameWith({first, second, third}, {100.0, 100.0, 100.0, 100.0, 100.0, 100.0},
                  {500.0, 500.0, 500.0, 500.0, 500.0, 500.0});
    // third 於站 2 距 100，經段 3 → 200/500 → 1 分鐘；first/second 於站 4（即目標站）
    // 距 1000，speedlist 無對應段 → 默認 500 m/min → ceil(1000/500) = 2 分鐘。
    const std::vector<EtaRecord> records = mybus::etaRecordsToStation(frame, 5);
    assert(records.size() == 2);
    assert(records[0].minutes == 1);
    assert(records[1].minutes == 2);
    assert(records[1].remark == "浙B11111D");
}

void signAndForm() {
    assert(mybus::signInput("104", "1700000000000") ==
           "104#1700000000000#zsgjwxapi01");
    assert(mybus::validDirection("1"));
    assert(mybus::validDirection("2"));
    assert(!mybus::validDirection(""));
    assert(!mybus::validDirection("3"));

    const std::string body = mybus::apiFormBody(
        "104", "1700000000000",
        {{"LINENAME", "滨海线"}, {"SIGN", "abc123"}});
    // UTF-8 中文按位元組轉義：「滨」= E6 BB A8、「海」= E6 B5 B7、「线」= E7 BA BF。
    assert(body ==
           "CMD=104&TIMESTAMP=1700000000000&LINENAME=%E6%BB%A8%E6%B5%B7%E7%BA%BF"
           "&SIGN=abc123");
    assert(mybus::apiEndpointBaseUrl() ==
           "https://wx.mygolbs.com/WxBusServer/ApiData.do");
}

}  // namespace

int main() {
    normalMultiSegment();
    frontStationArriving();
    defaultSpeedWhenSpeedlistShort();
    skipGhostAndPassed();
    refuseWhenDislistShort();
    noRealtimeSeparator();
    sortDedupeCap();
    signAndForm();
    return 0;
}
