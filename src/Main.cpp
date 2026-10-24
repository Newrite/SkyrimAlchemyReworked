#include "Config.h"
#include "Distributor.h"

namespace {
    class KIDEventHandler final : public RE::BSTEventSink<SKSE::ModCallbackEvent> {
    public:
        static KIDEventHandler* GetSingleton() {
            static KIDEventHandler handler;
            return &handler;
        }

        RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* event,
                                              RE::BSTEventSource<SKSE::ModCallbackEvent>*) override {
            if (event && event->eventName == "KID_KeywordDistributionDone") {
                SKSE::GetTaskInterface()->AddTask([] { AlchmeyDistributor::Initialize(); });
                SKSE::GetModCallbackEventSource()->RemoveEventSink(this);
            }
            return RE::BSEventNotifyControl::kContinue;
        }
    };

    void InitializeLogging() {
        auto path = SKSE::log::log_directory();
        if (!path) SKSE::stl::report_and_fail("Unable to locate SKSE log directory");
        *path /= "AlchemyReworked.log";
        auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
        auto logger = std::make_shared<spdlog::logger>("AlchemyReworked", sink);
        logger->set_level(spdlog::level::info);
        logger->flush_on(spdlog::level::info);
        spdlog::set_default_logger(std::move(logger));
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] [%s:%#] %v");
    }
}

SKSE_PLUGIN_LOAD(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse, {.log = false});
    InitializeLogging();
    try {
        const auto& config = Config::GetSingleton();
        spdlog::set_level(spdlog::level::from_str(config.debug.logLevel));
        spdlog::flush_on(spdlog::level::from_str(config.debug.flushLevel));
    } catch (const std::exception& error) {
        SKSE::log::critical("Invalid AlchemyReworked.yaml: {}", error.what());
        return false;
    }
    SKSE::log::info("AlchemyReworked 1.3.0 loaded; waiting for KID distribution");
    SKSE::GetModCallbackEventSource()->AddEventSink(KIDEventHandler::GetSingleton());
    return SKSE::GetMessagingInterface()->RegisterListener([](SKSE::MessagingInterface::Message* message) {
        if (!message) return;
        if (message->type == SKSE::MessagingInterface::kPreLoadGame ||
            message->type == SKSE::MessagingInterface::kNewGame) {
            AlchmeyDistributor::ResetSession();
        }
    });
}
