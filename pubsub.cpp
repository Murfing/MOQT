#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>

// Data structure to hold instrument data
struct InstrumentData {
    uint64_t instrumentId;
    double lastTradedPrice;
    double extraData; // Bond yield or last day volume based on publisher type
};

// Abstract class for Publisher
class Publisher {
protected:
    std::unordered_map<uint64_t, InstrumentData> data_;
    std::unordered_map<uint64_t, std::unordered_set<uint64_t>> subscribers_;

public:
    virtual ~Publisher() = default;

    virtual void update_data(uint64_t instrumentId, double lastTradedPrice, double extraData) {
        data_[instrumentId] = {instrumentId, lastTradedPrice, extraData};
    }

    void subscribe(uint64_t subscriberId, uint64_t instrumentId) {
        subscribers_[instrumentId].insert(subscriberId);
    }

    virtual InstrumentData get_data(uint64_t subscriberId, uint64_t instrumentId) {
        if (subscribers_[instrumentId].find(subscriberId) == subscribers_[instrumentId].end()) {
            throw std::runtime_error("Subscriber not authorized for this instrument");
        }
        if (data_.find(instrumentId) == data_.end()) {
            throw std::runtime_error("Instrument data not available");
        }
        return data_[instrumentId];
    }
};

// EquityPublisher class
class EquityPublisher : public Publisher {
public:
    void update_data(uint64_t instrumentId, double lastTradedPrice, double lastDayVolume) override {
        if (instrumentId >= 1000) {
            throw std::invalid_argument("Invalid instrument ID for EquityPublisher");
        }
        Publisher::update_data(instrumentId, lastTradedPrice, lastDayVolume);
    }
};

// BondPublisher class
class BondPublisher : public Publisher {
public:
    void update_data(uint64_t instrumentId, double lastTradedPrice, double bondYield) override {
        if (instrumentId < 1000 || instrumentId >= 2000) {
            throw std::invalid_argument("Invalid instrument ID for BondPublisher");
        }
        Publisher::update_data(instrumentId, lastTradedPrice, bondYield);
    }
};

// Abstract class for Subscriber
class Subscriber {
protected:
    uint64_t id_;

public:
    explicit Subscriber(uint64_t id) : id_(id) {}
    virtual ~Subscriber() = default;

    virtual void subscribe(std::shared_ptr<Publisher> publisher, uint64_t instrumentId) {
        publisher->subscribe(id_, instrumentId);
    }

    virtual std::string get_data(std::shared_ptr<Publisher> publisher, uint64_t instrumentId) = 0;
};

// FreeSubscriber class
class FreeSubscriber : public Subscriber {
private:
    int requestCount_ = 0;
    static const int MAX_REQUESTS = 100;

public:
    explicit FreeSubscriber(uint64_t id) : Subscriber(id) {}

    std::string get_data(std::shared_ptr<Publisher> publisher, uint64_t instrumentId) override {
        if (requestCount_ >= MAX_REQUESTS) {
            return "F, " + std::to_string(id_) + ", " + std::to_string(instrumentId) + ", invalid_request";
        }

        try {
            auto data = publisher->get_data(id_, instrumentId);
            requestCount_++;
            return "F, " + std::to_string(id_) + ", " + std::to_string(instrumentId) + ", " +
                   std::to_string(data.lastTradedPrice) + ", " + std::to_string(data.extraData);
        } catch (const std::exception &e) {
            return "F, " + std::to_string(id_) + ", " + std::to_string(instrumentId) + ", invalid_request";
        }
    }
};

// PaidSubscriber class
class PaidSubscriber : public Subscriber {
public:
    explicit PaidSubscriber(uint64_t id) : Subscriber(id) {}

    std::string get_data(std::shared_ptr<Publisher> publisher, uint64_t instrumentId) override {
        try {
            auto data = publisher->get_data(id_, instrumentId);
            return "P, " + std::to_string(id_) + ", " + std::to_string(instrumentId) + ", " +
                   std::to_string(data.lastTradedPrice) + ", " + std::to_string(data.extraData);
        } catch (const std::exception &e) {
            return "P, " + std::to_string(id_) + ", " + std::to_string(instrumentId) + ", invalid_request";
        }
    }
};

int main() {
    // Example usage
    auto equityPublisher = std::make_shared<EquityPublisher>();
    auto bondPublisher = std::make_shared<BondPublisher>();

    auto freeSubscriber = std::make_shared<FreeSubscriber>(1);
    auto paidSubscriber = std::make_shared<PaidSubscriber>(2);

    // Updating data
    equityPublisher->update_data(500, 150.5, 1000);
    bondPublisher->update_data(1500, 98.7, 3.5);

    // Subscribing
    freeSubscriber->subscribe(equityPublisher, 500);
    paidSubscriber->subscribe(bondPublisher, 1500);

    // Getting data
    std::cout << freeSubscriber->get_data(equityPublisher, 500) << std::endl;
    std::cout << paidSubscriber->get_data(bondPublisher, 1500) << std::endl;
    std::cout << freeSubscriber->get_data(bondPublisher, 1500) << std::endl; // Invalid request

    return 0;
}
