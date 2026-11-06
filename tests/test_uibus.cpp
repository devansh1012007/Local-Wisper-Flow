#include "core/UiBus.h"

#include <doctest/doctest.h>
#include <atomic>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

TEST_CASE("commands are delivered fifo without loss") {
    UiBus bus;
    bus.PostCommand(UiCommandType::OpenSettings, "a");
    bus.PostCommand(UiCommandType::Quit, "b");

    UiCommand command;
    REQUIRE(bus.PollCommand(command));
    CHECK(command.type == UiCommandType::OpenSettings);
    CHECK(command.payload == "a");
    REQUIRE(bus.PollCommand(command));
    CHECK(command.type == UiCommandType::Quit);
    CHECK_FALSE(bus.PollCommand(command));
}

TEST_CASE("concurrent producers never lose commands") {
    UiBus bus;
    constexpr int kProducers = 8;
    constexpr int kPerProducer = 1000;

    std::atomic<int> consumed{0};
    std::atomic<bool> stop{false};
    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&bus, p, kPerProducer] {
            for (int i = 0; i < kPerProducer; ++i) {
                bus.PostCommand(UiCommandType::SetEnabled,
                                std::to_string(p) + ":" + std::to_string(i));
            }
        });
    }
    std::thread consumer([&] {
        UiCommand command;
        while (!stop.load() || bus.PollCommand(command)) {
            if (bus.PollCommand(command)) {
                consumed.fetch_add(1);
            }
        }
    });

    for (std::thread& producer : producers) {
        producer.join();
    }
    while (consumed.load() < kProducers * kPerProducer) {
        UiCommand command;
        if (bus.PollCommand(command)) {
            consumed.fetch_add(1);
        }
    }
    stop.store(true);
    consumer.join();

    CHECK(consumed.load() == kProducers * kPerProducer);
}

TEST_CASE("snapshot reads are never torn while a writer publishes") {
    StatusSnapshot snapshot;
    std::atomic<bool> stop{false};
    std::atomic<bool> torn{false};

    std::thread writer([&] {
        for (int i = 1; i <= 20000; ++i) {
            StatusSnapshotValue value;
            value.error_counter = i;
            std::snprintf(value.error_message, sizeof(value.error_message), "error-%d", i);
            value.state = i % 6;
            snapshot.Publish(value);
        }
        stop.store(true);
    });

    std::thread reader([&] {
        while (!stop.load()) {
            const StatusSnapshotValue value = snapshot.Read();
            if (value.error_counter == 0) {
                continue;
            }
            const int expected = value.error_counter;
            char buffer[64];
            std::snprintf(buffer, sizeof(buffer), "error-%d", expected);
            if (std::string(value.error_message) != buffer) {
                torn.store(true);
            }
        }
    });

    writer.join();
    reader.join();
    CHECK_FALSE(torn.load());

    const StatusSnapshotValue final_value = snapshot.Read();
    CHECK(final_value.error_counter == 20000);
    CHECK(std::string(final_value.error_message) == "error-20000");
}

TEST_CASE("snapshot publish and read round trip") {
    StatusSnapshot snapshot;
    StatusSnapshotValue value;
    value.state = 3;
    value.audio_level = 0.42f;
    value.error_code = 5;
    value.error_counter = 7;
    value.error_timestamp = 123456;
    value.mode = 2;
    value.enabled = true;
    std::snprintf(value.error_message, sizeof(value.error_message), "boom");
    snapshot.Publish(value);

    const StatusSnapshotValue read = snapshot.Read();
    CHECK(read.state == 3);
    CHECK(read.audio_level == doctest::Approx(0.42f));
    CHECK(read.error_code == 5);
    CHECK(read.mode == 2);
    CHECK(read.enabled);
    CHECK(std::string(read.error_message) == "boom");
}
