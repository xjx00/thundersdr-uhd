#pragma once

#include <uhd/device.hpp>
#include <uhd/property_tree.hpp>
#include <uhd/stream.hpp>
#include <uhd/types/device_addr.hpp>

namespace uhd { namespace usrp { namespace vx300 {

class vx300_impl : public uhd::device
{
public:
    using sptr = std::shared_ptr<vx300_impl>;

    explicit vx300_impl(const uhd::device_addr_t& args);
    ~vx300_impl() override = default;

    uhd::rx_streamer::sptr get_rx_stream(const uhd::stream_args_t& args) override;
    uhd::tx_streamer::sptr get_tx_stream(const uhd::stream_args_t& args) override;

    bool recv_async_msg(
        uhd::async_metadata_t& async_metadata,
        double timeout = 0.1) override;

private:
    uhd::device_addr_t _args;

    void setup_tree();
    void setup_mboard_tree();
    void setup_rx_tree();
    void setup_tx_tree();
};

}}} // namespace uhd::usrp::vx300