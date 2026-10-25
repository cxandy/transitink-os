// MyBus 營辦商的 BusProvider 路由/錯誤處理 host 測試。
// 編譯方式見 tests/test_core.py::test_mybus_provider_cpp_behaviors。

#include "providers/BusProvider.h"

#include <cassert>
#include <string>
#include <vector>

namespace {

int gKmbCalls = 0;
int gCitybusCalls = 0;
int gTflCalls = 0;
int gMyBusCalls = 0;
int64_t gMyBusNowEpoch = 0;
bool gMyBusResult = true;
String gMyBusError = "";
std::vector<transitink::BusEtaRecord> gMyBusRecords;

transitink::WidgetConfig myBusConfig() {
    transitink::WidgetConfig config;
    config.type = transitink::WidgetType::BusEta;
    config.bus.operatorId = transitink::BusOperator::MyBus;
    config.bus.routeId = "滨海开发区专线";
    config.bus.directionId = "1";
    config.bus.serviceType = "慈溪市";
    config.bus.stopId = "6";
    config.bus.routeLabelTc = "滨海开发区专线";
    config.bus.stopLabelTc = "横新塘";
    config.bus.destinationLabelTc = "往龙山总站";
    return config;
}

transitink::BusEtaRecord myBusRecord(int64_t eventEpoch) {
    transitink::BusEtaRecord record;
    record.operatorId = transitink::BusOperator::MyBus;
    record.routeId = "滨海开发区专线";
    record.directionId = "1";
    record.serviceType = "慈溪市";
    record.eventEpoch = eventEpoch;
    record.remarkTc = "浙B37039D";
    return record;
}

}  // namespace

bool KmbClient::fetchEtaRecords(const transitink::BusWidgetConfig&,
                                std::vector<transitink::BusEtaRecord>&,
                                String&) {
    ++gKmbCalls;
    return false;
}

bool CitybusClient::fetchEtaRecords(const transitink::BusWidgetConfig&,
                                    std::vector<transitink::BusEtaRecord>&,
                                    String&) {
    ++gCitybusCalls;
    return false;
}

bool TflClient::fetchEtaRecords(const transitink::BusWidgetConfig&, int64_t,
                                std::vector<transitink::BusEtaRecord>&,
                                String&) {
    ++gTflCalls;
    return false;
}

bool MyBusClient::fetchEtaRecords(const transitink::BusWidgetConfig&, int64_t nowEpoch,
                                  std::vector<transitink::BusEtaRecord>& records,
                                  String& error) {
    ++gMyBusCalls;
    gMyBusNowEpoch = nowEpoch;
    records = gMyBusRecords;
    error = gMyBusError;
    return gMyBusResult;
}

int main() {
    constexpr int64_t now = 2000000000;

    {
        gMyBusCalls = 0;
        gMyBusResult = true;
        gMyBusError = "";
        gMyBusRecords = {myBusRecord(now + 5 * 60), myBusRecord(now + 12 * 60)};

        KmbClient kmb;
        CitybusClient citybus;
        TflClient tfl;
        MyBusClient mybus;
        BusProvider provider(kmb, citybus, tfl, mybus);

        const transitink::WidgetConfig config = myBusConfig();
        const auto result = provider.fetch(2, config, now);
        assert(gMyBusCalls == 1);
        assert(gKmbCalls == 0 && gCitybusCalls == 0 && gTflCalls == 0);
        assert(gMyBusNowEpoch == now);
        assert(result.outcome == transitink::ProviderOutcome::Success);
        assert(result.snapshot.slot == 2);
        assert(result.snapshot.state == transitink::WidgetState::Ready);
        assert(result.snapshot.valueCount == 2);
        assert(result.snapshot.values[0].eventEpoch == now + 5 * 60);
        assert(result.snapshot.values[1].eventEpoch == now + 12 * 60);
    }

    {
        // 客戶端失敗並帶錯誤訊息 → Failure 且 providerMessage 使用該訊息。
        gMyBusCalls = 0;
        gMyBusResult = false;
        gMyBusError = "請求被拒（簽名校驗失敗）";
        gMyBusRecords.clear();

        KmbClient kmb;
        CitybusClient citybus;
        TflClient tfl;
        MyBusClient mybus;
        BusProvider provider(kmb, citybus, tfl, mybus);

        const transitink::WidgetConfig config = myBusConfig();
        const auto result = provider.fetch(3, config, now);
        assert(gMyBusCalls == 1);
        assert(result.outcome == transitink::ProviderOutcome::Failure);
        assert(result.snapshot.state == transitink::WidgetState::Error);
        assert(result.snapshot.providerMessage == "請求被拒（簽名校驗失敗）");
    }

    {
        // 設定不完整 → 在呼叫客戶端之前回 InvalidConfig。
        gMyBusCalls = 0;
        gMyBusResult = true;
        gMyBusRecords = {myBusRecord(now + 60)};

        KmbClient kmb;
        CitybusClient citybus;
        TflClient tfl;
        MyBusClient mybus;
        BusProvider provider(kmb, citybus, tfl, mybus);

        transitink::WidgetConfig config = myBusConfig();
        config.bus.stopId.clear();
        const auto invalid = provider.fetch(0, config, now);
        assert(gMyBusCalls == 0);
        assert(invalid.outcome == transitink::ProviderOutcome::InvalidConfig);
        assert(invalid.snapshot.state == transitink::WidgetState::Error);

        // 時鐘未同步 → ClockUnsynced，同樣不呼叫客戶端。
        const auto unsynced = provider.fetch(0, myBusConfig(), 0);
        assert(gMyBusCalls == 0);
        assert(unsynced.outcome == transitink::ProviderOutcome::ClockUnsynced);
    }

    {
        // 客戶端成功但無可用記錄 → Empty。
        gMyBusCalls = 0;
        gMyBusResult = true;
        gMyBusError = "";
        gMyBusRecords.clear();

        KmbClient kmb;
        CitybusClient citybus;
        TflClient tfl;
        MyBusClient mybus;
        BusProvider provider(kmb, citybus, tfl, mybus);

        const auto result = provider.fetch(1, myBusConfig(), now);
        assert(gMyBusCalls == 1);
        assert(result.outcome == transitink::ProviderOutcome::Empty);
        assert(result.snapshot.state == transitink::WidgetState::Empty);
        assert(result.snapshot.valueCount == 0);
    }

    return 0;
}
